// -------------------------------------------------------------------------------------
// This is an Arduino-Based ZX Spectrum Game Loader (by Paul Overy)
// -------------------------------------------------------------------------------------
// This software uses the Arduino's ATmega328P Nano (2K SRAM, 32K flash & 1K EEPROM)
// IMPORTANT: Do not modify PORTB directly without preserving the clock/crystal bits
// -------------------------------------------------------------------------------------
// For ref: My default compiler optimise setting is located around here (optimise='-O2'):-
//          C:\Users\Admin\AppData\Local\Arduino15\packages\arduino\hardware\avr\1.8.8\platform.txt
//          I like to override with "__attribute__((optimize("...")))"
// -------------------------------------------------------------------------------------

// To upload via an external programmer instead of USB. Useful if the USB interface fails, and
// saves flash memory (though the code currently fits onto the Nano with plenty of room to spare).
//see https://www.youtube.com/watch?v=ToKerwRR-70 for a quick start guide.
//https://zadig.akeo.ie/  (for drivers - I used winusb )


#include <Arduino.h>
#include "Utils.h"
#include "Menu.h"
#include "InGamePauseMenu.h"
#include "Draw.h"
#include "SdCardSupport.h"
#include "BufferManager.h"
#include "Z80Bus.h"
#include "SnapZ80.h"
#include "Debug.h"  // see #defines to enable


__attribute__((optimize("-Os")))   
void setup() {

#ifdef SERIAL_DEBUG 
  Debug::setupSerial(); // For debugging
#endif

  Z80Bus::setupPins();
  Z80Bus::resetZ80();
  Utils::setupJoystick();

#ifdef DEBUG_OLED
  Debug::setupOled(); // For debugging
#endif

  // Use stock ROM when select button or fire held at power up
  if (Utils::readJoystick() & (INPUT_FIRE | INPUT_SELECT)) {
    Utils::stockRomBoot_Blocking();  // user pressing select again will exit
  }

  // Display the version in the bottom right corner (on a Cyan background)
  Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START, ZX_SCREEN_ATTR_SIZE, COL::Paper5Ink0);
  Draw::text_P(256 - 24, 192 - 8, F(VERSION));

  if (!Utils::isSdCardOK_Blocking()) {
     Utils::clearScreen(COL::BLACK_WHITE);  // clear "INSERT SD CARD" message
  };
}

void loop() {
  FatFile* pFile = Menu::handleMenu();
  const char* fileName = SdCardSupport::getFileName(pFile);
  if (strcasestr(fileName, ".scr")) {
    handleScrFile(pFile);
  } else if (strcasestr(fileName, ".sna")) {
    handleSnaFile(pFile);
  } else if (strcasestr(fileName, ".z80")) {
    handleZ80File(pFile);
  } else if (strcasestr(fileName, ".txt")) {
    handleTxtFile(pFile);
  }
  pFile->close();
}

// ---------------------
// .SCR FILE 
// ---------------------
void handleScrFile(FatFile* pFile) {
  Utils::clearScreen(0);
  if (pFile->fileSize() == ZX_SCREEN_TOTAL_SIZE) {
    Z80Bus::transferSnaData(pFile, false);  // No loading effects.
    Menu::waitForAnyKey();
    Utils::clearScreen(0);
  }
}

// -----------------------------------------------------------------------------------------------
// NOTES: Exiting the menu and restoring the snapshot. Before doing this, we must relocate 
// the Z80 stack to a safe working area. The menu defaults to 0xFFFF, which is a bad idea while loading a game.
// We set a temporary stack in screen memory (0x4000). This is safe because:
// - 0x4000 contains the Z80 "jp <patched-start-addr>" bootstrap instruction. 
// - We reuse 0x4003 for temporary 1-deep push/pop (at 0x4001/02).
// This temporary SP is later set by the load process to the games correct stack.
//
// We can safely (99.999%) set the SP at screen location during loading and simply sacrifice/corrupt 3 bytes of screen data.
// -----------------------------------------------------------------------------------------------

// ---------------------
// .SNA FILE 
// ---------------------
void handleSnaFile(FatFile* pFile) {
  Utils::clearScreen(0);
  if (pFile->fileSize() == SNAPSHOT_FILE_SIZE) {
    // Set stack NOW before sending data over
    Z80Bus::setStackCommand(ZX_SCREEN_ADDRESS_START + 3); 
    uint8_t mark = BufferManager::getMark();
    {
      uint8_t *snaHeaderPacket = BufferManager::allocate(SNA_TOTAL_ITEMS);
      pFile->read((void *)(snaHeaderPacket), SNA_TOTAL_ITEMS);
      Z80Bus::transferSnaData(pFile, true);
      Z80Bus::executeSnapshot(snaHeaderPacket);
    }
    BufferManager::freeToMark(mark);
    InGamePauseMenu::waitForUserExit();
    Z80Bus::setSnaRom();
    Z80Bus::resetZ80();
  }
}

// ---------------------
// .Z80 FILE 
// ---------------------
void handleZ80File(FatFile* pFile) {

  Utils::clearScreen(0);
  SnapZ80::Z80HeaderInfo headerInfo;
  uint8_t ver = readZ80Header(pFile, &headerInfo);

  if (ver != Z80_VERSION_UNKNOWN) {
    if (checkZ80FileValidity(pFile, &headerInfo)) {
      // Set stack NOW before sending data over
      Z80Bus::setStackCommand(ZX_SCREEN_ADDRESS_START + 3);  
      uint8_t mark = BufferManager::getMark();
      {
        uint8_t *snaHeaderPacket = BufferManager::allocate(SNA_TOTAL_ITEMS);
        SnapZ80::convertSendZ80toSNA(pFile, &headerInfo, snaHeaderPacket);
        Z80Bus::executeSnapshot(snaHeaderPacket);
      }
      BufferManager::freeToMark(mark);
      InGamePauseMenu::waitForUserExit();
      Z80Bus::setSnaRom();
      Z80Bus::resetZ80();
      return;  // load OK
    }
  }

  // drop down to report load error
  Utils::clearScreen(COL::BLACK_WHITE);
  Draw::text_P(0, 40, F("Can't load:"));
  Draw::text(0, 50, SdCardSupport::getFileName(pFile));
  if (SnapZ80::getMachineDetails(headerInfo.version, headerInfo.hw_mode) == MACHINE_128K) {
    Draw::text_P(80, 90, F("128K not supported (yet)"));
  }
  Menu::waitForAnyKey();
}

// ---------------------
// .TXT FILE
// ---------------------
__attribute__((optimize("-Os")))
void handleTxtFile(FatFile* pFile) {
  constexpr uint8_t charHeight = SmallFont::FNT_HEIGHT + SmallFont::FNT_GAP;
  constexpr uint8_t maxLines = ZX_SCREEN_HEIGHT_PIXELS / charHeight;
  constexpr uint8_t maxChars = ZX_SCREEN_WIDTH_PIXELS / SmallFont::FNT_CHAR_PITCH;

  uint16_t mark = BufferManager::getMark();
  char* buf = (char*)BufferManager::allocate(maxChars + 1);
  
  uint16_t currentPage = 0;
  uint32_t pageStartPos = 0; 

  while (true) {
    Utils::clearScreen(COL::BLACK_WHITE);
    pFile->seekSet(pageStartPos);
    
    uint8_t y = 0;
    for (uint8_t i = 0; i < maxLines; i++) {
      uint8_t len = 0;
      if (pFile->available()) {
        len = Utils::readLineTxt(pFile, buf, maxChars);
        if (!len) { buf[0] = ' '; len = 1; }
      }
      buf[len] = 0;
      Draw::textLine(y, buf);
      y += charHeight; // Faster/smaller than i * charH
    }

    uint32_t nextPos = pFile->curPosition();
    bool canFwd = pFile->available();

    // Block until button press
    Menu::Button_t btn;
    while ((btn = Menu::getButton()) == Menu::BUTTON_NONE);

    if (btn == Menu::BUTTON_MENU) break;
    if (btn == Menu::BUTTON_ADVANCE && canFwd) {
      pageStartPos = nextPos;
      currentPage++;
    } 
    else if (btn == Menu::BUTTON_BACK && currentPage > 0) {
      currentPage--;
      pageStartPos = 0; // Re-scan logic
      for (uint16_t p = 0; p < currentPage; p++) {
        pFile->seekSet(pageStartPos);
        for (uint8_t l = 0; l < maxLines; l++) {
          Utils::readLineTxt(pFile, nullptr, maxChars); 
        }
        pageStartPos = pFile->curPosition();
      }
    }

    const uint32_t wait = millis() + 350;
    while (Menu::getButton() != Menu::BUTTON_NONE && millis() < wait);
  }

  BufferManager::freeToMark(mark);
  Menu::waitForRelease();
}


// #ifdef SERIAL_DEBUG
//   void setupSerial() {
//     Serial.begin(9600);
//     while (!Serial) {};
//     Serial.println("SERIAL DEBUG BREAKS COMMS TO Z80");
//   }
// #endif


// #ifdef DEBUG_OLED
// __attribute__((optimize("-Os")))
// bool setupOled() {  // DEBUG USE ONLY
//   Wire.begin(); 
//   Wire.beginTransmission(I2C_ADDRESS);
//   bool result = (Wire.endTransmission() == 0);  // is OLED fitted
//   Wire.end();

//   if (result) {
//     oled.begin(&Adafruit128x32, I2C_ADDRESS);  // Initialise OLED
//     delay(1);                                  // some hardware is slow to initialise, and first call may not work
//     oled.begin(&Adafruit128x32, I2C_ADDRESS);
//     oled.setFont(fudged_Adafruit5x7);  // original Adafruit5x7 font
//     oled.clear();
//     oled.print(F("ver"));
//     oled.println(F(VERSION));
//   }
//   return result;
// }
// #endif


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
sprintf(_c, "Delay:%d seconds", _delay / (1000 / 20));  // eats flash memory
Draw::text(256 - 128, 0, _c);
*/
