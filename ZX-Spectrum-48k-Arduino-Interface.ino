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
const byte ISRDataPin = 9;    // connected to 74HC165 QH (pin-9 on chip)
const byte ISRLatchPin = 10;  // connected to 74HC165 SH/LD (pin-1 on chip)
//                   pin 11 MOSI - SD CARD
//                   pin 12 MISO - SD CARD
//                   pin 13 SCK  - SD CARD
const byte ledPin = 13;  // only used for critical errors (flashes)
const byte Z80_NMI = 14;
const byte ROM_HALF = 15;
const byte ISRClockPin = 16;  // connected to 74HC165 CLK (pin-2 on chip)
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
int totalFiles;

#define I2C_ADDRESS 0x3C  // 0x3C or 0x3D
SSD1306AsciiAvrI2c oled;

void debugFlash(int flashSpeed);
inline void swap(byte &a, byte &b);
uint8_t reverseBits(uint8_t byte);
void setupOled();

void setup() {
  /* 
  Serial.begin(9600);
  while (! Serial);
  while (1) {  
    byte pinState = bitRead(PINB, PINB1);
 //   byte val = digitalRead(Z80_A12);   
    Serial.println(pinState); 
  }; 
*/

  // Reset Z80
  pinMode(Z80_REST, OUTPUT);
  bitClear(PORTC, DDC3);  // reset-line "LOW" speccy
  delay(25);
  bitSet(PORTC, DDC3);  // reset-line "HIGH" allow speccy to startup

  // Setup pins for "74HC165" shift register
  pinMode(ISRDataPin, INPUT);
  pinMode(ISRLatchPin, OUTPUT);
  pinMode(ISRClockPin, OUTPUT);

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

  // Initialize SD card
  if (!sd.begin()) {
    // TODO : ERROR HERE
  }

  if (!root.open("/")) {
    debugFlash(200);
  }

  currentIndex = 0;

  root.rewind();
  totalFiles = 0;
  while (file.openNext(&root, O_RDONLY)) {
    if (file.isFile()) {
      if (file.fileSize() == 49179) {
        totalFiles++;
      }
    }
    file.close();
  }

  if (!openFileByIndex(currentIndex)) {
    // TODO : ERROR HERE
  }


}


void loop() {
  
  char fileName[16];
  file.getName7(fileName, 16);
  oled.clear();
  oled.print(F("Zx Spectrum interface\n"));
  oled.print(F("takes <*.sna> files\nLOADING: "));
  oled.print(fileName);
  oled.print(F("\nver"));
  oled.println(F(VERSION));


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

  file.close();

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

  // currentIndex++;
  // if (currentIndex > 13) {
  //  currentIndex = 0;
  //  }
  /*
  delay(3000);
  bitClear(PORTC, DDC3);  // reset-line "LOW" speccy
  bitClear(PORTC, DDC1);  // pin15 (A1) - Switch to low part of the ROM.
  delay(25);
  bitSet(PORTC, DDC3);  // reset-line "HIGH" allow speccy to startup
*/


  unsigned long lastButtonPress = 0;  // Store the last time a button press was processed
  unsigned long buttonDelay = 300;    // Initial delay between button actions in milliseconds
  unsigned long lastButtonHoldTime = 0; // Track how long the button has been held
  bool buttonHeld = false;  // Track if the button is being held

  while (1) {
    processJoystick();

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
          bitClear(PORTC, DDC3);  // reset-line "LOW" speccy
          bitClear(PORTC, DDC1);  // pin15 (A1) - Switch to low part of the ROM.
          delay(25);
          bitSet(PORTC, DDC3);  // reset-line "HIGH" allow speccy to startup
          break;
        } else if (but < (100 + 510)) {  // Third button action
          currentIndex--;
          if (currentIndex < 0) {
            currentIndex = totalFiles-1;
          }
          updateFileName();
        }

        // Update timing for the next press
        lastButtonPress = currentMillis;
        buttonHeld = true;  // Mark that the button is held
        lastButtonHoldTime = currentMillis;  // Reset hold timing
      }
    } else {
      // If no button is pressed, reset the button-held status and delay
      buttonHeld = false;
      buttonDelay = 300;  // Reset to the initial delay
    }
  }





/*
   unsigned long currentMillis = millis();  // Get the current time

    if (currentMillis - lastButtonPress >= buttonDelay) {  // Check if enough time has passed since the last action
 
      int but = analogRead(21);

      oled.setCursor(0, 0);
      if (but < (100 + 22)) {
        currentIndex++;
        if (currentIndex > 13) {
          currentIndex = 0;
        }
        updateFileName();
        lastButtonPress = currentMillis;  
      } else if (but < (100 + 324)) {
        bitClear(PORTC, DDC3);  // reset-line "LOW" speccy
        bitClear(PORTC, DDC1);  // pin15 (A1) - Switch to low part of the ROM.
        delay(25);
        bitSet(PORTC, DDC3);  // reset-line "HIGH" allow speccy to startup
        break;
      } else if (but < (100 + 510)) {
        currentIndex--;
        if (currentIndex < 0) {
          currentIndex = 13;
        }
        updateFileName();
        lastButtonPress = currentMillis;  
      } 
    }
  
  }  */

  //debugFlash(200);  // All ok - Flash led to show we have reached the end with no left over halts
}

void updateFileName() {
  file.close();
  if (openFileByIndex(currentIndex)) {
    char fileName[16];
    file.getName7(fileName, 16);
    oled.setCursor(0, 0);
//    oled.clear();
//    oled.print(F("Zx Spectrum interface\n"));
//    oled.print(F("takes <*.sna> files\nFILE: "));
    oled.print(F("FILE: "));
    oled.print(fileName);
    oled.print(".sna      ");    
  } else {
    // TODO : ERROR HERE
  }
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

void debugFlash(int flashspeed) {

  file.close();
  root.close();
  sd.end();
  while (1) {
    /*
    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, HIGH);
    delay(flashspeed);
    digitalWrite(ledPin, LOW);
    delay(flashspeed);
*/

    uint8_t data = 0;
    digitalWrite(ISRClockPin, HIGH);                    // preset clock to retrieve first bit
    digitalWrite(ISRLatchPin, HIGH);                    // disable input latching and enable shifting
    data = shiftIn(ISRDataPin, ISRClockPin, MSBFIRST);  // capture input values
    digitalWrite(ISRLatchPin, LOW);                     // disable shifting and enable input latching

    PORTD = reverseBits(data);

    if (data) {
      digitalWrite(ledPin, HIGH);
    } else {
      digitalWrite(ledPin, LOW);
    }
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
   oled.print(F("Zx Spectrum interface\ntakes <*.sna> files\n\nver"));
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

  digitalWrite(ISRClockPin, HIGH);                         // preset clock to retrieve first bit
  digitalWrite(ISRLatchPin, HIGH);                         // disable input latching and enable shifting
  byte data = shiftIn(ISRDataPin, ISRClockPin, MSBFIRST);  // capture input values
  digitalWrite(ISRLatchPin, LOW);                          // disable shifting and enable input latching
  PORTD = reverseBits(data);
  if (data) {
    digitalWrite(ledPin, HIGH);
  } else {
    digitalWrite(ledPin, LOW);
  }
  /*


  bitSet(PORTB, 0);  // HIGH, pin 16 (PB0), clock to retrieve first bit
  bitSet(PORTB, 2);  // HIGH, pin 10 (PB2), disable input latching/enable shifting

  // Read the data byte using shiftIn replacement with direct port manipulation
  byte data = 0;
  for (int i = 0; i < 8; i++) {
    data <<= 1;
    if (bitRead(PINB, 1)) {  // Reads data from pin 9 (PB1)
      bitSet(data, 0);
    }
    // Toggle clock pin low to high to read the next bit
    bitClear(PORTB, 0);    // Clock low
    delayMicroseconds(1);  // Short delay to ensure stable clocking
    bitSet(PORTB, 0);      // Clock high
  }

  bitClear(PORTB, 2);  //LOW, pin 10 (PB2),  Disable shifting (latch)

  data = reverseBits(data);  // H/W error in prototype!
  PORTD = data;  // output to z80 data lines

  if (data) {
    bitSet(PORTB, 5);  // LED, pin 13 (PB5)
  } else {
    bitClear(PORTB, 5);  // LED, pin 13 (PB5)
  }
  */
}


/*
  if (!sd.exists("status.txt")) {
    statusFile.open("status.txt", O_RDWR);
    if (statusFile) {
      statusFile.seekSet(0);
      statusFile.truncate(0);
      statusFile.seekSet(0);
      statusFile.print(0);
      statusFile.close();
    }
  }

  statusFile.open("status.txt", O_RDWR);
  if (statusFile) {
    statusFile.seekSet(0);
    currentIndex = statusFile.parseInt();
    statusFile.close();
  }
*/


/*  statusFile.open("status.txt", O_RDWR);
  if (statusFile) {
    statusFile.seekSet(0);
    currentIndex = statusFile.parseInt();
    statusFile.seekSet(0);
    statusFile.truncate(0);
    currentIndex++;
    if (currentIndex > 13) {
      currentIndex = 0;
    }
    statusFile.print(currentIndex);
    statusFile.close();
  }
  */
