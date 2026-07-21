#include "IngamePauseMenu.h"

#include <Arduino.h>

#include "BufferManager.h"
#include "SdCardSupport.h"
#include "Z80Bus.h"
#include "draw.h"
#include "menu.h"
#include "pin.h"
#include "utils.h"
#include "z80bus.h"
#include "PacketTypes.h"


constexpr uint16_t PM_INITIAL_DELAY = 380;
constexpr uint16_t PM_REPEAT_RATE = 120;
constexpr uint8_t PAUSE_XPOS = 80;
constexpr uint8_t PAUSE_YPOS_START = 48;
constexpr uint8_t HIGHLIGHT_OFFSET = PAUSE_YPOS_START / 8; 

constexpr uint8_t getY(int8_t line) { 
    return PAUSE_YPOS_START + (line * 8); 
}

#ifndef FPSTR
#define FPSTR(pstr) (reinterpret_cast<const __FlashStringHelper *>(pstr))
#endif

static const char SAVED_MSG[] PROGMEM = "SAVED";

// ----------------------------------------------------------------------------------------------
// HARDWARE SYNC: /NMI TRIGGER LOGIC TO AVOID CORRUPTION
// ----------------------------------------------------------------------------------------------
// PCB UPDATE:   (double duty for ShiftRegClockPin)
// Nano A2 monitors /RD and /IORQ via a passive resistor logic gate (4.7K per
// line). This acts as a hardware AND gate for active-low signals: A2 only hits
// a Logic LOW when both Z80 lines are active, pinpointing the I/O Read cycle.
// WHY: Most games poll input (IN) when the stack is stable, so using NMI
// directly after seeing a I/O read means the stack is unlikely to be hijacked.
//
// ----------------------------------------------------------------------------------------------
// waitForUserExit: Processes joystick / PCB button
//    Each frame the 'PORTD' Port register is updated with fresh joystick data.
//    (Kempston joy data hits data bus on: IORQ,RD,A7; all LOW)
// ----------------------------------------------------------------------------------------------
void InGamePauseMenu::waitForUserExit(uint8_t borderColour) {
  Utils::readJoystick();  // flush junk (Z80 rd/wr port 0x1f shared)

  while (true) {
    uint8_t buttonData = Utils::readJoystick();

    // Remap 2nd fire to jump (Todo: Needs config mapping framework)
    if (buttonData & INPUT_FIRE2) {
      buttonData |= 0b00001000;  // up  (BITS: XsfFUDLR)
    }

    PORTD = buttonData & INPUT_MASK;

    if (buttonData & INPUT_SELECT) {
      if (process(borderColour)) {
        break;  // back to game loader menu
      }
    }
  }
}

uint8_t InGamePauseMenu::getSelectedMenuOption_Blocking(uint8_t& selectedIndex) {

  unsigned long lastActionTime = 0;
  bool isRepeating = false;
  Menu::Button_t lastButton = Menu::BUTTON_NONE;

  while (true) {
    Utils::show5VoltRailStatus();

    Menu::Button_t currentButton = Menu::getButton();
    if (currentButton == Menu::BUTTON_NONE) {
      lastButton = Menu::BUTTON_NONE;
      isRepeating = false;
    } else {
      const unsigned long now = millis();
      const uint16_t delayThreshold = isRepeating ? PM_REPEAT_RATE : PM_INITIAL_DELAY;

      if (currentButton != lastButton || (now - lastActionTime) >= delayThreshold) {
        isRepeating = (currentButton == lastButton); 
        lastActionTime = now;
        lastButton = currentButton;

        if (currentButton == Menu::BUTTON_ADVANCE && selectedIndex < EXIT) {
          Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START + ((selectedIndex + HIGHLIGHT_OFFSET) * 32),
                                  32, COL::BRIGHT_BLACK_WHITE);
          selectedIndex++;
        } else if (currentButton == Menu::BUTTON_BACK &&
                   selectedIndex > RESUME) {
          Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START + ((selectedIndex + HIGHLIGHT_OFFSET) * 32),
                                  32, COL::BRIGHT_BLACK_WHITE);
          selectedIndex--;
        } else if (currentButton == Menu::BUTTON_MENU) {
          return selectedIndex;
        }
      }
    }
    Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START + ((selectedIndex + HIGHLIGHT_OFFSET) * 32), 32, COL::CYAN_BLACK);
    Utils::delay16(1);
  }
}

bool InGamePauseMenu::process(uint8_t borderColour) {
  uint8_t selectedIndex = 0;

  // Double Duty : Temporary Grab 'ShiftRegClockPin' as INPUT to monitor Z80's
  // combined /RD AND /IORQ lines
  pinModeFast(Pin::ShiftRegClockPin, INPUT);  // A2

  // Using Inline ASM for cycle-accurate timing.
  // Equivalent to:
  //   while (digitalRead(A2) == HIGH); // Wait for external signal on A2
  //   NMI_LOW(); NMI_HIGH();           // Pulse NMI on A0 immediately
  asm volatile(
      "1: sbic %[pin],2   \n\t"  // Check A2 (PC2); skip next instruction if LOW
      "rjmp 1b            \n\t"  // Jump back to '1' (Loop while A2 is HIGH)
      "cbi  %[port],0     \n\t"  // Drive A0 (PC0) LOW (Start NMI pulse)
      "sbi  %[port],0     \n\t"  // Drive A0 (PC0) HIGH (End NMI pulse)
      :
      : [pin] "I"(_SFR_IO_ADDR(PINC)), [port] "I"(_SFR_IO_ADDR(PORTC)));

  pinModeFast(Pin::ShiftRegClockPin, OUTPUT);  // back to normal clock use

  //------------------------------------------------------------------------
  // At this point we are running of the stock ROM
  // Allow NMI routine time to reach it's idle loop
  Utils::delay16(10);  // (way more than needed)
  //------------------------------------------------------------------------
  // Swap out to use SNA ROM, it takes over with NOPs
  Z80Bus::setSnaRom();
  //------------------------------------------------------------------------
  // Wait for Z80 to hit SNA ROM's '.IngameHook'
  Utils::delay16(10);  // (way more than needed)
  //------------------------------------------------------------------------

  // storeZ80States allocates memory, and restoreZ80States frees it.
  // If exiting without calling restoreZ80States, free structs marker manually.
  Z80Registers* z80Registers = Utils::storeZ80States();
   // borderColour: using original snapshot loaded value as we can't extract this at game time!
  z80Registers->borderCol = borderColour; 

  //  Get filename before we destroy the shared working FatFile
  static char cacheDirName[ZX_FILENAME_MAX_DISPLAY_LEN+1];
  if (cacheDirName[0] == '\0') {
    uint8_t len = SdCardSupport::getFile().getDisplayName7(cacheDirName, ZX_FILENAME_MAX_DISPLAY_LEN);
    cacheDirName[len - 4] = '\0';  // knock off the ".sna"
  }

   // Save screen to scratch file
  Utils::saveMemory(SCRATCH_FILE, ZX_SCREEN_ADDRESS_START, ZX_SCREEN_BITMAP_SIZE + ZX_SCREEN_ATTR_SIZE);
  
  uint8_t result;
  do {
    Utils::clearScreen(COL::BRIGHT_BLACK_WHITE);
    Draw::text_P(PAUSE_XPOS, getY(-2), F("PAUSE MENU"));
    Draw::text_P(PAUSE_XPOS, getY(RESUME), F("Resume"));
    Draw::text_P(PAUSE_XPOS, getY(SAVE_SNA), F("Save"));
    Draw::text_P(PAUSE_XPOS, getY(POKE), F("Poke"));
    Draw::text_P(PAUSE_XPOS, getY(SCREENSHOT), F("Screenshot"));
    Draw::text_P(PAUSE_XPOS, getY(MEM_VIEW), F("Mem View"));
    Draw::text_P(PAUSE_XPOS, getY(EXIT), F("Exit"));

    result = getSelectedMenuOption_Blocking(selectedIndex);

    Utils::clearScreen(COL::BRIGHT_BLACK_WHITE);
    Menu::waitForRelease();

    switch (result) {
      case SAVE_SNA:
        handleSaveSnapshot(z80Registers, cacheDirName);
        break;
      case POKE:
        handlePokeMenu();
        break;
      case MEM_VIEW:
        Utils::viewSpeccyMemory();
        break;
      case SCREENSHOT:
        handleScreenshotMenu();
        break;
      case RESUME:
        Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START, ZX_SCREEN_ATTR_SIZE, COL::BLACK_BLACK);
        break;
      case EXIT:
        Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START, ZX_SCREEN_ATTR_SIZE, COL::BLACK_BLACK);
        BufferManager::freeToMark(z80Registers->AllocMark);
        // reseting cache - to trigger new collection
        cacheDirName[0] = '\0'; 
        return true;  // Exit game back to launcher
    }
  } while ( result != RESUME && result != EXIT);

  Menu::waitForRelease();
  Utils::restorePauseMenuScreen();
  Utils::restoreZ80States(z80Registers);
  // Give Z80 time to reach the next idle loop so the stock ROM can take control.
  Utils::delay16(1);               
  // About to restore the stock ROM and continue game
  // We need to put something useful on the output pins for joystick port 0x1F.
  PORTD = Utils::readJoystick() & INPUT_MASK; 
  // Stock rom escapes the idle loop via its NOPs
  Z80Bus::setStockRom();  
  // let the stock rom catch up
  Utils::delay16(1);       
  Utils::readJoystick();  // flush junk

  return false;
}

void InGamePauseMenu::handlePokeMenu() {
  Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START + (32*(64/8)), 32*(64/8), COL::BLACK_WHITE);
  Draw::text_P(128 - ((21*6)/2), 40, F("Enter address & value"));
  Draw::text_P(0, 48, F("e.g. JetPac: Infinite Lives, Poke: 25014,0"));
  
  uint8_t x = 144;
  uint8_t y = 64 + 64;
  Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START + (((y+0) / 8) * 32) + (x / 8), 14, COL::BRIGHT_CYAN_BLACK);
  Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START + (((y+8) / 8) * 32) + (x / 8), 14, COL::MAGENTA_BLACK);
  Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START + (((y+16)/ 8) * 32) + (x / 8), 14, COL::GREEN_BLACK);
  
  Draw::text_P(x, y + 0,  F("[Space] Abort"));
  Draw::text_P(x, y + 8,  F("[Sym shft] Delete"));
  Draw::text_P(x, y + 16, F("[Enter] Apply"));
  Draw::text_P(128, 192 / 2, F(","));
  Draw::text_P(128 + 8, 192 / 2, F("___"));

  int32_t addr = readNumericInput(5, 64, 192 / 2, "Poke:", 0x4000, 0xffff);
  int32_t value = readNumericInput(3, 128, 192 / 2, ",", 0, 255);

  if (addr != -1 && value != -1) {
    Poke_Packet pkt(addr, value);
    Z80Bus::sendBytes(reinterpret_cast<uint8_t*>(&pkt), sizeof(Poke_Packet));
  }
}

 
int32_t InGamePauseMenu::readNumericInput(uint8_t maxDigits, int xPos, int yPos, const char* name, uint16_t min, uint16_t max) {
  Draw::text(xPos, yPos, name);

  xPos += strlen(name) * (SmallFont::FNT_WIDTH + SmallFont::FNT_GAP);
  xPos = (xPos + 7) & ~7; // Size Optimization: Replaces ((xPos + 7) / 8) * 8

  uint8_t mark = BufferManager::getMark();
  char* buf = (char*)BufferManager::allocate(maxDigits + 1);

  while (true) {
    memset(buf, ' ', maxDigits);
    buf[maxDigits] = '\0';

    uint8_t len = 0;
    uint8_t flash = 0;
    uint8_t prevKey = 0;

    // Inner loop handles text entry
    while (true) {
      uint8_t key = Z80Bus::getKeyboard();
      if (key != prevKey) {
        prevKey = key;
        if (key != 0) {
          if (key == 0x20) { // CANCEL key (Space) - Exit immediately
            BufferManager::freeToMark(mark);
            return -1;
          }
          if (key == 0x0D && len > 0 && buf[0] != ' ') { // ENTER key
            break; 
          }
          if (key == 0x02 && len > 0) { // DEL key
            buf[--len] = ' ';
            flash = 0;
          } 
          else if (key >= '0' && key <= '9' && len < maxDigits) { // Digit entry
            buf[len++] = key;
            flash = 0;
          }
        }
      }

      uint8_t cursorIdx = (len >= maxDigits) ? (maxDigits - 1) : len;
      char origChar = buf[cursorIdx];
      if (flash < 60) {  
        buf[cursorIdx] = 127; // Blink pattern 
      }

      Draw::text(xPos, yPos, buf);
      buf[cursorIdx] = origChar; // Restore after draw
      
      if (++flash >= 120) flash = 0;
      Utils::delay16(1);
    }
   
    Draw::text(xPos, yPos, buf); // Clear cursor

    // Convert text to value
    uint16_t value = 0;   
    for (uint8_t i = 0; i < len; i++) {
      value = (value * 10) + (buf[i] - '0');
    }
    
    while (Z80Bus::getKeyboard() != 0); 

    if (value >= min && value <= max) {
      BufferManager::freeToMark(mark);
      return value; // Success path: cleanup memory and exit function
    }

    uint16_t targetAddr = ZX_SCREEN_ATTR_ADDRESS_START + ((yPos & ~7) << 2) + (xPos >> 3);
    uint8_t widthCells = ((maxDigits * 6) + 7) >> 3;

    Z80Bus::sendFillCommand(targetAddr, widthCells, COL::FLASH_RED_WHITE);
    Utils::delay16(1100);
    Z80Bus::sendFillCommand(targetAddr, widthCells, COL::BRIGHT_BLACK_WHITE);
  }
}

void InGamePauseMenu::handleScreenshotMenu() {
  // Restore the Speccy's screen from the scratch file to show the user something is happening
  Utils::restorePauseMenuScreen();

  // copyScratchTo() uses SdCardSupport's internal file (FatFile) so keep it available!
  FatFile dir;
  static char filename[] = "Shot0000.SCR";  // will be modified
  if ( SdCardSupport::openOrCreateDirectory(dir, SHOTS_FOLDER) &&  SdCardSupport::findFreeFilename(dir, filename)) {
    Utils::clearTopBar();
    Draw::text(0, 0, filename);
    if (SdCardSupport::copyScratchTo(dir, filename)) {
        Draw::text_P(256-32,0, FPSTR(SAVED_MSG));
        Utils::delay16(2200);
    }
  }
  dir.close();
}

//static constexpr uint16_t HEAD_ID = 0xC0DE;
struct __attribute__ ((packed))  GameMeta {
//  uint16_t  head_id;   
  uint8_t   version;  // future thing : not used yet
  char      gameName[ZX_FILENAME_MAX_DISPLAY_LEN+1];
};

void InGamePauseMenu::handleSaveSnapshot(Z80Registers* z80Registers, const char* dirName) {
  Utils::restorePauseMenuScreen();
  FatFile dir, metaFile;
  GameMeta meta;
  bool hasMeta = false;

  // Look for GAMEINFO.DAT in the currently navigated folder
  SdCardSupport::syncRootToDepth();
  if (metaFile.open(&SdCardSupport::getRoot(), GAMEINFO_DAT, O_READ)) {
    hasMeta = true; 
    metaFile.read(&meta, sizeof(meta));
    metaFile.close();
  }

  SdCardSupport::reopenRoot();

  if (hasMeta) {
    // STRICT MODE: We have a known location - attempt to open it
    if (!dir.open(&SdCardSupport::getRoot(), meta.gameName, O_READ)) {
      return;  // Abort
    }
  } else {
    // CREATE MODE: No .dat file exists yet
    if (!SdCardSupport::openOrCreateDirectory(dir, dirName)) {
      return;  // Abort
    }

    // Write new metadata inside new folder
    if (metaFile.open(&dir, GAMEINFO_DAT, O_RDWR | O_CREAT)) {
      //     meta.head_id = HEAD_ID;
      meta.version = 1;
      strncpy(meta.gameName, dirName, sizeof(meta.gameName));
      metaFile.write(&meta, sizeof(meta));

      // hide dat file
      int currentAttribs = metaFile.attrib();
      if (currentAttribs != -1) {
        metaFile.attrib(currentAttribs | FS_ATTRIB_HIDDEN);
      }

      metaFile.close();
    }
  }

  // Find a free slot inside our opened directory and dump the snapshot
  static char saveName[13] = "Snap0000.SNA";
  if (SdCardSupport::findFreeFilename(dir, saveName)) {
    Utils::dumpMemoryAsSnapshot(z80Registers, saveName, dir);
    Utils::clearTopBar();
    Draw::text(0, 0, saveName);
    Draw::text_P(256-32, 0, FPSTR(SAVED_MSG));
    Utils::delay16(2000);
  }
  
  dir.close();
}
