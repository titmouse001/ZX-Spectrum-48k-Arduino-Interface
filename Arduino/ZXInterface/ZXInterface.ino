// This is an Arduino-Based ZX Spectrum Game Loader (by Paul Overy)
//
// This software uses the Arduino's ATmega328P Nano (2K SRAM, 32K flash & 1K EEPROM)
// IMPORTANT: Do not modify PORTB directly without preserving the clock/crystal bits
//
// -------------------------------------------------------------------------------------
// To upload via an external programmer instead of USB. Useful if the USB interface fails, and
// saves flash memory (though the code currently fits onto the Nano with some room to spare).
// see https://www.youtube.com/watch?v=ToKerwRR-70 for a quick start guide.
// https://zadig.akeo.ie/  (for drivers - I used winusb )

// For cheaper clone boards like ATmega328PB (not ATmega328P using standard options "Arduino AVR Boards" -> "Arduino Nano" )
// Tools > Board > MiniCore -> select ATmega328
// Settings -> board: 115200, "BOD 2.7v", Yes (UART0), External 16MHz, EEPROM retianed, LTO enabled, "328PB"
// Make sure programmer is set to USBaspm,
// then -> "upload using programmer"

// -------------------------------------------------------------------------------------

// see README.MD for more info

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

void setup() {

#ifdef SERIAL_DEBUG 
  Debug::setupSerial(); // For debugging
#endif

  Utils::resetSystem();

#ifdef DEBUG_OLED
  Debug::setupOled(); // For debugging
#endif

  // Use stock ROM when select button or fire held at power up
  if (Utils::readJoystick() & (INPUT_FIRE2 | INPUT_SELECT)) {
    Utils::stockRomBoot_Blocking();  // user pressing select again will exit
  }

  // Display the version (remove sd card to view version)
  Utils::clearScreen(COL::CYAN_BLACK); 
  Draw::text_P(256 - 24, 192 - 8, F(VERSION));

  Utils::waitForSDCard_Blocking(); // When blocking shows - "INSERT SD CARD"
}

void loop() {
  
  FatFile* pFile = Menu::handleMenu();
  char extBuff[4];
  pFile->getExtension((char*)&extBuff, sizeof(extBuff));
  char* ext = extBuff;

  if (ext) {
    if (strcasecmp(ext, "scr") == 0) {
      handleScrFile(pFile);
    } else if (strcasecmp(ext, "sna") == 0) {
      handleSnaFile(pFile);
    } else if (strcasecmp(ext, "z80") == 0) {
      handleZ80File(pFile);
    } else if (strcasecmp(ext, "txt") == 0) {
      handleTxtFile(pFile);
    }
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
    Menu::waitForRelease();
    Menu::waitForAnyKey();
    Utils::clearScreen(0);
  }
}

// -----------------------------------------------------------------------------------------------
// NOTES: Exiting the menu and restoring the snapshot. Before doing this, we must relocate 
// the Z80 stack to a safe working area. The menu defaults to 0xFFFF, which is a bad idea while loading a game.
// We set a temporary stack in screen memory (0x4000). This is safe because:
// - 0x4000 contains the Z80 "jp <patched-start-addr>" bootstrap instruction.       (JP = [1] byte)
// - We reuse 0x4003 for temporary 1-deep push/pop (at 0x4001/02).                  (STACK = [2] bytes)
// This temporary SP is later set by the load process to the games correct stack.
//
// We can safely (99.999%) set the SP at screen location during loading and simply sacrifice/corrupt [2] + [1] bytes.
// -----------------------------------------------------------------------------------------------

// ---------------------
// .SNA FILE 
// ---------------------
void handleSnaFile(FatFile* pFile) {
  uint8_t borderColour;
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
      borderColour =  snaHeaderPacket[SNA_BORDER_COLOUR];
    } 
    BufferManager::freeToMark(mark);
    InGamePauseMenu::waitForUserExit(borderColour);
    Z80Bus::setSnaRom();
    Z80Bus::resetZ80();
  }
}

// ---------------------
// .Z80 FILE 
// ---------------------
void handleZ80File(FatFile* pFile) {
  uint8_t borderColour;
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
        borderColour =  snaHeaderPacket[SNA_BORDER_COLOUR];
      }
      BufferManager::freeToMark(mark);
      InGamePauseMenu::waitForUserExit(borderColour);
      Z80Bus::setSnaRom();
      Z80Bus::resetZ80();
      return;  // load OK
    }
  }

  // drop down to report load error
  Utils::clearScreen(COL::BLACK_WHITE);
  Draw::text_P(0, 40, F("Can't load:"));

  char* buf = (char*)BufferManager::allocate(ZX_FILENAME_MAX_DISPLAY_LEN+1);
  uint8_t mark = BufferManager::getMark();
  pFile->getDisplayName7(buf,ZX_FILENAME_MAX_DISPLAY_LEN );
  Draw::text(0, 50,  buf);
  BufferManager::freeToMark(mark);

  if (SnapZ80::getMachineDetails(headerInfo.version, headerInfo.hw_mode) == MACHINE_128K) {
    Draw::text_P(80, 90, F("128K not supported (yet)"));
  }
  Menu::waitForAnyKey();
}

// ---------------------
// .TXT FILE
// ---------------------
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
  
    Menu::waitForRelease();
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



// -------------------------------------------------------------------------------------
// *** Some Useful Links ***
// ZX spectrum: https://mdfs.net/Docs/Comp/Spectrum/SpecIO
// Arduino    : https://devboards.info/boards/arduino-nano
//              https://arduino.stackexchange.com/questions/30968/how-do-interrupts-work-on-the-arduino-uno-and-similar-boards
// -------------------------------------------------------------------------------------

// Generate Map file
// >avr-nm -S --size-sort -t d C:\Users\Admin\Documents\GitHub\ZX-Spectrum-48k-Arduino-Interface\build.tmp\ZXInterface.ino.elf >c:\temp\2.txt
