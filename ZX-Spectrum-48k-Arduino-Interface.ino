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
#include <Wire.h> 

#include "SdFat.h"  // "SdFatConfig.h" options, I'm using "USE_LONG_FILE_NAMES 1"
#include "SSD1306AsciiAvrI2c.h"

#include "utils.h"
#include "smallfont.h"
#include "pins.h"

#define VERSION ("0.14")

// -----------------------------------------------------------------------------------
// Copy/Transfer Structure
//
// - Index for command character 
//  'C'    // Transfer/copy (things like drawing Text, displying .scr files)
//	'F'    // Fill (clearing screen areas, selector bar)   
//  'G'    // Transfer/copy with flashing boarder (use for loading .sna files)
//  'E'    // Execute program (includes restoring registers/states from the 27 byte header) 
//  'W'    // Wait for 50Hz maskable interrupt (prevents interrupts from interfering at final run stage)
const uint8_t HEADER_TOKEN = 0;          // Hold command charater 'C','F','G',E','W'
/* Index for payload as bytes (max send of 255 bytes) */
const uint8_t HEADER_PAYLOADSIZE = 1;    // Only 'C' and 'G' commands
/* Index for high byte of destination address */
const uint8_t HEADER_HIGH_BYTE = 2;      // Only 'C' and 'G' commands
/* Index for low byte of destination addresa */
const uint8_t HEADER_LOW_BYTE = 3;       // Only 'C' and 'G' commands
// ----------------------------------------------------------------------------------
const uint8_t SIZE_OF_HEADER = HEADER_LOW_BYTE + 1;
/* Maximum payload per transfer is one byte (0–255) */   
const uint8_t PAYLOAD_BUFFER_SIZE = 255;
const uint16_t BUFFER_SIZE = SIZE_OF_HEADER + PAYLOAD_BUFFER_SIZE;
// ----------------------------------------------------------------------------------  
static uint8_t head27_Execute[27 + 1] = { 'E' };   // pre populate with Execute command 
static uint8_t waitCommand[1] = { 'W' };           // pre populate with Wait command 
static uint8_t packetBuffer[BUFFER_SIZE] ;         // Used by 'C','G' & 'F' commands
// ----------------------------------------------------------------------------------
#define SCREEN_TEXT_ROWS 24            // Number of on screen text list items
byte TextBuffer[SmallFont::FNT_BUFFER_SIZE * SmallFont::FNT_HEIGHT] = { 0 };
// ----------------------------------------------------------------------------------
// SD card support
SdFat32 sd;
FatFile root;
FatFile file;
uint16_t totalFiles;
char fileName[65];
// ----------------------------------------------------------------------------------
// OLED support
#define I2C_ADDRESS 0x3C  // 0x3C or 0x3D
SSD1306AsciiAvrI2c oled;
bool haveOled = false;
// ----------------------------------------------------------------------------------

enum { BUTTON_NONE, BUTTON_DOWN, BUTTON_SELECT, BUTTON_UP };
unsigned long lastButtonPress = 0;     // Store the last time a button press was processed
unsigned long buttonDelay = 300;       // Initial delay between button actions in milliseconds
unsigned long lastButtonHoldTime = 0;  // Track how long the button has been held
bool buttonHeld = false;               // Track if the button is being held
uint16_t oldHighlightAddress= 0;      // Last highlighted screen position for clearing away
uint16_t currentFileIndex = 0;        // The currently selected file index in the list
uint16_t startFileIndex = 0;          // The start file index of the current viewing window


void setup() {

  //  Serial.begin(9600);
  //  while (! Serial) {};
  //  Serial.println("STARTING");


  // Reset Z80
  pinMode(Z80_REST, OUTPUT);
  WRITE_BIT(PORTC, DDC3, LOW);  // reset-line "LOW" speccy
  delay(10);
  WRITE_BIT(PORTC, DDC3, HIGH);  // reset-line "HIGH" allow speccy to startup

  // Setup pins for "74HC165" shift register
  pinMode(ShiftRegDataPin, INPUT);
  pinMode(ShiftRegLatchPin, OUTPUT);
  pinMode(ShiftRegClockPin, OUTPUT);

  // 16k Rom switching (by sellecting which rom half from a 32k Rom)
  WRITE_BIT(PORTC, DDC1, LOW);
  pinMode(ROM_HALF, OUTPUT);  // LOW external/HIGH stock rom
  WRITE_BIT(PORTC, DDC1, LOW);      // pin15, A1 , swap out stock rom

  // Connect to Z80's NMI Line to unhalt Z80 (using NMI due to speccys maskable taken for 50fps H/W refresh)
  pinMode(Z80_NMI, OUTPUT);  // Connetcs to the Z80 /NMI which releases the z80's 'halt' state
  WRITE_BIT(PORTC, DDC0, HIGH);       // pin14 (A0), Z80 /NMI line

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
    WRITE_BIT(PORTC, DDC1, HIGH);  // pin15 (A1) - Switch to high part of the ROM.
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
              ASSIGN_16BIT_COMMAND(packetBuffer,'C',bytesRead);
              ADDR_16BIT_COMMAND(packetBuffer,currentAddress);
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
      Utils::openFileByIndex(currentFileIndex);
      resetSpeccy();
      break;
    } else if (sg == BUTTON_DOWN || sg == BUTTON_UP) {
      HighLightFile();
    } else {
      if (oldIndex != currentFileIndex) {
        updateFileName();
        oldIndex = currentFileIndex;
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
    byte bytesReadHeader = (byte)file.read(&head27_Execute[0 + 1], 27);
    if (bytesReadHeader != 27) { debugFlash(3000); }
  }

  packetBuffer[0] = 'G';
  uint16_t currentAddress = 0x4000;  //starts at beginning of screen
  while (file.available()) {
    byte bytesRead = (byte)file.read(&packetBuffer[SIZE_OF_HEADER], PAYLOAD_BUFFER_SIZE);
    ASSIGN_16BIT_COMMAND(packetBuffer,'G',bytesRead);
    ADDR_16BIT_COMMAND(packetBuffer,currentAddress);
    sendBytes(packetBuffer, SIZE_OF_HEADER + (uint16_t)bytesRead);
    currentAddress += packetBuffer[HEADER_PAYLOADSIZE];
  }
  file.close();

  //-----------------------------------------
  sendBytes(waitCommand, sizeof(waitCommand));  // Command Speccy to wait for the next screen frefresh
  // The ZX Spectrum's maskable interrupt (triggered by the 50Hz screen refresh) will self release
  // the CPU from halt mode during the vertical blanking period.
  waitHalt();  // wait for speccy to triggered the 50Hz maskable interrupt
  //-----------------------------------------

  // *** Send Snapshot Header Section ***
  // head27_2[0] contains "E" which informs the speccy this packets a execute command.
  sendBytes(&head27_Execute[   0], 1+1+2+2+2+2 );  // Send command "E" then I,HL',DE',BC',AF'
  sendBytes(&head27_Execute[1+15], 2+2+1+1 );   // Send IY,IX,IFF2,R (packet data continued)
  sendBytes(&head27_Execute[1+23], 2);          // Send SP                     "
  sendBytes(&head27_Execute[1+ 9], 2);          // Send HL                     "
  sendBytes(&head27_Execute[1+25], 1);          // Send IM                     "
  sendBytes(&head27_Execute[1+26], 1);          // Send BorderColour           "
  sendBytes(&head27_Execute[1+11], 2);          // Send DE                     "
  sendBytes(&head27_Execute[1+13], 2);          // Send BC                     "
  sendBytes(&head27_Execute[1+21], 2);          // Send AF                     "

  // At this point the speccy is running code in screen memory to safely swap ROM banks.
  // A final HALT is given just before jumping back to the real snapshot code/game.
  waitRelease_NMI();  // signal NMI line for the speccy to resume

  // T-state duration:  1/3.5MHz = 0.2857μs 
  // NMI Z80 timings -  Push PC onto Stack: ~10 or 11 T-states
  //                    RETN Instruction:    14 T-states.
  //                    Total:              ~24 T-states 
  delayMicroseconds(7);  // (24×0.2857=6.857) wait for z80 to return to the main code.

  WRITE_BIT(PORTC, DDC1, HIGH);  // pin15 (A1) - Switch to high part of the ROM.
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

__attribute__((optimize("-Ofast"))) 
void sendBytes(byte *data, uint16_t size) {
  for (uint16_t i = 0; i < size; i++) {
    while ((bitRead(PINB, PINB0) == HIGH)) {};
    PORTD = data[i];
    WRITE_BIT(PORTC, DDC0, LOW);
    WRITE_BIT(PORTC, DDC0, HIGH);
    while ((bitRead(PINB, PINB0) == LOW)) {};
  }
}

void waitHalt() {
  // Here we are waiting for the speccy to release the halt, it does this every 50fps.
  while ((bitRead(PINB, PINB0) == HIGH)) {};
  while ((bitRead(PINB, PINB0) == LOW)) {};
}

void waitRelease_NMI() {
  // Here we MUST release the z80 for it's HALT state.
  while ((bitRead(PINB, PINB0) == HIGH)) {};  // (1) Wait for Halt line to go LOW.
  WRITE_BIT(PORTC, DDC0, LOW);                      // (2) A0 (pin-8) signals the Z80 /NMI line,
  WRITE_BIT(PORTC, DDC0, HIGH);                        //     LOW then HIGH, this will un-halt the Z80.
  while ((bitRead(PINB, PINB0) == LOW)) {};   // (3) Wait for halt line to go HIGH again.
}

void resetSpeccy(){
  WRITE_BIT(PORTC, DDC3, LOW);  // reset-line "LOW" to speccy
  WRITE_BIT(PORTC, DDC1, LOW);  // pin15 (A1) - Switch over to the lower part of the ROM.
  delay(10);
  WRITE_BIT(PORTC, DDC3, HIGH);    // reset-line "HIGH" allow speccy to startup
}

//-------------------------------------------------
// Section: File Selector
//-------------------------------------------------

void moveUp() {
  if (currentFileIndex > startFileIndex) {
    currentFileIndex--;
  } else if (startFileIndex >= SCREEN_TEXT_ROWS) {
    startFileIndex -= SCREEN_TEXT_ROWS;
    currentFileIndex = startFileIndex + SCREEN_TEXT_ROWS - 1;
    refreshFileList();
  }
}

void moveDown() {
  if (currentFileIndex < startFileIndex + SCREEN_TEXT_ROWS - 1 && currentFileIndex < totalFiles - 1) {
    currentFileIndex++;
  } else if (startFileIndex + SCREEN_TEXT_ROWS < totalFiles) {
    startFileIndex += SCREEN_TEXT_ROWS;
    currentFileIndex = startFileIndex;
    refreshFileList();
  }
}

//-------------------------------------------------
// Section: Arduino User Input Support
//-------------------------------------------------

#define BUTTON_UP_THRESHOLD    122
#define BUTTON_SELECT_THRESHOLD 424
#define BUTTON_DOWN_THRESHOLD   610

uint8_t getAnalogButton(int but) {
  if (but < BUTTON_UP_THRESHOLD) {
    return BUTTON_UP;
  } else if (but < BUTTON_SELECT_THRESHOLD) {
    return BUTTON_SELECT;
  } else if (but < BUTTON_DOWN_THRESHOLD) {
    return BUTTON_DOWN;
  }
  return BUTTON_NONE;
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

/* -------------------------------------------------
 * Section: Graphics Support for Zx Spectrum Screen
 * -------------------------------------------------
 * [F|B|P2|P1|P0|I2|I1|I0]
 * bit F sets the attribute FLASH mode
 * bit B sets the attribute BRIGHTNESS mode
 * bits P2 to P0 is the PAPER colour
 * bits I2 to I0 is the INK colour
 */
void clearScreenAttributes() {
    const uint16_t amount = 768;      
    const uint16_t startAddress = 0x5800;
    const uint8_t  whiteText = B01000111;
    /* Fill mode */
    FILL_8BIT_COMMAND(packetBuffer, amount, startAddress, whiteText );
    sendBytes(packetBuffer, 6);
}

void HighLightFile() {
  /* Draw Cyan selector bar with black text (FBPPPIII) */
  /* BITS COLOUR KEY: 0 Black, 1 Blue, 2 Red, 3 Magenta, 4 Green, 5 Cyan, 6 Yellow, 7 White	*/
  const uint16_t amount = 32;
  const uint16_t startAddress = 0x5800 + ((currentFileIndex - startFileIndex) * 32);
  /* Highlight file selection - B00101000: Black text, Cyan background*/
  FILL_8BIT_COMMAND(packetBuffer, amount, startAddress, B00101000 );    
  sendBytes(packetBuffer, 6 );

  if (oldHighlightAddress != startAddress) {
    /* Remove old highlight - B01000111: Restore white text/black background for future use */
    FILL_8BIT_COMMAND(packetBuffer, amount, oldHighlightAddress, B01000111 );
    sendBytes(packetBuffer, 6);
    oldHighlightAddress = startAddress;
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

        if ((count >= startFileIndex) && (count < startFileIndex + SCREEN_TEXT_ROWS)) {
          int len = file.getName7(fileName, 64);
          if (len== 0) { file.getSFN(fileName, 20); }

          if (len>42) {
            fileName[40] = '.';
            fileName[41] = '.';
            fileName[42] = '\0';
          }

          DrawSelectorText(0, ((count - startFileIndex) * 8), fileName);
          clr++;
        }
        count++;
      }
    }
    file.close();
    if (clr==SCREEN_TEXT_ROWS) {
      break;
    }
  }

  // Clear the remaining screen after last list item when needed.
  fileName[0] = ' ';  // Empty file selector slots (just wipes area)
  fileName[1] = '\0';
  for (uint8_t i = clr; i < SCREEN_TEXT_ROWS; i++) {
    DrawSelectorText(0, (i * 8), fileName);
  }
}

__attribute__((optimize("-Ofast")))
uint8_t prepareTextGraphics(const char *message) {
  Utils::memsetZero(&TextBuffer[0], SmallFont::FNT_BUFFER_SIZE * SmallFont::FNT_HEIGHT);
  uint8_t charCount = 0;

  for (uint8_t i = 0; message[i] != '\0'; i++) {
    // font characters in flash
    const uint8_t *ptr = &fudged_Adafruit5x7[((message[i] - 0x20) * SmallFont::FNT_WIDTH) + 6];
    const uint8_t d0 = pgm_read_byte(&ptr[0]);
    const uint8_t d1 = pgm_read_byte(&ptr[1]);
    const uint8_t d2 = pgm_read_byte(&ptr[2]);
    const uint8_t d3 = pgm_read_byte(&ptr[3]);
    const uint8_t d4 = pgm_read_byte(&ptr[4]);

    // build each row’s transposed byte
    for (uint8_t row = 0; row < SmallFont::FNT_HEIGHT; row++) {
      // bit 4 comes from column 0, bit 3 from col 1 ... bit 0 from col 4
      const uint8_t transposedRow =
          (((d0 >> row) & 0x01) << 4) |
          (((d1 >> row) & 0x01) << 3) |
          (((d2 >> row) & 0x01) << 2) |
          (((d3 >> row) & 0x01) << 1) |
          (((d4 >> row) & 0x01) << 0);

      // compute bit-offset into the big output buffer
      const uint16_t bitPosition = (SmallFont::FNT_BUFFER_SIZE * row) * 8 + (i * (SmallFont::FNT_WIDTH + SmallFont::FNT_GAP));
      Utils::joinBits(TextBuffer,transposedRow, SmallFont::FNT_WIDTH + SmallFont::FNT_GAP, bitPosition);
    }
    charCount++;
  }
  return charCount;
}


__attribute__((optimize("-Ofast"))) 
void DrawSelectorText(int xpos, int ypos, const char *message) {
  prepareTextGraphics(message); // return ignored - drawing the whole buffer
  // Lets draw the whole width of 256 pixles so it will also clear away the old text.
  ASSIGN_16BIT_COMMAND(packetBuffer,'C',SmallFont::FNT_BUFFER_SIZE);
  uint8_t *outputLine = TextBuffer;
  for (uint8_t y = 0; y < SmallFont::FNT_HEIGHT; ++y, outputLine += SmallFont::FNT_BUFFER_SIZE) { 
    uint16_t currentAddress = Utils::zx_spectrum_screen_address(xpos, ypos + y);
    ADDR_16BIT_COMMAND(packetBuffer, currentAddress);
    memcpy(&packetBuffer[SIZE_OF_HEADER], outputLine, SmallFont::FNT_BUFFER_SIZE); 
    sendBytes(packetBuffer, SIZE_OF_HEADER + SmallFont::FNT_BUFFER_SIZE);   // transmit each line
  }
}

__attribute__((optimize("-Os")))
void DrawText(int xpos, int ypos, const char *message) {
  uint8_t charCount = prepareTextGraphics(message);
  uint8_t byteCount = ((charCount * (SmallFont::FNT_WIDTH + SmallFont::FNT_GAP)) + 7) / 8;  // byte alignment
  ASSIGN_16BIT_COMMAND(packetBuffer,'C',byteCount);
  for (uint8_t y = 0; y < SmallFont::FNT_HEIGHT; y++) {
    uint8_t *ptr = &TextBuffer[y * SmallFont::FNT_BUFFER_SIZE];
    uint16_t currentAddress = Utils::zx_spectrum_screen_address(xpos, ypos + y); 
    ADDR_16BIT_COMMAND(packetBuffer, currentAddress);
    memcpy(&packetBuffer[SIZE_OF_HEADER], ptr, byteCount);
    sendBytes(packetBuffer, SIZE_OF_HEADER + byteCount);
  }
}

//-------------------------------------------------
// Section: OLED Support 
//-------------------------------------------------

void updateFileName() {
  if (haveOled) {
    Utils::openFileByIndex(currentFileIndex);
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