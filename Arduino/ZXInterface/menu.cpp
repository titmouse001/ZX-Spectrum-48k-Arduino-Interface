#include "Menu.h"
#include "Constants.h"
#include "Utils.h"
#include "SdCardSupport.h"
#include "BufferManager.h"
#include "draw.h"
#include "Z80Bus.h"

#include "PacketTypes.h"



uint32_t Menu::lastButtonPressTime = 0;
uint32_t Menu::lastButtonHoldTime = 0;
uint16_t Menu::buttonDelay = MAX_REPEAT_KEY_DELAY;
uint16_t Menu::currentFileIndex = 0;
uint16_t Menu::startFileIndex = 0;
bool     Menu::buttonHeld = false;
bool     Menu::inSubFolder = false;

constexpr uint8_t MENU_TEXT_COLOUR = COL::BRIGHT_BLACK_WHITE;
constexpr uint8_t MENU_DIR_COLOUR = COL::BRIGHT_BLACK_CYAN;

// Cache highlight bars without re‑reading SD.
static uint8_t rowBaseColourCache[SCREEN_TEXT_ROWS]; 

FatFile* Menu::handleMenu() {
  SdCardSupport::syncRootToDepth();    // Re-synchronize the menu
  Utils::clearScreen(MENU_TEXT_COLOUR);
  uint16_t totalFiles = scanFolder();

  // did the folder contents grow
  uint16_t virtualTotalFiles = totalFiles + (inSubFolder ? 1 : 0);
  if (virtualTotalFiles > 0 && currentFileIndex >= virtualTotalFiles) {
    currentFileIndex = virtualTotalFiles - 1;
    startFileIndex = (currentFileIndex / SCREEN_TEXT_ROWS) * SCREEN_TEXT_ROWS;
  }

  drawFileList(); //(true);
  drawHighlightBar();

  while (true) {

#ifdef _DEBUG_POOL_SIZE_ENABLED_
    Utils::clearScreen(MENU_TEXT_COLOUR);
    char _c[8];
    itoa(BufferManager::poolOffsetLastMax, _c, 10);
    Draw::text(0, 0, _c);
    itoa(BufferManager::poolOffset, _c, 10);
    Draw::text(0, 16, _c);
#endif

    //const uint32_t start = millis();
    uint16_t lastIndex = currentFileIndex;
    MenuAction_t action = getMenuAction(totalFiles);

    // if (!SdCardSupport::isInserted()) {
    //   static constexpr uint8_t clrScreenFlag(true);
    //   Utils::waitForSDCard_Blocking(clrScreenFlag);  // shows message "INSERT SD CARD"
    //   action = ACTION_REFRESH_LIST;
    // }

    if (!SdCardSupport::isInserted()) {
      Utils::waitForSDCard_Blocking(true);
      SdCardSupport::resetNavigation();        // depth + history + inSubFolder
   //   currentFileIndex = 0;
   //   startFileIndex   = 0;
      buttonHeld       = false;
      buttonDelay      = MAX_REPEAT_KEY_DELAY;
      SdCardSupport::syncRootToDepth();        // re-anchor root on the new card
      Utils::clearScreen(MENU_TEXT_COLOUR);
      totalFiles = scanFolder(true);           // re-count
      action     = ACTION_REFRESH_LIST;
    }


    if (action == ACTION_SELECT_FILE) {
      bool backOneLevel = (inSubFolder && currentFileIndex == 0);  // Inside folder - first item shows "[/]"

      if (backOneLevel) {
        if (SdCardSupport::menuPathDepth > 0) {
          SdCardSupport::menuPathDepth--;
          uint16_t exitedFolderIndex = SdCardSupport::menuPathHistory[SdCardSupport::menuPathDepth];
          
          SdCardSupport::syncRootToDepth();
          totalFiles = scanFolder(false);  
          
          // back to the folder we just exited
          currentFileIndex = inSubFolder ? (exitedFolderIndex + 1) : exitedFolderIndex;
          startFileIndex = (currentFileIndex / SCREEN_TEXT_ROWS) * SCREEN_TEXT_ROWS;
        }
      } else {
        // Regular File or Folder selection
        const uint16_t selectedIndex = inSubFolder ? currentFileIndex - 1 : currentFileIndex;
        FatFile* file = SdCardSupport::openFileByIndex(selectedIndex);
        if (file->isDir()) {
          // Check if we are allowed to go deeper
          if (SdCardSupport::menuPathDepth < FOLDER_NAV_DEPTH) { 
            SdCardSupport::menuPathHistory[SdCardSupport::menuPathDepth] = selectedIndex;
            SdCardSupport::menuPathDepth++;
            SdCardSupport::syncRootToDepth();
            totalFiles = scanFolder(true);  // new subfolder
          } else {
            file->close();
            Menu::waitForRelease();
            continue;     // DO NOTHING! Max depth reached!
          }
        } else {
          return file;  // Selection confirmed. Handing execution off to game launcher.
        }
      }
      Menu::waitForRelease();
      Utils::clearScreen(MENU_TEXT_COLOUR); // clear old page
      drawFileList();//(true);  // full redraw
      drawHighlightBar();
      continue;
    }

    if (action == ACTION_REFRESH_LIST) {
      drawFileList();//(true);
      drawHighlightBar();
    } else if (action == ACTION_MOVE_DOWN || action == ACTION_MOVE_UP) {
      // Selection moved within the current page. Do not rescan the SD card.
      const uint16_t oldRow = lastIndex - startFileIndex;
      const uint16_t newRow = currentFileIndex - startFileIndex;

      if (oldRow < SCREEN_TEXT_ROWS) {
        Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START + (oldRow << 5), 32, rowBaseColourCache[oldRow]);
      }
      if (newRow < SCREEN_TEXT_ROWS) {
        Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START + (newRow << 5), 32, COL::CYAN_BLACK);
      }
    }
  //  Utils::delay16(MAX_BUTTON_READ_MILLISECONDS);  // aiming for 50 FPS'ish
  }
}


void Menu::drawFileList() {
    FatFile& root = SdCardSupport::getRoot();
    root.rewind();

    uint8_t  hi = 0x40;
    uint8_t  lo = 0x00;
    uint8_t  row = 0;

    uint16_t mark = BufferManager::getMark();
    char* textBuffer = (char*)BufferManager::allocate(ZX_FILENAME_MAX_DISPLAY_LEN + 1);
    uint16_t filesSkipped = 0;

    // Packet buffer (4-byte header + font payload)
    uint8_t *workingArea = BufferManager::allocate(4 + 224);

    // Send packet header with data
    const uint16_t commandAddr = cmd_addr(CMD_Copy32x7WithFixedStride);
    workingArea[0] = (uint8_t)(commandAddr >> 8);
    workingArea[1] = (uint8_t)(commandAddr);
    workingArea[2] = hi;
    workingArea[3] = lo;

    // Parent-directory indicator
    if (inSubFolder && startFileIndex == 0) {
        rowBaseColourCache[0] = COL::BRIGHT_BLACK_GREEN;
        Draw::textLine("[/]", workingArea);

        lo += 0x20;
        if (lo == 0) {
            hi += 8;
        }
        workingArea[2] = hi;
        workingArea[3] = lo;
        ++row;
    }

    uint16_t actualFilesToSkip = startFileIndex;
    if (inSubFolder && startFileIndex > 0) { 
        --actualFilesToSkip; 
    }

    FatFile& file = SdCardSupport::getFile();

    while (row < SCREEN_TEXT_ROWS && file.openNext(&root, O_RDONLY)) {
        if (!file.isHidden()) {
            if (filesSkipped < actualFilesToSkip) {
                ++filesSkipped;
            } else {
                const bool isDirectory = file.isDir();
                const uint8_t colour = isDirectory ? MENU_DIR_COLOUR : MENU_TEXT_COLOUR;
                rowBaseColourCache[row] = colour;

                uint8_t len = file.getDisplayName7(textBuffer, ZX_FILENAME_MAX_DISPLAY_LEN + 1);
                if (isDirectory && len < ZX_FILENAME_MAX_DISPLAY_LEN) {
                    textBuffer[len++] = '/';
                    textBuffer[len] = '\0';
                }

                Draw::textLine(textBuffer, workingArea);
                lo += 0x20;
                if (lo == 0) hi += 8;   // Third boundary check (speccy screen)
                workingArea[2] = hi;
                workingArea[3] = lo;
                ++row;
            }
        }
        file.close();
    }

    // Clear leftover rows
    if (row < SCREEN_TEXT_ROWS) {
        const uint8_t remaining = SCREEN_TEXT_ROWS - row;
        memset(&rowBaseColourCache[row], MENU_TEXT_COLOUR, remaining);
        
        do {
            Draw::textLine(((uint16_t)hi << 8) | lo, NULL);
            lo += 0x20;
            if (lo == 0) hi += 8;
        } while (++row < SCREEN_TEXT_ROWS);

        // need a fillwithStride
    }


    // Batch-send attribute fills using run-length encoding
    uint16_t attrAddr = ZX_SCREEN_ATTR_ADDRESS_START;
    uint8_t runStart = 0;
    uint8_t runColour = rowBaseColourCache[0];
    for (uint8_t i = 1; i < SCREEN_TEXT_ROWS; ++i) {
        if (rowBaseColourCache[i] != runColour) {
            const uint16_t runLength = (uint16_t)(i - runStart) << 5; 
            Z80Bus::sendFillCommand(attrAddr, runLength, runColour);
            attrAddr += runLength; // next starting fill address
            runStart = i;
            runColour = rowBaseColourCache[i];
        }
    }

    // Flush the final remaining run up to SCREEN_TEXT_ROWS
    const uint16_t finalRunLength = (uint16_t)(SCREEN_TEXT_ROWS - runStart) << 5;
    Z80Bus::sendFillCommand(attrAddr, finalRunLength, runColour);

    BufferManager::freeToMark(mark);
}

void Menu::goPage(bool forward, uint8_t row, uint16_t total) {
  if (total == 0) { startFileIndex = 0; currentFileIndex = 0; return; }
  if (row >= SCREEN_TEXT_ROWS) row = SCREEN_TEXT_ROWS - 1;   // defensive

  const uint16_t lastPage = ((total - 1) / SCREEN_TEXT_ROWS) * SCREEN_TEXT_ROWS;

  if (forward) {
    startFileIndex = (startFileIndex < lastPage)
                     ? startFileIndex + SCREEN_TEXT_ROWS : 0;
  } else {
    startFileIndex = (startFileIndex >= SCREEN_TEXT_ROWS)
                     ? startFileIndex - SCREEN_TEXT_ROWS : lastPage;
  }

  currentFileIndex = startFileIndex + row;
  if (currentFileIndex >= total) currentFileIndex = total - 1;
}

Menu::MenuAction_t Menu::getMenuAction(uint16_t totalFiles) {
  static Button_t lastButton = BUTTON_NONE;

  // +1 to account for when [/] is added to top of list for navigating back to root
  uint16_t virtualTotalFiles = totalFiles + (inSubFolder ? 1 : 0);  
  const Button_t button = getButton();

  if (button == BUTTON_NONE) {
    lastButton = BUTTON_NONE;   // release -> next press of any key clicks again
    buttonHeld = false;
    buttonDelay = MAX_REPEAT_KEY_DELAY;
    return ACTION_NONE;
  }

  bool clicked = false;
  if (button != lastButton) {
    Utils::playClick();
    clicked = true;
  }
  lastButton = button;

  if (button == BUTTON_MENU) { 
    return ACTION_SELECT_FILE;
  }

  const uint32_t now  = millis();
  if (buttonHeld && (now  - lastButtonHoldTime >= buttonDelay)) {  // speed up key repeat 
    buttonDelay = (buttonDelay > MIN_REPEAT_KEY_DELAY) ? buttonDelay - ADJUST_REPEAT_KEY_DELAY : MIN_REPEAT_KEY_DELAY;
    lastButtonHoldTime = now ;
  }

  if (!buttonHeld || (now - lastButtonPressTime >= buttonDelay)) {   // not held or delay elapsed
    MenuAction_t action = ACTION_NONE;
    if (button == BUTTON_ADVANCE) {
       // Move down or wrap to next page
      if (currentFileIndex < startFileIndex + SCREEN_TEXT_ROWS - 1 && currentFileIndex < virtualTotalFiles - 1) {
        currentFileIndex++;
        action = ACTION_MOVE_DOWN;
      } else {
        goPage(true, 0, virtualTotalFiles);         // wrap to top of next page
        action = ACTION_REFRESH_LIST;
      }
    } else if (button == BUTTON_BACK) {
      // Move up or wrap to previous page
      if (currentFileIndex > startFileIndex) { // still inside current page
        currentFileIndex--;
        action = ACTION_MOVE_UP;
      } else {
        if (startFileIndex >= SCREEN_TEXT_ROWS) {  // valid previous page
          startFileIndex -= SCREEN_TEXT_ROWS;
          currentFileIndex = startFileIndex + SCREEN_TEXT_ROWS - 1;
        } else {  // 1st page wraps to last page
          goPage(false, SCREEN_TEXT_ROWS - 1, virtualTotalFiles); // wrap to bottom of prev page
        }
        action = ACTION_REFRESH_LIST;
      }
    } else if (button == BUTTON_PAGE_DOWN) {
      // Jump forward a full page (Key 'P')
      goPage(true,  currentFileIndex - startFileIndex, virtualTotalFiles); // row captured pre-call
      action = ACTION_REFRESH_LIST;
    } else if (button == BUTTON_PAGE_UP) {
      // Jump back a full page (Key 'O')
      goPage(false, currentFileIndex - startFileIndex, virtualTotalFiles);
      action = ACTION_REFRESH_LIST;
    }

    if (action != ACTION_NONE) {
      if (!clicked) {
        Utils::playClick();
      }
      buttonHeld = true;
      lastButtonPressTime = now ;
      lastButtonHoldTime = now ;
      return action;
    }
  }
  return ACTION_NONE;
}
 
uint16_t Menu::scanFolder(bool reset) {
  if (reset) {
    currentFileIndex = 0;
    startFileIndex = 0;
  }
  uint16_t totalFiles;
  boolean clear=true;
  while ((totalFiles = SdCardSupport::countSnapshotFiles()) == 0) {
    if (SdCardSupport::isInserted()) {
      if (clear) {
        Utils::clearScreen(MENU_TEXT_COLOUR);
        clear = false;
      }
      Draw::text_P(80, 90, F("NO FILES FOUND"));
      Utils::delay16(20);
    } else {
      Utils::waitForSDCard_Blocking();
    }
  }
  return totalFiles;
}

Menu::Button_t Menu::getButton() {
  const uint8_t joy = Utils::readJoystick();
  // bits "XsfFUDLR"
  if (joy & (0x10 | 0x40)) return BUTTON_MENU;
  if (joy & 0x01) return BUTTON_PAGE_UP;     // Joy left
  if (joy & 0x02) return BUTTON_PAGE_DOWN;   // Joy Right
  if (joy & 0x04) return BUTTON_ADVANCE;     // Joy Down
  if (joy & 0x08) return BUTTON_BACK;        // Joy UP

  switch (Z80Bus::getKeyboard()) {
    case 'Q': return BUTTON_BACK;
    case 'A': return BUTTON_ADVANCE;
    case 'O': return BUTTON_PAGE_UP;
    case 'P': return BUTTON_PAGE_DOWN;
    case 0x0D: return BUTTON_MENU;   // 0x0D=enter
    default: return BUTTON_NONE;
  } 
}

void Menu::drawHighlightBar() {
    uint16_t rowOffset = (currentFileIndex - startFileIndex) << 5; 
    Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START + rowOffset, 32, COL::CYAN_BLACK);
};
