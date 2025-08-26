#include "Menu.h"
#include "Constants.h"
#include "Utils.h"
#include "SdCardSupport.h"
#include "BufferManager.h"
#include "draw.h"
#include "Z80Bus.h"

uint32_t Menu::lastButtonPressTime = 0;
uint32_t Menu::lastButtonHoldTime = 0;
uint16_t Menu::buttonDelay = Menu::MAX_REPEAT_KEY_DELAY;
bool     Menu::buttonHeld = false;

uint16_t Menu::oldHighlightAddress = 0;
uint16_t Menu::currentFileIndex = 0;
uint16_t Menu::startFileIndex = 0;
bool     Menu::inSubFolder = false;

void Menu::displayItemList(uint16_t startFileIndex) {
  FatFile& file = SdCardSupport::getFile(); 
  FatFile& root = SdCardSupport::getRoot();
  root.rewind();

  char* nameBuffer = (char*)&BufferManager::packetBuffer[E(Copy32Packet::PACKET_LEN) + SmallFont::FNT_BUFFER_SIZE];
  uint16_t linesDrawn = 0;
  uint16_t filesSkipped = 0;

  if (inSubFolder && startFileIndex == 0) {
    Draw::textLine(0, "[/]");
    linesDrawn = 1;
  }

  while (file.openNext(&root, O_RDONLY)) {
    if (file.isHidden()) {
      file.close();
      continue;
    }

    if (filesSkipped < startFileIndex) {  // Skip until start index
      filesSkipped++;
      file.close();
      continue;
    }

    if (linesDrawn < SCREEN_TEXT_ROWS) {
      char* displayName = nameBuffer;

      if (file.isDir()) {
        nameBuffer[0] = '[';  // directory "[" prefix
        displayName = &nameBuffer[1];
      }

      uint16_t len = file.getName7(displayName, MAX_FILENAME_LEN);
      if (len == 0) {
        file.getSFN(displayName, 20);
        len = strlen(displayName);
      }

      if (len > ZX_FILENAME_MAX_DISPLAY_LEN) {  // Trim long names
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
  }

  // Use blank textLine to clear remaining rows
  nameBuffer[0] = ' ';
  nameBuffer[1] = '\0';
  for (uint8_t i = linesDrawn; i < SCREEN_TEXT_ROWS; i++) {
    Draw::textLine(i * FONT_HEIGHT_WITH_GAP, nameBuffer);
  }
}

Menu::Button_t Menu::getButton() {
  const uint8_t joy = Utils::readJoystick();
  if (joy & (0x10 | 0x40)) return BUTTON_SELECT;
  if (joy & 0x04) return BUTTON_ADVANCE;
  if (joy & 0x08) return BUTTON_BACK;
  switch (Z80Bus::GetKeyPulses()) {
    case 11: return BUTTON_BACK;
    case 6: return BUTTON_ADVANCE;
    case 31: return BUTTON_SELECT;
    default: return BUTTON_NONE;
  }
}

Menu::MenuAction_t Menu::getMenuAction(uint16_t totalFiles) {
 
  // +1 to account for when [/] is added to top of list for navigating back to root
  uint16_t virtualTotalFiles = totalFiles + (inSubFolder ? 1 : 0);  

  const Button_t button = getButton();

  if (button == BUTTON_SELECT) { 
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


uint16_t Menu::rescanFolder(bool reset) {
 
  if (reset) {
    currentFileIndex = 0;
    startFileIndex = 0;
  }
  uint16_t totalFiles = SdCardSupport::countSnapshotFiles();
  if (totalFiles == 0) {
    Draw::text_P(80, 90, F("NO FILES FOUND"));
    do {
      delay(20);
      totalFiles = SdCardSupport::countSnapshotFiles();
    } while (totalFiles == 0);
  }
  Z80Bus::clearScreen(COL::BLACK_WHITE);
  displayItemList(startFileIndex);
  Z80Bus::highlightSelection(currentFileIndex, startFileIndex, oldHighlightAddress);
  return totalFiles;
}

FatFile* Menu::handleMenu() {

 
  uint16_t totalFiles = rescanFolder();

  while (true) {
    const uint32_t start = millis();
    const MenuAction_t action = getMenuAction(totalFiles);
    if (action != ACTION_NONE) {
      Z80Bus::highlightSelection(currentFileIndex, startFileIndex, oldHighlightAddress);
    } 
    if (action == ACTION_SELECT_FILE) {
       FatFile& root = SdCardSupport::getRoot();

      if (inSubFolder && currentFileIndex == 0) { // Go back to root
        root.close();
        if (root.open("/")) {  // parent not supported - simply resets to root
          inSubFolder = false;
          totalFiles = rescanFolder(true);
        }
        continue;
      }
      uint16_t actualFileIndex = inSubFolder ? currentFileIndex - 1 : currentFileIndex;
      SdCardSupport::openFileByIndex(actualFileIndex);

 //     FatFile& file = SdCardSupport::file;
      FatFile& file = SdCardSupport::getFile(); 
 
      if (file.isDir()) {
        char* nameWithPath = SdCardSupport::getFileNameWithSlash(&file, (char*)&BufferManager::packetBuffer[FILE_READ_BUFFER_OFFSET]);
        root.close();
        if (root.open(nameWithPath)) {
          inSubFolder = true;
        } else {  // should not happen - just incase
          inSubFolder = false;
          root.open("/");  // something failed!
        }
        totalFiles = rescanFolder(true);
      } else {
        return &file;  // actualFileIndex;
      }
    } else if (action == ACTION_REFRESH_LIST) {
      displayItemList(startFileIndex);
    }
    Utils::frameDelay(start);
  }
}
