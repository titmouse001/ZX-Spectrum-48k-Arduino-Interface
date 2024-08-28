// -------------------------------------------------------------------------------------
//  "Arduino Hardware Interface for ZX Spectrum" - 2023/24 P.Overy
// -------------------------------------------------------------------------------------
// This software uses the Arduino's ATmega328P range (Nano, Pro mini ... )
// for example here's the Nano specs:-
// - 32K bytes of in-system self-programmable flash program memory
// - 1K bytes EEPROM
// - 2K bytes internal SRAM
// IMPORTANT: Do not modify PORTB directly without preserving the clock/crystal bits.
// -------------------------------------------------------------------------------------
// SPEC OF SPECCY H/W PORTS HERE: - https://mdfs.net/Docs/Comp/Spectrum/SpecIO
// good links for Arduino info  :   https://devboards.info/boards/arduino-nano
//                                  https://arduino.stackexchange.com/questions/30968/how-do-interrupts-work-on-the-arduino-uno-and-similar-boards
// SNAPSHOT FILE HEADER (.sna files)
//  REG_I	 =00, REG_HL'=01, REG_DE'	=03, REG_BC' =05	(.sna file header)
//  REG_AF'=07, REG_HL =09, REG_DE	=11, REG_BC	 =13  (        27 bytes)
//  REG_IY =15, REG_IX =17, REG_IFF =19, REG_R	 =20
//  REG_AF =21, REG_SP =23, REG_IM	=25, REG_BDR =26
// -------------------------------------------------------------------------------------

#include <avr/pgmspace.h>
#include <SPI.h>
#include "SdFat.h"  // "SdFatConfig.h" options, I'm using "USE_LONG_FILE_NAMES 1"
#include "SSD1306AsciiAvrI2c.h"
#include "fudgefont.h"  // Based on the Adafruit5x7 font, with '!' to '(' changed to work as a VU BAR (8 chars)

#define VERSION ("0.3")

// 'Z80_D0Pin' to 'Z80_D7Pin'
const byte Z80_D0Pin = 0;     // Arduino to z80 data pins
const byte Z80_D1Pin = 1;     //  ""
const byte Z80_D2Pin = 2;     //  ""
const byte Z80_D3Pin = 3;     //  ""
const byte Z80_D4Pin = 4;     //  ""
const byte Z80_D5Pin = 5;     //  ""
const byte Z80_D6Pin = 6;     //  ""
const byte Z80_D7Pin = 7;     //  ""
const byte Z80_HALT = 8;      // PINB0 (PORT B), Z80 'Halt' Status
const byte ShiftRegDataPin = 9;    // connected to 74HC165 QH (pin-9 on chip)
const byte ShiftRegLatchPin = 10;  // connected to 74HC165 SH/LD (pin-1 on chip)
//                   pin 11 MOSI - SD CARD
//                   pin 12 MISO - SD CARD
//                   pin 13 SCK  - SD CARD
const byte ledPin = 13;  // only used for critical errors (flashes)
const byte Z80_NMI = 14;
const byte ROM_HALF = 15;
const byte ShiftRegClockPin = 16;  // connected to 74HC165 CLK (pin-2 on 165)
const byte Z80_REST = 17;
//                   pin 18 SDA - OLED
//                   pin 19 SCL - OLED

// Transfer structure
const byte HEADER_START_G = 0;      // Index for first byte of header
const byte HEADER_START_O = 1;      // Index for second byte of header
const byte HEADER_PAYLOADSIZE = 2;  // Index for payload as bytes
const byte HEADER_HIGH_BYTE = 3;    // Index for high byte of address
const byte HEADER_LOW_BYTE = 4;     // Index for low byte of address
const byte SIZE_OF_HEADER = HEADER_LOW_BYTE + 1;
const byte PAYLOAD_BUFFER_SIZE = 250;
const byte BUFFER_SIZE = SIZE_OF_HEADER + PAYLOAD_BUFFER_SIZE;

static byte buffer[BUFFER_SIZE] = { 'G', 'O' };  // Load program "GO"

static byte head27_2[27 + 2] = { 'E', 'X' };  // Execute command "EX"

SdFat32 sd;
FatFile root;
FatFile file;
File statusFile;
int currentIndex;
int lastIndex;
int totalFiles;

#define I2C_ADDRESS 0x3C  // 0x3C or 0x3D
SSD1306AsciiAvrI2c oled;

void debugFlash(int flashSpeed);
inline void swap(byte &a, byte &b);
uint8_t reverseBits(uint8_t byte);
void setupOled();



  unsigned long lastButtonPress = 0;  // Store the last time a button press was processed
  unsigned long buttonDelay = 300;    // Initial delay between button actions in milliseconds
  unsigned long lastButtonHoldTime = 0; // Track how long the button has been held
  bool buttonHeld = false;  // Track if the button is being held

// Transposed data storage
//uint8_t transposed[8] = {0};

static const int _FONT_WIDTH = 5;
static const int _FONT_HEIGHT = 7;
static const int _FONT_GAP = 1;
static const int _FONT_BUFFER_SIZE = 32;

byte finalOutput[_FONT_BUFFER_SIZE * _FONT_HEIGHT] = { 0 };
int bitPosition[_FONT_HEIGHT] = { 0 };

  char fileName[16];

uint16_t zx_spectrum_screen_address(uint8_t x, uint8_t y) {
    // Base screen address in ZX Spectrum
    uint16_t base_address = 0x4000;

    // Calculate section offset based on the Y coordinate
    uint16_t section_offset;
    if (y < 64) {
        section_offset = 0;  // First section
    } else if (y < 128) {
        section_offset = 0x0800;  // Second section
    } else {
        section_offset = 0x1000;  // Third section
    }

    // Calculate the correct interleaved line address
    uint8_t block_in_section = (y & 0b00111000) >> 3;  // Extract bits 3-5 (block number)
    uint8_t line_in_block = y & 0b00000111;            // Extract bits 0-2 (line within block)
    uint16_t row_within_section = (line_in_block * 256) + (block_in_section * 32);

    // Calculate the horizontal byte index (each byte represents 8 pixels)
    uint8_t x_byte_index = x >> 3;

    // Calculate and return the final screen address
    return base_address + section_offset + row_within_section + x_byte_index;
}

void joinBits(uint8_t input, byte bitWidth, byte k) {
		int bitPos = bitPosition[k];     
		int byteIndex = bitPos / 8;         
		int bitIndex = bitPos % 8;          
		// Using a WORD to allow for a boundary crossing
		uint16_t alignedInput = (uint16_t)input << (8 - bitWidth);
		finalOutput[byteIndex] |= alignedInput >> bitIndex;
		if (bitIndex + bitWidth > 8) {  // spans across two bytes
			finalOutput[byteIndex + 1] |= alignedInput << (8 - bitIndex);
		}
		bitPosition[k] += bitWidth;
	}

void setup() {

  // Serial.begin(9600);
  // while (! Serial);

  // Serial.println("STARTING");
  //  while (1) {
  //  byte pinState = bitRead(PINB, PINB1);
  //   byte val = digitalRead(Z80_A12);
  //  Serial.println(pinState);
  // };


  // Reset Z80
  pinMode(Z80_REST, OUTPUT);
  bitClear(PORTC, DDC3);  // reset-line "LOW" speccy
  delay(25);
  bitSet(PORTC, DDC3);  // reset-line "HIGH" allow speccy to startup

  // Setup pins for "74HC165" shift register
  pinMode(ShiftRegDataPin, INPUT);
  pinMode(ShiftRegLatchPin, OUTPUT);
  pinMode(ShiftRegClockPin, OUTPUT);

  // 16k Rom switching (by sellecting which rom half from a 32k Rom)
  bitClear(PORTC, DDC1);
  pinMode(ROM_HALF, OUTPUT);  // LOW external/HIGH stock rom
  bitClear(PORTC, DDC1);      // pin15, A1 , swap out stock rom

  // Connect to Z80's NMI Line to unhalt Z80 (using NMI due to speccys maskable taken for 50fps H/W refresh)
  pinMode(Z80_NMI, OUTPUT);  // Connetcs to the Z80 /NMI which releases the z80's 'halt' state
  bitSet(PORTC, DDC0);       // pin14 (A0), Z80 /NMI line

  // These pins are connected to the address lines on the Z80
  pinMode(Z80_D0Pin, OUTPUT);
  pinMode(Z80_D1Pin, OUTPUT);
  pinMode(Z80_D2Pin, OUTPUT);
  pinMode(Z80_D3Pin, OUTPUT);
  pinMode(Z80_D4Pin, OUTPUT);
  pinMode(Z80_D5Pin, OUTPUT);
  pinMode(Z80_D6Pin, OUTPUT);
  pinMode(Z80_D7Pin, OUTPUT);
  pinMode(Z80_HALT, INPUT);

  setupOled();  // Using a mono OLED with 128x32 pixels

  //-------------


  uint16_t currentAddress = 0x5800;
  //768
  buffer[HEADER_PAYLOADSIZE] = 250;
  buffer[HEADER_HIGH_BYTE] = (currentAddress >> 8) & 0xFF;  // high byte
  buffer[HEADER_LOW_BYTE] = currentAddress & 0xFF;          // low byte
  for (int i = 0; i < 250; i++) {
    buffer[SIZE_OF_HEADER + i] = 7;
  }
  sendBytes(buffer, SIZE_OF_HEADER + 250);

  currentAddress += 250;
  buffer[HEADER_PAYLOADSIZE] = 250;
  buffer[HEADER_HIGH_BYTE] = (currentAddress >> 8) & 0xFF;  // high byte
  buffer[HEADER_LOW_BYTE] = currentAddress & 0xFF;          // low byte
  for (int i = 0; i < 250; i++) {
    buffer[SIZE_OF_HEADER + i] = 7;
  }
  sendBytes(buffer, SIZE_OF_HEADER + 250);

  currentAddress += 250;
  buffer[HEADER_PAYLOADSIZE] = 250;
  buffer[HEADER_HIGH_BYTE] = (currentAddress >> 8) & 0xFF;  // high byte
  buffer[HEADER_LOW_BYTE] = currentAddress & 0xFF;          // low byte
  for (int i = 0; i < 250; i++) {
    buffer[SIZE_OF_HEADER + i] = 7;
  }
  sendBytes(buffer, SIZE_OF_HEADER + 250);




  // Initialize SD card
  if (!sd.begin()) {
    // TODO : ERROR HERE
  }

  if (!root.open("/")) {
    debugFlash(200);
  }


  root.rewind();
  totalFiles = 0;
  while (file.openNext(&root, O_RDONLY)) {
    if (file.isFile()) {
      if (file.fileSize() == 49179) {
    
        file.getName7(fileName, 16);
        //---------------

        for (int i = 0; i < _FONT_HEIGHT; i++) { bitPosition[i] = (_FONT_BUFFER_SIZE * i) * 8;  }
        memset(&buffer[2], 0, 255 - SIZE_OF_HEADER);
        memset(&finalOutput[0], 0, _FONT_BUFFER_SIZE * _FONT_HEIGHT);

        for (int i = 0; fileName[i] != '\0'; i++) {
          byte* ptr = &fudged_Adafruit5x7[((fileName[i] - 0x20) * 5) + 0 + 6];

          for (int row = 0; row < _FONT_HEIGHT; row++) {
            uint8_t transposedRow = 0;
            for (int col = 0; col < _FONT_WIDTH; col++) {
              byte value = pgm_read_byte(&ptr[col]);
              transposedRow |= ((value >> row) & 0x01) << (_FONT_WIDTH - 1 - col);
            }
            joinBits(transposedRow, _FONT_WIDTH + _FONT_GAP, row);
          }
        }

        int amount = ((bitPosition[0] + 7) / 8);
        for (int y = 0; y < _FONT_HEIGHT; y++) {
          uint8_t* ptr = &finalOutput[y * _FONT_BUFFER_SIZE];
          uint16_t currentAddress = zx_spectrum_screen_address(8, y + ((totalFiles) * 8));
          buffer[HEADER_PAYLOADSIZE] = amount;
          buffer[HEADER_HIGH_BYTE] = (currentAddress >> 8) & 0xFF;
          buffer[HEADER_LOW_BYTE] = currentAddress & 0xFF;
          for (int i = 0; i < amount; i++) {
            buffer[SIZE_OF_HEADER + i] = ptr[i];
          }
          sendBytes(buffer, SIZE_OF_HEADER + amount);
        }

        totalFiles++;
        //---------------
      }
    }
    file.close();
  }


  currentIndex = 0;
  lastIndex = -1;
}

void loop() {
  
  oled.clear();
  while (1) {
    if (selectGame() ) {
      break;
    }else {


      for (int k=0; k<totalFiles; k++) {


         if (currentIndex==k)  {
          fileName[0]='>';
         }else {
          fileName[0]=' ';
         }
          fileName[1] = '\0';
        //---------------

        for (int i = 0; i < _FONT_HEIGHT; i++) { bitPosition[i] = (_FONT_BUFFER_SIZE * i) * 8;  }
        memset(&buffer[2], 0, 255 - SIZE_OF_HEADER);
        memset(&finalOutput[0], 0, _FONT_BUFFER_SIZE * _FONT_HEIGHT);

        for (int i = 0; fileName[i] != '\0'; i++) {
          byte* ptr = &fudged_Adafruit5x7[((fileName[i] - 0x20) * 5) + 0 + 6];

          for (int row = 0; row < _FONT_HEIGHT; row++) {
            uint8_t transposedRow = 0;
            for (int col = 0; col < _FONT_WIDTH; col++) {
              byte value = pgm_read_byte(&ptr[col]);
              transposedRow |= ((value >> row) & 0x01) << (_FONT_WIDTH - 1 - col);
            }
            joinBits(transposedRow, _FONT_WIDTH /*+ _FONT_GAP*/, row);
          }
        }

        int amount = ((bitPosition[0] + 7) / 8);
        for (int y = 0; y < _FONT_HEIGHT; y++) {
          uint8_t* ptr = &finalOutput[y * _FONT_BUFFER_SIZE];
          uint16_t currentAddress = zx_spectrum_screen_address(0, y + ((k) * 8));
          buffer[HEADER_PAYLOADSIZE] = amount;
          buffer[HEADER_HIGH_BYTE] = (currentAddress >> 8) & 0xFF;
          buffer[HEADER_LOW_BYTE] = currentAddress & 0xFF;
          for (int i = 0; i < amount; i++) {
            buffer[SIZE_OF_HEADER + i] = ptr[i];
          }
          sendBytes(buffer, SIZE_OF_HEADER + amount);
        }
      }
    }
  }


  //char fileName[16];
  file.getName7(fileName, 16);
  oled.clear();
  oled.print(F("Loading:\n"));
  oled.println(fileName);

  // Read the Snapshots 27-byte header
  if (file.available()) {
    byte bytesReadHeader = (byte)file.read(&head27_2[0 + 2], 27);
    if (bytesReadHeader != 27) { debugFlash(3000); }
  }

  uint16_t currentAddress = 0x4000;  //starts at beginning of screen
  while (file.available()) {
    byte bytesRead = (byte)file.read(&buffer[SIZE_OF_HEADER], PAYLOAD_BUFFER_SIZE);
    buffer[HEADER_PAYLOADSIZE] = bytesRead;
    buffer[HEADER_HIGH_BYTE] = (currentAddress >> 8) & 0xFF;  // high byte
    buffer[HEADER_LOW_BYTE] = currentAddress & 0xFF;          // low byte
    sendBytes(buffer, SIZE_OF_HEADER + bytesRead);
    currentAddress += buffer[HEADER_PAYLOADSIZE];
  }

 //file.close();

  // *** Send Snapshot Header Section ***
  sendBytes(head27_2, sizeof(head27_2));  // Send entire header

  // Resend DE,BC & AF in that order - Z80 ignored them in the previous send
  sendBytes(&head27_2[2 + 11], 2);  // Send DE
  sendBytes(&head27_2[2 + 13], 2);  // Send BC
  sendBytes(&head27_2[2 + 21], 2);  // Send AF

  // Wait for the Z80 to halt. The Spectrum's maskable interrupt will release it during the 50FPS screen refresh.
  waitHalt();  // 1st halt - trigger by maskable interrupt

  // At this point the Spectrum is resorting running code in screen memory to safely swap ROM banks.
  // The Spectrum does a 2nd halt so we can tell it's started executing the final launch code.
  waitHalt();  // 2nd halt - trigger by maskable interrupt

  delayMicroseconds(7);  // TODO ..... !!!!!!!!!!!!! WHY !!!!!!!!!!!!!

  bitSet(PORTC, DDC1);  // pin15 (A1) - Switch to high part of the ROM.

  // At ths point rhe Spectrum's external ROM has switched to using the 16K stock ROM.

  while (bitRead(PINB, PINB0) == LOW) {}  // DEBUG, blocking here if Z80's -HALT still in action

  oled.clear();
  oled.print("running:\n");
  oled.println(fileName);

  while(1) {
    processJoystick();
    if (analogRead(21) < (100 + 510)) {
      break;
    }
  }

  //debugFlash(200);  // All ok - Flash led to show we have reached the end with no left over halts
}

void sendBytes(byte *data, byte size) {
  for (byte bufferIndex = 0; bufferIndex < size; bufferIndex++) {
    while ((bitRead(PINB, PINB0) == HIGH)) {};
    PORTD = reverseBits(data[bufferIndex]);
    bitClear(PORTC, DDC0);
    bitSet(PORTC, DDC0);
    while ((bitRead(PINB, PINB0) == LOW)) {};
  }
}

void waitHalt() {
  // Here we are waiting for the speccy to release the hault, it does this every 50fps.
  while ((bitRead(PINB, PINB0) == HIGH)) {};
  while ((bitRead(PINB, PINB0) == LOW)) {};
}

void waitReleaseHalt() {
  // Here we MUST release the z80 for it's HALT state.
  while ((bitRead(PINB, PINB0) == HIGH)) {};  // (1) Wait for Halt line to go LOW.
  bitClear(PORTC, DDC0);                      // (2) A0 (pin-8) signals the Z80 /NMI line,
  bitSet(PORTC, DDC0);                        //     LOW then HIGH, this will un-halt the Z80.
  while ((bitRead(PINB, PINB0) == LOW)) {};   // (3) Wait for halt line to go HIGH again.
}

inline void swap(byte &a, byte &b) {
  byte temp = a;
  a = b;
  b = temp;
}

void updateFileName() {
  file.close();
  if (openFileByIndex(currentIndex)) {
   // char fileName[16];
    file.getName7(fileName, 16);
    oled.setCursor(0, 0);
    oled.print(F("Select File:\n"));
    oled.print(fileName);
    oled.print("      ");    
  } else {
    // TODO : ERROR HERE
  }
}

void setupOled() {
  oled.begin(&Adafruit128x32, I2C_ADDRESS);
  delay(1);
  // some hardware is slow to initialise, first call does not work.
  oled.begin(&Adafruit128x32, I2C_ADDRESS);
  // original Adafruit5x7 font with tweeks at start for VU meter
  oled.setFont(fudged_Adafruit5x7);
  oled.clear();
   oled.print(F("ver"));
   oled.println(F(VERSION));
}

inline byte reverseBits(byte data) {
  data = (data & 0xF0) >> 4 | (data & 0x0F) << 4;
  data = (data & 0xCC) >> 2 | (data & 0x33) << 2;
  data = (data & 0xAA) >> 1 | (data & 0x55) << 1;
  return data;
}

bool openFileByIndex(int searchIndex) {
  root.rewind();
  int index = 0;
  while (file.openNext(&root, O_RDONLY)) {
    if (file.isFile()) {
      if (file.fileSize() == 49179) {
        if (index == searchIndex) {
          return true;
        }
        index++;
      }
    }
    file.close();
  }
  return false;
}

void processJoystick() {
 
  bitSet(PORTB, DDB2);    // HIGH, pin 10, disable input latching/enable shifting
  bitSet(PORTC, DDC2); // HIGH, pin 16, clock to retrieve first bit
  
  byte data = 0;
  // Read the data byte using shiftIn replacement with direct port manipulation
  for (int i = 0; i < 8; i++) {
    data <<= 1;
    if (bitRead(PINB, 1)) {  // Reads data from pin 9 (PB1)
      bitSet(data, 0);
    }
    // Toggle clock pin low to high to read the next bit 
    bitClear(PORTC, DDC2); // Clock low
    delayMicroseconds(1);  // Short delay to ensure stable clocking
    bitSet(PORTC, DDC2);  // Clock high
  }
  bitClear(PORTB, DDB2);  //LOW, pin 10 (PB2),  Disable shifting (latch)

  data = reverseBits(data);  // H/W error in prototype!
  PORTD = data;  // output to z80 data lines
}

bool selectGame() {

    // ANALOGUE VALUES ARE AROUND... 1024,510,324,22

    int but = analogRead(21);
    unsigned long currentMillis = millis();  // Get the current time

    // Determine if a button is pressed and which one
    bool buttonPressed = false;
    if (but < (100 + 22)) {
      buttonPressed = true;
    } else if (but < (100 + 324)) {
      buttonPressed = true;
    } else if (but < (100 + 510)) {
      buttonPressed = true;
    }

    if (buttonPressed) {
      if (buttonHeld && (currentMillis - lastButtonHoldTime >= buttonDelay)) {
        // Minimum delay of 50ms, decrease by 20ms each time
        buttonDelay = max(50, buttonDelay - 20);  
        lastButtonHoldTime = currentMillis;
      }

      if (currentMillis - lastButtonPress >= buttonDelay) {     
        if (but < (100 + 22)) {  // First button action
          currentIndex++;
          if (currentIndex >= totalFiles) {
            currentIndex = 0;
          }
          updateFileName();
        } else if (but < (100 + 324)) {  // Second button action
            updateFileName();

            bitClear(PORTC, DDC3);  // reset-line "LOW" speccy
            bitClear(PORTC, DDC1);  // pin15 (A1) - Switch to low part of the ROM.
            delay(10);
            bitSet(PORTC, DDC3);  // reset-line "HIGH" allow speccy to startup
            return true;
        } else if (but < (100 + 510)) {  // Third button action
          currentIndex--;
          if (currentIndex < 0) {
            currentIndex = totalFiles-1;
          }
          updateFileName();
        }

        buttonHeld = true; 
        lastButtonPress = currentMillis; 
        lastButtonHoldTime = currentMillis; 
      }
    } else {
      // If no button is pressed, reset the button-held status and delay
      buttonHeld = false;
      buttonDelay = 300;  // Reset to the initial delay
    }

    return false;
}

void debugFlash(int flashspeed) {
  // If we get here it's faital
  file.close();
  root.close();
  sd.end();
  while (1) {
    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, HIGH);
    delay(flashspeed);
    digitalWrite(ledPin, LOW);
    delay(flashspeed);
  }
}
