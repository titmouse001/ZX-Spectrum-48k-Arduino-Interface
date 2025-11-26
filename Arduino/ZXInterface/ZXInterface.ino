// -------------------------------------------------------------------------------------
// This is an Arduino-Based ZX Spectrum Game Loader - 2023/25 P.Overy
// -------------------------------------------------------------------------------------
// This software uses the Arduino's ATmega328P Nano (2K SRAM, 32K flash & 1K EEPROM)
// IMPORTANT: Do not modify PORTB directly without preserving the clock/crystal bits
// -------------------------------------------------------------------------------------
// My default compiler setting here (optimise='-O2'):-
// C:\Users\Admin\AppData\Local\Arduino15\packages\arduino\hardware\avr\1.8.6\platform.txt
// -------------------------------------------------------------------------------------

// Board detection - will cause compile error if wrong board selected
//#if !defined(ARDUINO_AVR_NANO) && !defined(ARDUINO_AVR_UNO)
//#error "This sketch is designed for Arduino Nano (ATmega328P). Please select the correct board in Tools > Board."
//#endif

#if F_CPU != 16000000L
#warning "This sketch expects 16MHz clock speed."
#endif

#include <Arduino.h>
#include "digitalWriteFast.h"
#include "SnapZ80.h"
#include "Menu.h"
#include "Utils.h"
#include "Draw.h"
#include "SdCardSupport.h"

#include "CommandRegistry.h"
#include "PacketTypes.h"
#include "BufferManager.h"
#include "Z80Bus.h"

#define VERSION ("0.24")  // Arduino firmware
#define DEBUG_OLED 0
#define SERIAL_DEBUG 0

// ----------------------------------------------------------------------------------
#if (SERIAL_DEBUG == 1)
#warning "*** SERIAL_DEBUG is enabled ***"
// D0/D1 pins are shared with Z80 data bus. Enabling debug breaks Z80 communication.
#include <SPI.h>
#endif
// ----------------------------------------------------------------------------------
#if (DEBUG_OLED == 1)
#warning "*** DEBUG_OLED enabled: Pin A4 conflict with SD card CS! ***"
//
// OLED Configuration:
//   - Uses I2C pins: A4 (SDA) & A5 (SCL)
//   - Display: 128x32 pixel SSD1306
//   - I2C Address: 0x3C (alternative: 0x3D)
//
// WARNING: Pin A4 serves dual purpose:
//   - OLED SDA (when DEBUG_OLED enabled)
//   - SD card Chip Select (in main code)
//   This creates a hardware conflict!
//
#include <Wire.h>                // I2C for OLED.
#include "SSD1306AsciiAvrI2c.h"  // SSD1306 OLED displays on AVR.
#include "fontdata.h"

#define I2C_ADDRESS 0x3C         // 0x3C or 0x3D
SSD1306AsciiAvrI2c oled;
extern bool setupOled();  // debugging with a 128x32 pixel oled
#endif
// ----------------------------------------------------------------------------------

void setup() {

#if (SERIAL_DEBUG == 1)
  Serial.begin(9600);
  while (!Serial) {};
  Serial.println("SERIAL DEBUG BREAKS COMMS TO Z80");
#endif

// SdCardSupport::init(PIN_A4);
//   uint16_t  a = SdCardSupport::countSnapshotFiles() ;
//  Serial.print("count =");
//  Serial.println(a);

  Z80Bus::setupPins();     // Configures Arduino pins for Z80 bus interface.
  Utils::setupJoystick();  // Initializes joystick input pins.
#if (DEBUG_OLED == 1)
  setupOled();           // For debugging
#endif
  // ---------------------------------------------------------------------------
  // *** Use stock ROM *** when select button or fire held at power up
  if (Utils::readJoystick() & (JOYSTICK_FIRE | JOYSTICK_SELECT)) {
    digitalWriteFast(Pin::ROM_HALF, HIGH);                     //Switches Spectrum to internal ROM.
    Z80Bus::resetZ80();                                        // Resets Z80 for a clean boot from internal ROM.
    while ((Utils::readJoystick() & JOYSTICK_SELECT) != 0) {}  // Debounces button release.
    while (true) {
      if (Utils::readJoystick() & JOYSTICK_SELECT) {
        digitalWriteFast(Pin::ROM_HALF, LOW);  //Switches back to Sna ROM.
        break;                                 // Returns to Sna loader.
      }
      delay(50);
    }
  }


  Z80Bus::resetZ80();
  CommandRegistry::initialize();

  //Z80Bus::fillScreenAttributes(COL::BLACK_WHITE);
  Z80Bus::sendFillCommand( ZX_SCREEN_ATTR_ADDRESS_START, ZX_SCREEN_ATTR_SIZE, COL::Paper5Ink0);
  Draw::text_P(256 - 24, 192 - 8, F(VERSION));

  // SD Init must eat a pin for CS! We use the free OLED debug pin A4 
  // but we dont care about this pin (SD Card's CS pin is fixed to GND).
  while (!SdCardSupport::init(PIN_A4)) {
    Draw::text_P(80, 90, F("INSERT SD CARD"));
    do {  
      delay(20);  
    } while (!SdCardSupport::init(PIN_A4));  // keep looking
    Utils::clearScreen(COL::BLACK_WHITE);
  }
}

// ---------------------
// .SCR FILE 
// ---------------------
void handleScrFile(FatFile* pFile) {
    Utils::clearScreen(0);
    if (pFile->fileSize() == ZX_SCREEN_TOTAL_SIZE) { 
    Z80Bus::transferSnaData(pFile, false);  // No loading effects.
    while (Menu::getButton() == Menu::BUTTON_NONE) { delay(1000/50); }
    Utils::clearScreen(0);
  }
}

// -----------------------------------------------------------------------------------------------
// NOTES: Leaving the menu and restoring the snapshot. Before continuing, we must relocate 
// the Z80 stack to a safer working area. The menu uses 0xFFFF which cannot be left in place.
//
// We use a temporary stack in screen memory (0x4000). This is effectively safe:
//  - 0x4000 also contains the Z80 "jp <patched-start-addr>" bootstrap, we can reuse the location
//    at 0x4003 for temporary 1 deep push/pops, which land at 0x4001/0x4002 due to the SP–2 push behavior.
// This temporary SP is only used until the SNA restore positions the final stack.
// -----------------------------------------------------------------------------------------------

// ---------------------
// .SNA FILE 
// ---------------------
void handleSnaFile(FatFile* pFile) {

  Utils::clearScreen(0);

  if (pFile->fileSize() == SNAPSHOT_FILE_SIZE) {
    Z80Bus::sendStackCommand(ZX_SCREEN_ADDRESS_START + 3, 1);  // 1=set
    // Header information stored globaly in head27_Execute[] for later - will need it for executing the snapshot
    pFile->read((void*)&BufferManager::head27_Execute[E(ExecutePacket::PACKET_LEN)], (size_t)SNA_TOTAL_ITEMS);
    Z80Bus::transferSnaData(pFile, true);
    Z80Bus::executeSnapshot();
    Utils::waitForUserExit();
    Z80Bus::resetToSnaRom();
    CommandRegistry::initialize();
  }
}

// ---------------------
// .Z80 FILE 
// ---------------------
void handleZ80File(FatFile* pFile) {
  Utils::clearScreen(0);

  if (SnapZ80::convertZ80toSNA(pFile)) {  //  == BLOCK_SUCCESS) {
    Z80Bus::sendStackCommand(ZX_SCREEN_ADDRESS_START + 3, 1 );  // 1=set 
    Z80Bus::executeSnapshot();
    Utils::waitForUserExit();
    Z80Bus::resetToSnaRom();
    CommandRegistry::initialize();
  }
}

// ---------------------
// .TXT FILE
// ---------------------

void handleTxtFile(FatFile* pFile) {
  constexpr uint16_t maxCharsPerLine = ZX_SCREEN_WIDTH_PIXELS / SmallFont::FNT_CHAR_PITCH;
  constexpr uint16_t charHeight = SmallFont::FNT_HEIGHT + SmallFont::FNT_GAP;
  constexpr uint16_t maxLinesPerScreen = ZX_SCREEN_HEIGHT_PIXELS / charHeight;
  char* lineBuffer = (char*)&BufferManager::packetBuffer[FILE_READ_BUFFER_OFFSET];
  uint16_t currentPageIndex = 0;
  uint32_t lastSeekPos = 0;    // file position of the start of the last drawn page
  uint16_t lastPageIndex = 0;  // index of that page

  Utils::clearScreen(COL::BLACK_WHITE);

  while (true) {

    const uint32_t autoScrollDelay = millis() + 400;

    if (currentPageIndex == lastPageIndex + 1) {  // forward
      pFile->seekSet(lastSeekPos);
    } else if (currentPageIndex == lastPageIndex - 1) {  // backward
      pFile->seekSet(0);
      for (uint16_t i = 0; i < currentPageIndex; ++i) {
        uint16_t linesReadOnPage = 0;
        while (linesReadOnPage < maxLinesPerScreen) {
          Utils::readLineTxt(pFile,NULL,maxCharsPerLine);  // skip
          linesReadOnPage++;
        }
      }
    } else {                        // page limits
      if (currentPageIndex == 0) {  // hard stop on top page
        pFile->seekSet(0);
      } else if (currentPageIndex == lastPageIndex) {  // can only be last page now
        pFile->seekSet(lastSeekPos);
      }
    }

    // Draw the current page
    uint16_t currentLine = 0;
    while (currentLine < maxLinesPerScreen && pFile->available()) {
      uint16_t bytesRead = Utils::readLineTxt(pFile, lineBuffer, maxCharsPerLine);
      
      // empty line becomes single space
      if (bytesRead == 0) {
        lineBuffer[0] = ' ';
        bytesRead = 1;
      }
      lineBuffer[bytesRead] = '\0';
      Draw::textLine(currentLine * charHeight, lineBuffer);
      currentLine++;
    }

    bool atEOF = !pFile->available();
    if (!atEOF) {
      lastSeekPos = pFile->curPosition();
    }
    lastPageIndex = currentPageIndex;

    lineBuffer[0] = ' ';
    lineBuffer[1] = '\0';
    for (uint8_t i = currentLine; i < maxLinesPerScreen; i++) {   // Clear remaining lines
      Draw::textLine(i * charHeight, lineBuffer);
    }

    Menu::Button_t button;
    do {
      button = Menu::getButton();
    } while (button == Menu::BUTTON_NONE);

    if (button == Menu::BUTTON_ADVANCE) {
      if (!atEOF) {
        currentPageIndex++;
      }
    } else if (button == Menu::BUTTON_BACK) {
      if (currentPageIndex > 0) {
        currentPageIndex--;
      }
    } else if (button == Menu::BUTTON_MENU) {
      return;
    }

    while (Menu::getButton() != Menu::BUTTON_NONE) {
      if (millis() > autoScrollDelay) {
        break;
      }
    }
  }
  while (Menu::getButton() != Menu::BUTTON_NONE) { delay(20); }
}


void loop() {
  
  FatFile* pFile = Menu::handleMenu();

  //if (pFile->fileSize() == ZX_SCREEN_TOTAL_SIZE) {
  if (strcasestr( SdCardSupport::getFileName(pFile) , ".scr")) {
    handleScrFile(pFile);
//  } else if (pFile->fileSize() == SNAPSHOT_FILE_SIZE) {
  } else if (strcasestr( SdCardSupport::getFileName(pFile) , ".sna")) {
    handleSnaFile(pFile);
  } else if (strcasestr( SdCardSupport::getFileName(pFile) , ".z80")) {
    handleZ80File(pFile);
  } else if (strcasestr( SdCardSupport::getFileName(pFile) , ".txt")) {
    handleTxtFile(pFile);
  }

  pFile->close();
}


#if (DEBUG_OLED == 1)
__attribute__((optimize("-Os")))
bool setupOled() {  // DEBUG USE ONLY
  Wire.begin(); 
  Wire.beginTransmission(I2C_ADDRESS);
  bool result = (Wire.endTransmission() == 0);  // is OLED fitted
  Wire.end();

  if (result) {
    oled.begin(&Adafruit128x32, I2C_ADDRESS);  // Initialise OLED
    delay(1);                                  // some hardware is slow to initialise, and first call may not work
    oled.begin(&Adafruit128x32, I2C_ADDRESS);
    oled.setFont(fudged_Adafruit5x7);  // original Adafruit5x7 font
    oled.clear();
    oled.print(F("ver"));
    oled.println(F(VERSION));
  }
  return result;
}
#endif


// -------------------------------------------------------------------------------------
// *** Some Useful Links ***
// ZX spectrum: https://mdfs.net/Docs/Comp/Spectrum/SpecIO
// Arduino    : https://devboards.info/boards/arduino-nano
//              https://arduino.stackexchange.com/questions/30968/how-do-interrupts-work-on-the-arduino-uno-and-similar-boards
// -------------------------------------------------------------------------------------

/* DEBUG EXAMPLE TO SPECTRUM SCREEN
uint16_t destAddr;
char _c[32];
for (uint8_t y = 0; y < 8; y++) {
  destAddr = Utils::zx_spectrum_screen_address(128, y);
  FILL_COMMAND(packetBuffer, 128 / 8, destAddr, 0x00);
  Z80Bus::sendBytes(packetBuffer, 6);
}
destAddr = 0x5800 + (128 / 8);
FILL_COMMAND(packetBuffer, 128 / 8, destAddr, B01000100);
Z80Bus::sendBytes(packetBuffer, 6);
sprintf(_c, "Delay:%d seconds", _delay / (1000 / 20));
Draw::text(256 - 128, 0, _c);
*/
