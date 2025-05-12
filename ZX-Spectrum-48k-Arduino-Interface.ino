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

// Note about slow roms giving unextpected behaviour.

#include <Arduino.h>
//#include <avr/pgmspace.h>
#include <SPI.h>
#include <Wire.h>

#include "SdFat.h"  // "SdFatConfig.h" options, I'm using "USE_LONG_FILE_NAMES 1"
#include "SSD1306AsciiAvrI2c.h"

#include "utils.h"
#include "smallfont.h"
#include "pins.h"
#include "ScrSupport.h"
#include "Z80Bus.h"
#include "Buffers.h"

#define VERSION ("0.14")

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

enum { BUTTON_NONE,
       BUTTON_DOWN,
       BUTTON_SELECT,
       BUTTON_UP,
       BUTTON_DOWN_REFRESH_LIST,
       BUTTON_UP_REFRESH_LIST };

unsigned long lastButtonPress = 0;     // Store the last time a button press was processed
unsigned long buttonDelay = 300;       // Initial delay between button actions in milliseconds
unsigned long lastButtonHoldTime = 0;  // Track how long the button has been held
bool buttonHeld = false;               // Track if the button is being held
uint16_t oldHighlightAddress = 0;      // Last highlighted screen position for clearing away
uint16_t currentFileIndex = 0;         // The currently selected file index in the list
uint16_t startFileIndex = 0;           // The start file index of the current viewing window

void setup() {
  //  Serial.begin(9600);
  //  while (! Serial) {};
  //  Serial.println("STARTING");

  Z80Bus::setupReset();
  Z80Bus::setupNMI();
  Z80Bus::setupDataLine(OUTPUT);
  Z80Bus::setupHalt();
  NanoPins::setupShiftRegister();

  /* Optional - if detected display output to a 128x32 pixel OLED */
  haveOled = setupOled();

  if (getAnalogButton(analogRead(BUTTON_PIN)) == BUTTON_SELECT) {
    /* Select button was held down, allows speccy to revert into stock ROM */
    Z80Bus::bankSwitchStockRom();
    Z80Bus::resetZ80();
    // TO DO -  allow user to go back to the sna loading mode  ???
    while(true) { delay(50); }
  }
}

void loop() {


  Z80Bus::resetToSnaRom();
  Z80Bus::setupScreenAttributes(Utils::Ink7Paper0);   // setup the whole screen

  InitializeSDcard();

  if (getAnalogButton(analogRead(BUTTON_PIN)) == BUTTON_DOWN) {
    ScrSupport::DemoScrFiles(root, file, packetBuffer);
  }

  updateOledFileName();
  drawFileList();
  Z80Bus::highlightSelection(currentFileIndex, startFileIndex, oldHighlightAddress);

   //Wait here until SELECT is released, so we don't retrigger on the same press.
  while ( getAnalogButton( analogRead(BUTTON_PIN) ) == BUTTON_SELECT ) {
    delay(1); // do nothing
  }
    
  // ***************************************
  // *** Main loop - file selection menu ***
  // ***************************************
  while (true) {
    unsigned long start = millis();
    byte action = doMenu();
    if (action == BUTTON_SELECT) {
      break;
    }
    Utils::frameDelay(start);
  }

  oledLoadingMessage();
/*
  Z80Bus::sendBytes(stackCommand, sizeof(stackCommand));
  Z80Bus::waitRelease_NMI();
*/
  
  const uint16_t address = 0x4004;
  packetBuffer[0] = 'S';
  packetBuffer[1] = (uint8_t)(((address) >> 8) & 0xFF);
  packetBuffer[2] = (uint8_t)((address)&0xFF);
  Z80Bus::sendBytes(packetBuffer, 3);
  Z80Bus::waitRelease_NMI(); 

  //-----------------------------------------
  // Read the Snapshots 27-byte header
  if (file.available()) {
    byte bytesReadHeader = (byte)file.read(&head27_Execute[0 + 1], 27);
    if (bytesReadHeader != 27) { debugFlash(3000); }

    // TODO: show error message
  } 
  //-----------------------------------------
  // Copy snapshot data into Speccy RAM (0x4000-0xFFFF)
  packetBuffer[0] = 'G';
  uint16_t currentAddress = 0x4000;  //starts at beginning of screen
  while (file.available()) {
    byte bytesRead = (byte)file.read(&packetBuffer[SIZE_OF_HEADER], PAYLOAD_BUFFER_SIZE);
    START_UPLOAD_COMMAND(packetBuffer, 'G', bytesRead);
    END_UPLOAD_COMMAND(packetBuffer, currentAddress);
    Z80Bus::sendBytes(packetBuffer, SIZE_OF_HEADER + (uint16_t)bytesRead);
    currentAddress += packetBuffer[HEADER_PAYLOADSIZE];
  }
  file.close();
  //-----------------------------------------
  // Command Speccy to wait for the next screen frefresh
  Z80Bus::sendBytes(waitCommand, sizeof(waitCommand));
  //-----------------------------------------
  // The ZX Spectrum's maskable interrupt (triggered by the 50Hz screen refresh) will self release
  // the CPU from halt mode during the vertical blanking period.
  Z80Bus::waitHalt();  // wait for speccy to triggered the 50Hz maskable interrupt
  //-----------------------------------------
  Z80Bus::sendSnaHeader(&head27_Execute[0]);  // sends 27 commands allowing Z80 to patch together the Sna files saved register states.
  //-----------------------------------------
  // At this point the speccy is running code in screen memory to safely swap ROM banks.
  // A final HALT is given just before jumping back to the real snapshot code/game.
  Z80Bus::waitRelease_NMI();  // signal NMI line for the speccy to resume
  //-----------------------------------------
  // T-state duration:  1/3.5MHz = 0.2857μs
  // NMI Z80 timings -  Push PC onto Stack: ~10 or 11 T-states
  //                    RETN Instruction:    14 T-states.
  //                    Total:              ~24 T-states
  delayMicroseconds(7);  // (24×0.2857=6.857) wait for z80 to return to the main code.

  Z80Bus::bankSwitchStockRom();
  // At ths point rhe Spectrum's external ROM has switched to using the 16K stock ROM.

  while (bitRead(PINB, PINB0) == LOW) {}  // DEBUG, blocking here if Z80's -HALT still in action

  oledRunningMessage();

  do {
    unsigned long startTime = millis();
    PORTD = Utils::readJoystick();  // send to the Z80 data lines (Kempston standard)
    Utils::frameDelay(startTime);
  } while (getAnalogButton(analogRead(BUTTON_PIN)) != BUTTON_SELECT);

}

//-------------------------------------------------
// Section: Drawing Support
//-------------------------------------------------

__attribute__((optimize("-Ofast"))) void DrawSelectorText(int xpos, int ypos, const char *message) {
  prepareTextGraphics(message);  // return ignored - drawing the whole buffer
  // Lets draw the whole width of 256 pixles so it will also clear away the old text.
  START_UPLOAD_COMMAND(packetBuffer, 'C', SmallFont::FNT_BUFFER_SIZE);
  uint8_t *outputLine = TextBuffer;
  for (uint8_t y = 0; y < SmallFont::FNT_HEIGHT; ++y, outputLine += SmallFont::FNT_BUFFER_SIZE) {
    uint16_t currentAddress = Utils::zx_spectrum_screen_address(xpos, ypos + y);
    END_UPLOAD_COMMAND(packetBuffer, currentAddress);
    memcpy(&packetBuffer[SIZE_OF_HEADER], outputLine, SmallFont::FNT_BUFFER_SIZE);
    Z80Bus::sendBytes(packetBuffer, SIZE_OF_HEADER + SmallFont::FNT_BUFFER_SIZE);  // transmit each line
  }
}

__attribute__((optimize("-Os"))) void DrawText(int xpos, int ypos, const char *message) {
  uint8_t charCount = prepareTextGraphics(message);
  uint8_t byteCount = ((charCount * (SmallFont::FNT_WIDTH + SmallFont::FNT_GAP)) + 7) / 8;  // byte alignment
  START_UPLOAD_COMMAND(packetBuffer, 'C', byteCount);
  for (uint8_t y = 0; y < SmallFont::FNT_HEIGHT; y++) {
    uint8_t *ptr = &TextBuffer[y * SmallFont::FNT_BUFFER_SIZE];
    uint16_t currentAddress = Utils::zx_spectrum_screen_address(xpos, ypos + y);
    END_UPLOAD_COMMAND(packetBuffer, currentAddress);
    memcpy(&packetBuffer[SIZE_OF_HEADER], ptr, byteCount);
    Z80Bus::sendBytes(packetBuffer, SIZE_OF_HEADER + byteCount);
  }
}

__attribute__((optimize("-Ofast")))
uint8_t
prepareTextGraphics(const char *message) {
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
        (((d0 >> row) & 0x01) << 4) | (((d1 >> row) & 0x01) << 3) | (((d2 >> row) & 0x01) << 2) | (((d3 >> row) & 0x01) << 1) | (((d4 >> row) & 0x01) << 0);

      // compute bit-offset into the big output buffer
      const uint16_t bitPosition = (SmallFont::FNT_BUFFER_SIZE * row) * 8 + (i * (SmallFont::FNT_WIDTH + SmallFont::FNT_GAP));
      Utils::joinBits(TextBuffer, transposedRow, SmallFont::FNT_WIDTH + SmallFont::FNT_GAP, bitPosition);
    }
    charCount++;
  }
  return charCount;
}

//-------------------------------------------------
// Section: OLED Support
//-------------------------------------------------

bool setupOled() {
  Wire.begin();
  Wire.beginTransmission(I2C_ADDRESS);
  bool result = (Wire.endTransmission() == 0);  // is OLED fitted
  Wire.end();

  if (result) {
    // Initialise OLED
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
  return result;  // is OLED hardware available 
}

void updateOledFileName() {
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

void oledLoadingMessage() {
  if (haveOled) {
    file.getName(fileName, 15);
    oled.clear();
    oled.print(F("Loading:\n"));
    oled.println(fileName);
  }
}

void oledRunningMessage() {
  if (haveOled) {
    oled.clear();
    oled.print("running:\n");
    oled.println(fileName);
  }
}


//-------------------------------------------------
// Section: Input Support
//-------------------------------------------------

byte doMenu() {
  byte btn = readButtons();
  switch (btn) {
    case BUTTON_SELECT:
      Utils::openFileByIndex(currentFileIndex);
      break;
    case BUTTON_UP:
    case BUTTON_DOWN:
      updateOledFileName();
      Z80Bus::highlightSelection(currentFileIndex, startFileIndex, oldHighlightAddress);
      break;

    case BUTTON_UP_REFRESH_LIST:
    case BUTTON_DOWN_REFRESH_LIST:
      updateOledFileName();
      drawFileList();
      Z80Bus::highlightSelection(currentFileIndex, startFileIndex, oldHighlightAddress);
      break;

    default:
      break;
  }
  return btn;
}

byte readButtons() {
  // ANALOGUE VALUES ARE AROUND... 1024,510,324,22
  byte ret = BUTTON_NONE;
  int but = analogRead(BUTTON_PIN);
  uint8_t joy = Utils::readJoystick();  // Kempston standard, "000FUDLR" bit pattern

  if (getAnalogButton(but) || (joy & B00011100)) {
    unsigned long currentMillis = millis();     // Get the current time
    if (buttonHeld && (currentMillis - lastButtonHoldTime >= buttonDelay)) {
      buttonDelay = max(40, buttonDelay - 20);  // speed-up file selection over time
      lastButtonHoldTime = currentMillis;
    }

    if ((!buttonHeld) || (currentMillis - lastButtonPress >= buttonDelay)) {
      if (but < (100 + 22) || (joy & B00000100)) {  // 1st button
        if (currentFileIndex < startFileIndex + SCREEN_TEXT_ROWS - 1 && currentFileIndex < totalFiles - 1) {
          currentFileIndex++;
          ret = BUTTON_DOWN;
        } else if (startFileIndex + SCREEN_TEXT_ROWS < totalFiles) {
          startFileIndex += SCREEN_TEXT_ROWS;
          currentFileIndex = startFileIndex;
          ret = BUTTON_DOWN_REFRESH_LIST;
        }
      } else if (but < (100 + 324) || (joy & B00010000)) {  // 2nd button
        ret = BUTTON_SELECT;
      } else if (but < (100 + 510) || (joy & B00001000)) {  // 3rd button
          if (currentFileIndex > startFileIndex) {
            currentFileIndex--;
             ret = BUTTON_UP;
          } else if (startFileIndex >= SCREEN_TEXT_ROWS) {
            startFileIndex -= SCREEN_TEXT_ROWS;
            currentFileIndex = startFileIndex + SCREEN_TEXT_ROWS - 1;
            ret = BUTTON_UP_REFRESH_LIST;
          }
      }
      buttonHeld = true;
      lastButtonPress = currentMillis;
      lastButtonHoldTime = currentMillis;
    }
  } else {   /* no button is pressed - reset delay */
    buttonHeld = false;
    buttonDelay = 300;  // Reset to the initial delay
  }
  return ret;
}

uint8_t getAnalogButton(int but) {
  static constexpr int BUTTON_UP_THRESHOLD = 122;
  static constexpr int BUTTON_SELECT_THRESHOLD = 424;
  static constexpr int BUTTON_DOWN_THRESHOLD = 610;

  if (but < BUTTON_UP_THRESHOLD) {
    return BUTTON_UP;
  } else if (but < BUTTON_SELECT_THRESHOLD) {
    return BUTTON_SELECT;
  } else if (but < BUTTON_DOWN_THRESHOLD) {
    return BUTTON_DOWN;
  }
  return BUTTON_NONE;
}


//-------------------------------------------------
// Section: SD Card Support
//-------------------------------------------------

void InitializeSDcard() {

  if (totalFiles > 0) {
    root.close();
    sd.end();
    totalFiles = 0;
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
    delay(1000 / 50);
  }
}

__attribute__((optimize("-Ofast"))) void drawFileList() {
  root.rewind();
  uint8_t clr = 0;
  uint8_t count = 0;
  while (file.openNext(&root, O_RDONLY)) {
    if (file.isFile()) {
      if (file.fileSize() == 49179) {

        if ((count >= startFileIndex) && (count < startFileIndex + SCREEN_TEXT_ROWS)) {
          int len = file.getName7(fileName, 64);
          if (len == 0) { file.getSFN(fileName, 20); }

          if (len > 42) {
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
    if (clr == SCREEN_TEXT_ROWS) {
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
