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
bool     Menu::buttonHeld = false;

//uint16_t Menu::oldHighlightAddress = 0;
uint16_t Menu::currentFileIndex = 0;
uint16_t Menu::startFileIndex = 0;
bool     Menu::inSubFolder = false;

//constexpr uint8_t MENU_TEXT_COLOUR = COL::BRIGHT_BLACK_GREEN; 
constexpr uint8_t MENU_TEXT_COLOUR = COL::BRIGHT_BLACK_WHITE; 


__attribute__((optimize("-Os"))) 
FatFile* Menu::handleMenu() {

  Utils::clearScreen(MENU_TEXT_COLOUR);
  uint16_t totalFiles = scanFolder();

  displayFileList();
  Z80Bus::sendFillCommand( ZX_SCREEN_ATTR_ADDRESS_START + ((currentFileIndex - startFileIndex) * 32), 32, COL::CYAN_BLACK);

  while (true) {
    const uint32_t start = millis();
    uint16_t lastIndex = currentFileIndex;
    MenuAction_t action = getMenuAction(totalFiles);

    if (!SdCardSupport::isInserted()) {
      constexpr uint8_t clrScreenFlag(true);
      Utils::waitForSDCard_Blocking(clrScreenFlag);  // shows message "INSERT SD CARD"
      action = ACTION_REFRESH_LIST;
    }

    if (action == ACTION_SELECT_FILE) {
      FatFile& root = SdCardSupport::getRoot();
      bool backToRoot = (inSubFolder && currentFileIndex == 0);  // Inside folder - first item shows "[/]"

      if (backToRoot) {
        root.close();
        inSubFolder = false;
        root.open("/");
        totalFiles = scanFolder(false);  
      } else {
        // File or Folder
        SdCardSupport::openFileByIndex(inSubFolder ? currentFileIndex - 1 : currentFileIndex);
        FatFile& file = SdCardSupport::getFile();
        if (file.isDir()) {
          char* path = SdCardSupport::getFileNameWithSlash(&file);
          root.close();
          inSubFolder = root.open(path);
          if (!inSubFolder) root.open("/");  // Fallback on failure
          totalFiles = scanFolder(true);  // refresh for nav change
        } else {
          return &file;  // Selection confirmed
        }
      }
      Menu::waitForRelease();
      Utils::clearScreen(MENU_TEXT_COLOUR);
      displayFileList();
      Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START + ((currentFileIndex - startFileIndex) * 32), 32, COL::CYAN_BLACK);
      continue;
    }

    if (action == ACTION_REFRESH_LIST) {
      displayFileList();
    }

    if (action != ACTION_NONE) {
      Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START + ((currentFileIndex - startFileIndex) * 32), 32, COL::CYAN_BLACK);
    }
    Utils::frameDelay(start);
  }
}

__attribute__((optimize("-Ofast"))) 
void Menu::displayFileList() {

  FatFile& root = SdCardSupport::getRoot();
  root.rewind();

  uint16_t mark = BufferManager::getMark();
  constexpr uint8_t BRACKETS_AND_TERMINATOR = 3;
  char* nameBuffer = (char*)BufferManager::allocate(ZX_FILENAME_MAX_DISPLAY_LEN + BRACKETS_AND_TERMINATOR);
  uint16_t linesDrawn = 0;
  uint16_t filesSkipped = 0;

  if (inSubFolder && startFileIndex == 0) {
    Draw::textLine(0, "[/]");  // parent directory indicator
    linesDrawn = 1;
  }

  uint16_t actualFilesToSkip = startFileIndex;
  if (inSubFolder && startFileIndex > 0) {
    actualFilesToSkip--;
  }

  FatFile& file = SdCardSupport::getFile();
    while (file.openNext(&root, O_RDONLY)) {
    if (file.isHidden()) {
      file.close();
      continue;
    }

    if (filesSkipped < actualFilesToSkip) {  // Skip until start index
      filesSkipped++;
      file.close();
      continue;  // fast forward through the directory
    }

    if (linesDrawn < SCREEN_TEXT_ROWS) {
      char* displayName = nameBuffer;

      if (file.isDir()) {
        nameBuffer[0] = '[';  // directory "[" prefix
        displayName = &nameBuffer[1];
      }

      uint8_t len = SdCardSupport::getFileName(&file, displayName);  // names are null terminated
      if (len > ZX_FILENAME_MAX_DISPLAY_LEN) {                       // Trim long names
        displayName[ZX_FILENAME_MAX_DISPLAY_LEN - 2] = '.';
        displayName[ZX_FILENAME_MAX_DISPLAY_LEN - 1] = '.';
        displayName[ZX_FILENAME_MAX_DISPLAY_LEN] = '\0';
      } else if (file.isDir()) {
        nameBuffer[len + 1] = ']';  // directory "]" suffix
        nameBuffer[len + 2] = '\0';
      }

      Draw::textLine(linesDrawn * FONT_HEIGHT_WITH_GAP, nameBuffer);
      linesDrawn++;
    }
    file.close();
    if (linesDrawn >= SCREEN_TEXT_ROWS) break; // screen is full
  }

  // Use blank textLine to clear remaining rows
  for (uint8_t i = linesDrawn * FONT_HEIGHT_WITH_GAP; i < SCREEN_TEXT_ROWS * FONT_HEIGHT_WITH_GAP; i += FONT_HEIGHT_WITH_GAP) {
    Draw::textLine(i, " ");
  }

  BufferManager::freeToMark(mark);
}

__attribute__((optimize("-Os")))
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
         
      Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START + ((currentFileIndex-startFileIndex)*32), 32, MENU_TEXT_COLOUR);

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

      Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START + ((currentFileIndex-startFileIndex)*32), 32, MENU_TEXT_COLOUR);

       // Move up or wrap to previous page
      if (currentFileIndex > startFileIndex) {
        currentFileIndex--;
        action = ACTION_MOVE_UP;
      } else {
        if (startFileIndex >= SCREEN_TEXT_ROWS) {
          startFileIndex -= SCREEN_TEXT_ROWS;
          currentFileIndex = startFileIndex + SCREEN_TEXT_ROWS - 1;
        } else {
          startFileIndex = virtualTotalFiles - (virtualTotalFiles % SCREEN_TEXT_ROWS);
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

__attribute__((optimize("-Os"))) 
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
      Utils::waitForSDCard_Blocking(false);
    }
    delay(20);
    clr = true;
  }
  if (clr) {
    Utils::clearScreen(MENU_TEXT_COLOUR);
  }

  return totalFiles;
}

__attribute__((optimize("-Os")))
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
}