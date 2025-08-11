#include "Menu.h"
#include "Constants.h"
#include "Utils.h"
#include "SdCardSupport.h"
#include "BufferManager.h"
#include "draw.h"
#include "Z80Bus.h"

uint32_t Menu::lastButtonPress = 0;
uint32_t Menu::lastButtonHoldTime = 0;
uint16_t Menu::buttonDelay = Menu::MAX_REPEAT_KEY_DELAY;
uint16_t Menu::oldHighlightAddress = 0;
uint16_t Menu::currentFileIndex = 0;
uint16_t Menu::startFileIndex = 0;
bool Menu::buttonHeld = false;

void Menu::fileList(uint16_t startFileIndex) {
  FatFile& file = SdCardSupport::file;
  FatFile& root = SdCardSupport::root;
  root.rewind();
  uint16_t count = 0;
  while (file.openNext(&root, O_RDONLY)) {
    if (file.isFile()) {
      if (count >= startFileIndex) break;
      count++;
    }
    file.close();
  }
  char* fileName = (char*)&BufferManager::packetBuffer[E(Copy32Packet::PACKET_LEN) + SmallFont::FNT_BUFFER_SIZE];
  count = 0;
  do {
    if (!file.isFile()) break;
    if (count < SCREEN_TEXT_ROWS) {
      uint16_t len = file.getName7(fileName, 64);
      if (len == 0) file.getSFN(fileName, 20);
      if (len > ZX_FILENAME_MAX_DISPLAY_LEN) {
        fileName[ZX_FILENAME_MAX_DISPLAY_LEN - 2] = '.';
        fileName[ZX_FILENAME_MAX_DISPLAY_LEN - 1] = '.';
        fileName[ZX_FILENAME_MAX_DISPLAY_LEN - 0] = '\0';
      }
      Draw::textLine(count * 8, fileName);
      count++;
    }
    file.close();
  } while (file.openNext(&root, O_RDONLY));
  file.close();
  fileName[0] = ' ';
  fileName[1] = '\0';
  for (uint8_t i = count; i < SCREEN_TEXT_ROWS; i++) {
    Draw::textLine(i * FONT_HEIGHT_WITH_GAP, fileName);
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
  const Button_t button = getButton();
  if (button == BUTTON_SELECT) return ACTION_SELECT_FILE;
  if (button == BUTTON_NONE) {
    buttonHeld = false;
    buttonDelay = MAX_REPEAT_KEY_DELAY;
    return ACTION_NONE;
  }
  const uint32_t currentMillis = millis();
  if (buttonHeld && (currentMillis - lastButtonHoldTime >= buttonDelay)) {
    buttonDelay = (buttonDelay > 40) ? buttonDelay - 20 : 40;
    lastButtonHoldTime = currentMillis;
  }
  if (!buttonHeld || (currentMillis - lastButtonPress >= buttonDelay)) {
    MenuAction_t action = ACTION_NONE;
    if (button == BUTTON_ADVANCE) {
      if (currentFileIndex < startFileIndex + SCREEN_TEXT_ROWS - 1 && currentFileIndex < totalFiles - 1) {
        currentFileIndex++;
        action = ACTION_MOVE_DOWN;
      } else {
        startFileIndex = (startFileIndex + SCREEN_TEXT_ROWS < totalFiles) ? startFileIndex + SCREEN_TEXT_ROWS : 0;
        currentFileIndex = startFileIndex;
        action = ACTION_REFRESH_LIST;
      }
    } else if (button == BUTTON_BACK) {
      if (currentFileIndex > startFileIndex) {
        currentFileIndex--;
        action = ACTION_MOVE_UP;
      } else {
        if (startFileIndex >= SCREEN_TEXT_ROWS) {
          startFileIndex -= SCREEN_TEXT_ROWS;
          currentFileIndex = startFileIndex + SCREEN_TEXT_ROWS - 1;
        } else {
          startFileIndex = totalFiles - (totalFiles % SCREEN_TEXT_ROWS);
          currentFileIndex = totalFiles - 1;
        }
        action = ACTION_REFRESH_LIST;
      }
    }
    if (action != ACTION_NONE) {
      buttonHeld = true;
      lastButtonPress = currentMillis;
      lastButtonHoldTime = currentMillis;
      return action;
    }
  }
  return ACTION_NONE;
}
uint16_t Menu::doFileMenu(uint16_t totalFiles) {
  fileList(startFileIndex);
  Z80Bus::highlightSelection(currentFileIndex, startFileIndex, oldHighlightAddress);
  while (true) {
    const uint32_t start = millis();
    const MenuAction_t action = getMenuAction(totalFiles);
    if (action != ACTION_NONE) {
      Z80Bus::highlightSelection(currentFileIndex, startFileIndex, oldHighlightAddress);
    }
    if (action == ACTION_SELECT_FILE) {
      return currentFileIndex;
    } else if (action == ACTION_REFRESH_LIST) {
      fileList(startFileIndex);
    }
    Utils::frameDelay(start);
  }
}