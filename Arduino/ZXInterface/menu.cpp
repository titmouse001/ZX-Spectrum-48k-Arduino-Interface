#include "Menu.h"
#include "Constants.h"
#include "Utils.h"
#include "SdCardSupport.h"
#include "BufferManager.h"
#include "draw.h"
#include "Z80Bus.h"


uint32_t Menu::lastButtonPressTime = 0;
uint32_t Menu::lastButtonHoldTime = 0;
uint16_t Menu::buttonDelay = MAX_REPEAT_KEY_DELAY;
uint16_t Menu::currentFileIndex = 0;
uint16_t Menu::startFileIndex = 0;
bool     Menu::buttonHeld = false;
bool     Menu::inSubFolder = false;

constexpr uint8_t MENU_TEXT_COLOUR = COL::BRIGHT_BLACK_WHITE; 


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

  drawFileList(true);
  drawHighlightBar();

  while (true) {

#ifdef _DEBUG_POOL_SIZE_ENABLED_
    char _c[8];
    itoa(BufferManager::poolOffsetLastMax, _c, 10);
    Draw::text(256 - 64, 32, _c);

    itoa(BufferManager::poolOffset, _c, 10);
    Draw::text(256 - 64, 32+16, _c);
#endif

    //const uint32_t start = millis();
    uint16_t lastIndex = currentFileIndex;
    MenuAction_t action = getMenuAction(totalFiles);

    if (!SdCardSupport::isInserted()) {
      static constexpr uint8_t clrScreenFlag(true);
      Utils::waitForSDCard_Blocking(clrScreenFlag);  // shows message "INSERT SD CARD"
      action = ACTION_REFRESH_LIST;
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
        uint16_t selectedIndex = inSubFolder ? currentFileIndex - 1 : currentFileIndex;
        FatFile* file= SdCardSupport::openFileByIndex(selectedIndex);
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
      drawFileList(true);  // full redraw
      drawHighlightBar();
      continue;
    }

    if (action == ACTION_REFRESH_LIST) {    
      drawFileList(true); 
      drawHighlightBar();
    }else if (action != ACTION_NONE) {
      drawFileList(false);                   // only reset attribute colours (clears old highlight)
      drawHighlightBar();
    }
    Utils::delay16(MAX_BUTTON_READ_MILLISECONDS);  // aiming for 50 FPS'ish
  }
}

void Menu::drawFileList(bool updateText) {
  FatFile& root = SdCardSupport::getRoot();
  root.rewind();

  uint16_t mark = BufferManager::getMark();
  char* nameBuffer = (char*)BufferManager::allocate(ZX_FILENAME_MAX_DISPLAY_LEN + 1);
  uint16_t linesDrawn = 0;
  uint16_t filesSkipped = 0;

  // Parent directory indicator (if needed)
  if (inSubFolder && startFileIndex == 0) {
    Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START, 32,
                            COL::BRIGHT_BLACK_GREEN);
    if (updateText) {
      Draw::textLine(0, "[/]");
    }
    linesDrawn = 1;
  }

  uint16_t actualFilesToSkip = startFileIndex;
  if (inSubFolder && startFileIndex > 0) {
    actualFilesToSkip--;
  }

  FatFile& file = SdCardSupport::getFile();
  while (file.openNext(&root, O_RDONLY)) {
    if (!file.isHidden()) {
      if (filesSkipped < actualFilesToSkip) {
        filesSkipped++;
      } else {
        bool isDirectory = file.isDir();
        if (updateText) {
          uint8_t len = file.getDisplayName7(nameBuffer, ZX_FILENAME_MAX_DISPLAY_LEN + 1);
          if (isDirectory && len < ZX_FILENAME_MAX_DISPLAY_LEN) {
            nameBuffer[len] = '/';
            nameBuffer[len + 1] = '\0';
          }
        }
        Z80Bus::sendFillCommand( ZX_SCREEN_ATTR_ADDRESS_START + (linesDrawn * 32), 32,
                                isDirectory ? COL::BRIGHT_BLACK_GREEN : MENU_TEXT_COLOUR);
        if (updateText) {
          Draw::textLine(linesDrawn * FONT_HEIGHT_WITH_GAP, nameBuffer);
        }
        linesDrawn++;
      }
    }
    file.close();
    if (linesDrawn >= SCREEN_TEXT_ROWS) break;
  }

  if (updateText) {
    // Clear remaining rows
    for (uint8_t i = linesDrawn; i < SCREEN_TEXT_ROWS; i++) {
      Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START + (i * 32), 32,
                              MENU_TEXT_COLOUR);
      Draw::textLine(i * FONT_HEIGHT_WITH_GAP, NULL);  // null will clear line
    }
  }

  BufferManager::freeToMark(mark);
}

Menu::MenuAction_t Menu::getMenuAction(uint16_t totalFiles) {

  // +1 to account for when [/] is added to top of list for navigating back to root
  uint16_t virtualTotalFiles = totalFiles + (inSubFolder ? 1 : 0);  
  const Button_t button = getButton();

  if (button == BUTTON_MENU) { 
    return ACTION_SELECT_FILE;
  }

  if (button == BUTTON_NONE) {
    buttonHeld = false;
    buttonDelay = MAX_REPEAT_KEY_DELAY;
    return ACTION_NONE;
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
        startFileIndex = (startFileIndex + SCREEN_TEXT_ROWS < virtualTotalFiles) ? startFileIndex + SCREEN_TEXT_ROWS : 0;
        currentFileIndex = startFileIndex;
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
          startFileIndex = ((virtualTotalFiles - 1) / SCREEN_TEXT_ROWS) * SCREEN_TEXT_ROWS;
          currentFileIndex = virtualTotalFiles - 1;
        }
        action = ACTION_REFRESH_LIST;
      }
    }
    if (action != ACTION_NONE) {
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
  bool clr = false;
  while ((totalFiles = SdCardSupport::countSnapshotFiles()) == 0) {
    if (SdCardSupport::isInserted()) {
      Draw::text_P(80, 90, F("NO FILES FOUND"));
    } else {
      Utils::waitForSDCard_Blocking();
    }
    Utils::delay16(20);
    clr = true;
  }
  if (clr) {
    Utils::clearScreen(MENU_TEXT_COLOUR);
  }
  return totalFiles;
}

Menu::Button_t Menu::getButton() {
  const uint8_t joy = Utils::readJoystick();
  if (joy & (0x10 | 0x40)) return BUTTON_MENU;
  if (joy & 0x04) return BUTTON_ADVANCE;
  if (joy & 0x08) return BUTTON_BACK;

  switch (Z80Bus::getKeyboard()) {
    case 'Q': return BUTTON_BACK;
    case 'A': return BUTTON_ADVANCE;
    case 0x0D: return BUTTON_MENU;   // 0x0D=enter
    default: return BUTTON_NONE;
  } 
}

void Menu::resetToRoot() {
  currentFileIndex = 0;
  startFileIndex = 0;
  inSubFolder = false;
  SdCardSupport::menuPathDepth = 0;
}

void Menu::drawHighlightBar() {
    uint16_t rowOffset = (currentFileIndex - startFileIndex) << 5; 
    Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START + rowOffset, 32, COL::CYAN_BLACK);
};