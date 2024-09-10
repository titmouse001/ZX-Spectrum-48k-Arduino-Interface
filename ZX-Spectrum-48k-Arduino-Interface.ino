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
#include "utils.h"

#include <Wire.h> 

#define VERSION ("0.14")

// 'Z80_D0Pin' to 'Z80_D7Pin'
const byte Z80_D0Pin = 0;          // Arduino to z80 data pins
const byte Z80_D1Pin = 1;          //  ""
const byte Z80_D2Pin = 2;          //  ""
const byte Z80_D3Pin = 3;          //  ""
const byte Z80_D4Pin = 4;          //  ""
const byte Z80_D5Pin = 5;          //  ""
const byte Z80_D6Pin = 6;          //  ""
const byte Z80_D7Pin = 7;          //  ""
const byte Z80_HALT = 8;           // PINB0 (PORT B), Z80 'Halt' Status
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
//const byte HEADER_START_O = 1;      // Index for second byte of header
const byte HEADER_PAYLOADSIZE = 1;  // Index for payload as bytes
const byte HEADER_HIGH_BYTE = 2;    // Index for high byte of address
const byte HEADER_LOW_BYTE = 3;     // Index for low byte of address
const byte SIZE_OF_HEADER = HEADER_LOW_BYTE + 1;
const byte PAYLOAD_BUFFER_SIZE = 251;
const byte BUFFER_SIZE = SIZE_OF_HEADER + PAYLOAD_BUFFER_SIZE;

static byte packetBuffer[BUFFER_SIZE] ; //= { 'G', 'O' };  // Load program "GO"

//static byte head27_2[27 + 2]; // = { 'E', 'X' };  // Execute command "EX"
static byte head27_2[27 + 1] = { 'E' };  // Execute command "EX"

SdFat32 sd;
FatFile root;
FatFile file;
File statusFile;

uint16_t totalFiles;

#define I2C_ADDRESS 0x3C  // 0x3C or 0x3D
SSD1306AsciiAvrI2c oled;

unsigned long lastButtonPress = 0;     // Store the last time a button press was processed
unsigned long buttonDelay = 300;       // Initial delay between button actions in milliseconds
unsigned long lastButtonHoldTime = 0;  // Track how long the button has been held
bool buttonHeld = false;               // Track if the button is being held

static const uint8_t _FONT_WIDTH = 5;
static const uint8_t _FONT_HEIGHT = 7;
static const uint8_t _FONT_GAP = 1;
static const uint8_t _FONT_BUFFER_SIZE = 32;

byte finalOutput[_FONT_BUFFER_SIZE * _FONT_HEIGHT] = { 0 };
char fileName[65];

#define WINDOW_SIZE 24  // Number of items visible at a time
uint16_t oldHighlightAddress= 0; 
uint16_t currentIndex = 0;  // The currently selected index in the list
uint16_t startIndex = 0;    // The start index of the current viewing window

enum { BUTTON_NONE, BUTTON_DOWN, BUTTON_SELECT, BUTTON_UP };

bool haveOled = false;

void setup() {

  //  Serial.begin(9600);
  //  while (! Serial) {};
  //  Serial.println("STARTING");


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

  haveOled = setupOled();  // Using a mono OLED with 128x32 pixels

  int but = getAnalogButton(analogRead(21));
  if (but == BUTTON_SELECT) {
    bitSet(PORTC, DDC1);  // pin15 (A1) - Switch to high part of the ROM.
  }

  if (but == BUTTON_SELECT) {
    bitSet(PORTC, DDC1);  // pin15 (A1) - Switch to high part of the ROM.
  }

  if (but == BUTTON_DOWN) {
    InitializeSDcard();

    // TEST SECTION ... loads .SCR files as a slide show
    while (1) {
      root.rewind();
      while (file.openNext(&root, O_RDONLY)) {
        if (file.isFile()) {
          if (file.fileSize() == 6912) {
            uint16_t currentAddress = 0x4000;  // screen start
            while (file.available()) {
              byte bytesRead = (byte)file.read(&packetBuffer[SIZE_OF_HEADER], PAYLOAD_BUFFER_SIZE);
              packetBuffer[0] = 'C';
              //   packetBuffer[1] = 'P';
              packetBuffer[HEADER_PAYLOADSIZE] = bytesRead;
              packetBuffer[HEADER_HIGH_BYTE] = (currentAddress >> 8) & 0xFF;  // high byte
              packetBuffer[HEADER_LOW_BYTE] = currentAddress & 0xFF;          // low byte
              sendBytes(packetBuffer, SIZE_OF_HEADER + bytesRead);
              currentAddress += bytesRead;
            }
            delay(2000);
          }
        }
        file.close();
      }
    }
  }
}

void loop() {

  clearScreenAttributes();

  InitializeSDcard();
  refreshFileList();
  HighLightFile();

  uint16_t oldIndex = -1;
  const unsigned long maxButtonInputLoopTime = 1000 / 50;  // in ms

  while (1) {
    unsigned long startTime = millis();
    byte sg = readButtons();
    if (sg == BUTTON_SELECT) {
      Utils::openFileByIndex(currentIndex);
      resetSpeccy();
      break;
    } else if (sg == BUTTON_DOWN || sg == BUTTON_UP) {
      HighLightFile();
    } else {
      if (oldIndex != currentIndex) {
        updateFileName();
        oldIndex = currentIndex;
      }
    }

    unsigned long timeSpent = millis() - startTime;
    if (timeSpent < maxButtonInputLoopTime) {
      delay(maxButtonInputLoopTime - timeSpent);  // mainly to stop button bounce
    }
  }

  if (haveOled) {
    file.getName(fileName, 15);
    oled.clear();
    oled.print(F("Loading:\n"));
    oled.println(fileName);
  }

  // Read the Snapshots 27-byte header
  if (file.available()) {
    //byte bytesReadHeader = (byte)file.read(&head27_2[0 + 2], 27);
    byte bytesReadHeader = (byte)file.read(&head27_2[0 + 1], 27);
    if (bytesReadHeader != 27) { debugFlash(3000); }
  }

  packetBuffer[0] = 'G';
//  packetBuffer[1] = 'O';
  uint16_t currentAddress = 0x4000;  //starts at beginning of screen
  while (file.available()) {
    byte bytesRead = (byte)file.read(&packetBuffer[SIZE_OF_HEADER], PAYLOAD_BUFFER_SIZE);
    packetBuffer[HEADER_PAYLOADSIZE] = bytesRead;
    packetBuffer[HEADER_HIGH_BYTE] = (currentAddress >> 8) & 0xFF;  // high byte
    packetBuffer[HEADER_LOW_BYTE] = currentAddress & 0xFF;          // low byte
    sendBytes(packetBuffer, SIZE_OF_HEADER + bytesRead);
    currentAddress += packetBuffer[HEADER_PAYLOADSIZE];
  }

  file.close();

  // *** Send Snapshot Header Section ***
  sendBytes(head27_2, sizeof(head27_2));  // Send entire header

  // Resend DE,BC & AF in that order - Z80 ignored them in the previous send
  sendBytes(&head27_2[1 + 11], 2);  // Send DE
  sendBytes(&head27_2[1 + 13], 2);  // Send BC
  sendBytes(&head27_2[1 + 21], 2);  // Send AF

  // Wait for the Z80 processor to halt.
  // The ZX Spectrum's maskable interrupt (triggered by the 50Hz screen refresh) 
  // will release the CPU from halt mode during the vertical blanking period.
  waitHalt();  // First halt - triggered by the 50Hz maskable interrupt

  // At this point the Spectrum is running code in screen memory to safely swap ROM banks.
  // The Spectrum runs a 2nd halt so we can tell it's started executing the final launch code.
  waitHalt();  // 2nd halt - triggered by the 50FPS maskable interrupt

  // delayMicroseconds(7) seems to fix same odd timing issues, though the exact reason is unclear.
  // In "FireFly" two minor clicks at the start of the music happen without this delay.
  delayMicroseconds(7);

  bitSet(PORTC, DDC1);  // pin15 (A1) - Switch to high part of the ROM.
  // At ths point rhe Spectrum's external ROM has switched to using the 16K stock ROM.

  while (bitRead(PINB, PINB0) == LOW) {}  // DEBUG, blocking here if Z80's -HALT still in action

  if (haveOled) {
    oled.clear();
    oled.print("running:\n");
    oled.println(fileName);
  }

  while (1) {
    unsigned long startTime = millis();
    PORTD = readJoystick();  // output to z80 data lines
    if (getAnalogButton(analogRead(21)) == 2) {
      resetSpeccy();
      break;
    }
    unsigned long timeSpent = millis() - startTime;
    if (timeSpent < maxButtonInputLoopTime) {
      delay(maxButtonInputLoopTime - timeSpent);
    }
  }
}

//-------------------------------------------------
// Section: Z80 Data Transfer and Control Routines
//-------------------------------------------------

//__attribute__((optimize("-Ofast"))) 
void sendBytes(byte *data, byte size) {
  for (byte bufferIndex = 0; bufferIndex < size; bufferIndex++) {
    while ((bitRead(PINB, PINB0) == HIGH)) {};
    PORTD = data[bufferIndex];
    bitClear(PORTC, DDC0);
    bitSet(PORTC, DDC0);
    while ((bitRead(PINB, PINB0) == LOW)) {};
  }
}

void waitHalt() {
  // Here we are waiting for the speccy to release the halt, it does this every 50fps.
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

void resetSpeccy(){
  bitClear(PORTC, DDC3);  // reset-line "LOW" to speccy
  bitClear(PORTC, DDC1);  // pin15 (A1) - Switch over to the lower part of the ROM.
  delay(10);
  bitSet(PORTC, DDC3);    // reset-line "HIGH" allow speccy to startup
}

//-------------------------------------------------
// Section: File Selector
//-------------------------------------------------

void moveUp() {
  if (currentIndex > startIndex) {
    currentIndex--;
  } else if (startIndex > 0) {
    startIndex -= WINDOW_SIZE;
    currentIndex = startIndex + WINDOW_SIZE - 1;
    refreshFileList();
  }
}

void moveDown() {
  if (currentIndex < startIndex + WINDOW_SIZE - 1 && currentIndex < totalFiles - 1) {
    currentIndex++;
  } else if (startIndex + WINDOW_SIZE < totalFiles) {
    startIndex += WINDOW_SIZE;
    currentIndex = startIndex;
    refreshFileList();
  }
}

//-------------------------------------------------
// Section: Arduino User Input Support
//-------------------------------------------------

uint8_t getAnalogButton(int but) {
  byte buttonPressed = 0;
  if (but < (100 + 22)) {
    buttonPressed = BUTTON_UP;
  } else if (but < (100 + 324)) {
    buttonPressed = BUTTON_SELECT;
  } else if (but < (100 + 510)) {
    buttonPressed = BUTTON_DOWN;
  }
  return buttonPressed;
}

uint8_t readJoystick() {
  bitSet(PORTB, DDB2);  // HIGH, pin 10, disable input latching/enable shifting
  bitSet(PORTC, DDC2);  // HIGH, pin 16, clock to retrieve first bit

  byte data = 0;
  // Read the data byte using shiftIn replacement with direct port manipulation
  for (uint8_t i = 0; i < 8; i++) {  // only need first 5 bits "000FUDLR"
    data <<= 1;
    if (bitRead(PINB, 1)) {  // Reads data from pin 9 (PB1)
      bitSet(data, 0);
    }
    // Toggle clock pin low to high to read the next bit
    bitClear(PORTC, DDC2);  // Clock low
    delayMicroseconds(1);   // Short delay to ensure stable clocking
    bitSet(PORTC, DDC2);    // Clock high
  }
  bitClear(PORTB, DDB2);  //LOW, pin 10 (PB2),  Disable shifting (latch)

  // TODO - CODE PATCH - REMOVE FOR NEW v0.14 PCB
  byte bit1 = (data >> 1) & 1;
  byte bit2 = (data >> 2) & 1;
  if (bit1 != bit2) {
    data ^= (1 << 1);
    data ^= (1 << 2);
  }
  return data;
}

byte readButtons() {

  // ANALOGUE VALUES ARE AROUND... 1024,510,324,22
  byte ret = 0;
  int but = analogRead(21);
  uint8_t joy = readJoystick();  //"000FUDLR" bit pattern

  if (getAnalogButton(but) || (joy & B00011100)) {
    unsigned long currentMillis = millis();  // Get the current time
    if (buttonHeld && (currentMillis - lastButtonHoldTime >= buttonDelay)) {
      // Minimum delay of 50ms, decrease by 20ms each time
      buttonDelay = max(40, buttonDelay - 20);
      lastButtonHoldTime = currentMillis;
    }

    if ((!buttonHeld) || (currentMillis - lastButtonPress >= buttonDelay)) {
      if (but < (100 + 22) || (joy & B00000100)) {  // 1st button
        moveDown();
        ret = BUTTON_UP;
      } else if (but < (100 + 324) || (joy & B00010000)) {  // 2nd button
        ret = BUTTON_SELECT;
      } else if (but < (100 + 510) || (joy & B00001000)) {  // 3rd button
        moveUp();
        ret = BUTTON_DOWN;
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
  return ret;
}

//-------------------------------------------------
// Section: Graphics Support for Zx Spectrum Screen
//-------------------------------------------------

void clearScreenAttributes() {
  uint16_t amount = 768;
  uint16_t currentAddress = 0x5800;
  packetBuffer[0] = 'F';                          // Fill mode
//  packetBuffer[1] = 'L';
  packetBuffer[1] = (amount >> 8) & 0xFF;         // high byte
  packetBuffer[2] =  amount & 0xFF;               // low byte
  packetBuffer[3] = (currentAddress >> 8) & 0xFF; // high byte
  packetBuffer[4] = currentAddress & 0xFF;        // low byte
  packetBuffer[5] = B01000111;  // FBPPPIII;  
  sendBytes(packetBuffer, 6 );  // Send clear command for z80 to process
}

void HighLightFile() {
  //0 Black, 1 Blue, 2 Red, 3 Magenta, 4 Green, 5 Cyan, 6 Yellow, 7 White	

  // Draw selector bar - blue with white text
  uint16_t amount = 32;
  uint16_t currentAddress = 0x5800 + ((currentIndex - startIndex) * 32);
  packetBuffer[0] = 'F';                          // Fill mode
//  packetBuffer[1] = 'L';
  packetBuffer[1] = (amount >> 8) & 0xFF;         // high byte
  packetBuffer[2] =  amount & 0xFF;               // low byte
  packetBuffer[3] = (currentAddress >> 8) & 0xFF; // high byte
  packetBuffer[4] = currentAddress & 0xFF;        // low byte
  packetBuffer[5] = B00101000;  // FBPPPIII; background colour
  sendBytes(packetBuffer, 6 );

  // clear away old selector bar
  if (oldHighlightAddress != currentAddress) {
    packetBuffer[3] = (oldHighlightAddress >> 8) & 0xFF;
    packetBuffer[4] = oldHighlightAddress & 0xFF;
    packetBuffer[5] = B01000111;  // FBPPPIII
    sendBytes(packetBuffer, 6);
    oldHighlightAddress = currentAddress;
  }
}

__attribute__((optimize("-Ofast"))) 
void refreshFileList() {
  root.rewind();
  uint8_t clr = 0;
  uint8_t count = 0;
  while (file.openNext(&root, O_RDONLY)) {
    if (file.isFile()) {
      if (file.fileSize() == 49179) {

        if ((count >= startIndex) && (count < startIndex + WINDOW_SIZE)) {
          int len = file.getName7(fileName, 64);
          if (len== 0) { file.getSFN(fileName, 20); }

          if (len>42) {
            fileName[40] = '.';
            fileName[41] = '.';
            fileName[42] = '\0';
          }

          DrawSelectorText(0, ((count - startIndex) * 8), fileName);
          clr++;
        }
        count++;
      }
    }
    file.close();
    if (clr==WINDOW_SIZE) {
      break;
    }
  }

  fileName[0] = ' ';  // Empty file selector slots
  fileName[1] = '\0';
  for (uint8_t i = clr; i < WINDOW_SIZE; i++) {
    DrawSelectorText(0, (i * 8), fileName);
  }
}


__attribute__((optimize("-Ofast")))
uint8_t prepareTextGraphics(const char *message) {

  Utils::memsetZero(&finalOutput[0], _FONT_BUFFER_SIZE * _FONT_HEIGHT);
  uint16_t bitPosition = 0;
  uint8_t charCount = 0;
  for (uint8_t i = 0; message[i] != '\0'; i++) {
    const uint8_t *ptr = &fudged_Adafruit5x7[((message[i] - 0x20) * 5) + 0 + 6];
    for (uint8_t row = 0; row < _FONT_HEIGHT; row++) {
      uint8_t transposedRow = 0;
      for (uint8_t col = 0; col < _FONT_WIDTH; col++) {
        byte value = pgm_read_byte(&ptr[col]);
        transposedRow |= ((value >> row) & 0x01) << (_FONT_WIDTH - 1 - col);
      }
      bitPosition = (_FONT_BUFFER_SIZE * row) * 8 + (i * (_FONT_WIDTH + _FONT_GAP));
      Utils::joinBits(finalOutput, transposedRow, _FONT_WIDTH + _FONT_GAP, bitPosition);
    }
    charCount++;
  }
  return charCount;
}

__attribute__((optimize("-Ofast"))) 
void DrawSelectorText(int xpos, int ypos, const char *message) {

  uint8_t charCount = prepareTextGraphics(message);
  uint8_t byteCount = ((charCount * 6) + 7) / 8;      // to byte count with byte alignment
  int8_t clr = 32 - byteCount;    // bytes to clear screen after the text

  // Setup fill mode command for clearing space after text.
  // The packetBuffer[64] offset is used to share with the text buffer
  packetBuffer[64+0] = 'F';  // Fill mode
  //packetBuffer[64+1] = 'L';
  packetBuffer[64+1] = (clr >> 8) & 0xFF;             // high byte
  packetBuffer[64+2] = clr & 0xFF;                    // low byte
  packetBuffer[64+5] = B00000000; // Fill pattern

  // Packet for sending text data
  packetBuffer[HEADER_START_G] = 'C';  // copy data
//  packetBuffer[HEADER_START_O] = 'P';
  packetBuffer[HEADER_PAYLOADSIZE] = byteCount;

  for (uint8_t y = 0; y < _FONT_HEIGHT; y++) {
    uint8_t *rowBufferPtr = &finalOutput[y * _FONT_BUFFER_SIZE];
    uint16_t currentAddress = Utils::zx_spectrum_screen_address(xpos, ypos + y);

    packetBuffer[HEADER_HIGH_BYTE] = (currentAddress >> 8) & 0xFF;
    packetBuffer[HEADER_LOW_BYTE] = currentAddress & 0xFF;
    // Copy the text graphic data into the packet buffer
    for (int i = 0; i < byteCount; i++) {
      packetBuffer[SIZE_OF_HEADER + i] = rowBufferPtr[i];
    }
    sendBytes(packetBuffer, SIZE_OF_HEADER + byteCount);

    // Clear space after the text
    if (clr > 0) {
      currentAddress += byteCount;
      packetBuffer[64+3] = (currentAddress >> 8) & 0xFF;  // high byte
      packetBuffer[64+4] = currentAddress & 0xFF;         // low byte
      sendBytes(&packetBuffer[64], 6);     // Send the fill command
    }
  }
}

__attribute__((optimize("-Os")))
void DrawText(int xpos, int ypos, const char *message) {
  uint8_t byteCount = prepareTextGraphics(message);
  byteCount = ((byteCount * (_FONT_WIDTH + _FONT_GAP)) + 7) / 8;  // byte alignment
  packetBuffer[HEADER_START_G] = 'C';
//  packetBuffer[HEADER_START_O] = 'P';
  packetBuffer[HEADER_PAYLOADSIZE] = byteCount;
  for (uint8_t y = 0; y < _FONT_HEIGHT; y++) {
    uint8_t *ptr = &finalOutput[y * _FONT_BUFFER_SIZE];
    uint16_t currentAddress = Utils::zx_spectrum_screen_address(xpos, ypos + y); 
    packetBuffer[HEADER_HIGH_BYTE] = (currentAddress >> 8) & 0xFF;
    packetBuffer[HEADER_LOW_BYTE] = currentAddress & 0xFF;
    for (int i = 0; i < byteCount; i++) {  // copy gfx
      packetBuffer[SIZE_OF_HEADER + i] = ptr[i];
    }
    sendBytes(packetBuffer, SIZE_OF_HEADER + byteCount);
  }
}

//-------------------------------------------------
// Section: OLED Support 
//-------------------------------------------------

void updateFileName() {
  if (haveOled) {
    Utils::openFileByIndex(currentIndex);
    uint8_t a = file.getName7(fileName, 41);
    if (a == 0) { a = file.getSFN(fileName, 40); }
    oled.setCursor(0, 0);
    oled.print(F("Select:\n"));
    oled.print(fileName);
    oled.print("      ");
    file.close();
  }
}

bool setupOled() {

  // First check that we have an OLED installed
  Wire.begin();
  Wire.beginTransmission(I2C_ADDRESS);
  bool result = (Wire.endTransmission() == 0); 
  Wire.end();

  // Initialise OLED
  if (result) {
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

  return result;
}

//-------------------------------------------------
// Section: SD Card
//-------------------------------------------------

void InitializeSDcard() {
  
  if (totalFiles > 0) {
    root.close();
    sd.end();
  }

  while (1) {
    if (!sd.begin()) {}

    if (root.open("/")) {
      totalFiles = Utils::getSnaFileCount();
      if (totalFiles > 0) {
        break;
      } else {
        DrawText(80, 100, "NO FILES FOUND");
      }
    } else {
      DrawText(80, 80, "INSERT SD CARD");
      root.close();
      sd.end();
    }
    delay(1000/50);
  }
}

//-------------------------------------------------
// Section: Debug Supprt
//-------------------------------------------------

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



/*
inline byte reverseBits(byte data) {
  data = (data & 0xF0) >> 4 | (data & 0x0F) << 4;
  data = (data & 0xCC) >> 2 | (data & 0x33) << 2;
  data = (data & 0xAA) >> 1 | (data & 0x55) << 1;
  return data;
}
*/