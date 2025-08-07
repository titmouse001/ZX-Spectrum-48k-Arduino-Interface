#ifndef MENU_H
#define MENU_H

#include "pin.h"
#include "draw.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

namespace Menu {

uint32_t lastButtonPress = 0;
uint32_t lastButtonHoldTime = 0;
uint16_t buttonDelay = 300;
uint16_t oldHighlightAddress = 0;
uint16_t currentFileIndex = 0;
uint16_t startFileIndex = 0;
bool buttonHeld = false;

constexpr uint8_t SCREEN_TEXT_ROWS = 24;  // 192/8
constexpr uint8_t ZX_FILENAME_MAX_DISPLAY_LEN = 42;
constexpr uint8_t FONT_HEIGHT_WITH_GAP = SmallFont::FNT_HEIGHT + SmallFont::FNT_GAP;
constexpr uint16_t MAX_REPEAT_KEY_DELAY = 300;

typedef enum {
  BUTTON_NONE,
  BUTTON_BACK,
  BUTTON_SELECT,
  BUTTON_ADVANCE
} Button_t;

typedef enum {
  ACTION_NONE,
  ACTION_SELECT_FILE,
  ACTION_MOVE_UP,
  ACTION_MOVE_DOWN,
  ACTION_REFRESH_LIST
} MenuAction_t;


__attribute__((optimize("-Ofast"))) 
void fileList(uint16_t startFileIndex) {
 
  FatFile& file = SdCardSupport::file;
  FatFile& root = SdCardSupport::root;
  root.rewind();

  uint16_t count = 0;
  while (file.openNext(&root, O_RDONLY)) {
    if (file.isFile()) {
      if (count >= startFileIndex) { break; }
      count++;
    }
    file.close();
  }

  //char* fileName = (char*)&packetBuffer[5 + SmallFont::FNT_BUFFER_SIZE];   // TO DO uses command_Copy32 = 4 !!!!!
  // NOTE: File name uses the space just after the one for Draw::textLine which uses [0] to [4+32-1]
  // We are free to use [+35] while fileList is going on. 
  char* fileName = (char*)&packetBuffer[ E(Copy32Packet::PACKET_LEN) + SmallFont::FNT_BUFFER_SIZE];  
  count = 0;
  do {
    if (!file.isFile()) break;
    if (count < SCREEN_TEXT_ROWS) {
      uint16_t len = file.getName7(fileName, 64);
      if (len == 0) { file.getSFN(fileName, 20); }
      if (len > ZX_FILENAME_MAX_DISPLAY_LEN) {  // limit to fit screen
        fileName[ZX_FILENAME_MAX_DISPLAY_LEN - 2] = '.';
        fileName[ZX_FILENAME_MAX_DISPLAY_LEN - 1] = '.';
        fileName[ZX_FILENAME_MAX_DISPLAY_LEN - 0] = '\0';
      }
      Draw::textLine(0, count * 8, fileName); // 
      count++;
    }
    file.close();
  } while (file.openNext(&root, O_RDONLY));
  file.close();  // as we broke out early

  fileName[0] = ' ';  // blank filename
  fileName[1] = '\0';
  for (uint8_t i = count; i < SCREEN_TEXT_ROWS; i++) {
    Draw::textLine(0, i * FONT_HEIGHT_WITH_GAP, fileName);  // clear unused space
  }
}

Button_t getButton() {
  // Kempston joystick bitmask: "000FUDLR" (Fire, Up, Down, Left, Right)
  const uint8_t joy = Utils::readJoystick();

  if (joy & (0x10 | 0x40)) { return BUTTON_SELECT; }  // Fire or select button
  if (joy & 0x04) { return BUTTON_ADVANCE; }          // Down
  if (joy & 0x08) { return BUTTON_BACK; }             // Up

  // Process speccy keyboard
  switch (Z80Bus::GetKeyPulses()) {
    case 11: return BUTTON_BACK;    // A
    case 6: return BUTTON_ADVANCE;  // Q
    case 31: return BUTTON_SELECT;  // ENTER
    default: return BUTTON_NONE;
  }
}

MenuAction_t getMenuAction(uint16_t totalFiles) {
  const Button_t button = getButton();

  // Handle select button immediately - no repeat logic needed
  if (button == BUTTON_SELECT) {
    return ACTION_SELECT_FILE;
  }

  // Handle button release
  if (button == BUTTON_NONE) {
    buttonHeld = false;
    buttonDelay = MAX_REPEAT_KEY_DELAY;
    return ACTION_NONE;
  }

  // Handle button timing and repeat logic
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
        // Wrap to next page or beginning
        startFileIndex = (startFileIndex + SCREEN_TEXT_ROWS < totalFiles) ? startFileIndex + SCREEN_TEXT_ROWS : 0;
        currentFileIndex = startFileIndex;
        action = ACTION_REFRESH_LIST;
      }
    } else if (button == BUTTON_BACK) {
      if (currentFileIndex > startFileIndex) {
        currentFileIndex--;
        action = ACTION_MOVE_UP;
      } else {
        // Wrap to previous page or end
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

uint16_t doFileMenu(uint16_t totalFiles) {
  fileList(startFileIndex);
  Z80Bus::highlightSelection(currentFileIndex, startFileIndex, oldHighlightAddress);

  while (true) {
    const uint32_t start = millis();
    const MenuAction_t action = getMenuAction(totalFiles);

    if (action != ACTION_NONE) { // do highlight before fileList for best feedback response 
      Z80Bus::highlightSelection(currentFileIndex, startFileIndex, oldHighlightAddress);
    }
    if (action == ACTION_SELECT_FILE) {
      return currentFileIndex;
    }else if (action == ACTION_REFRESH_LIST) {
      fileList(startFileIndex);  // takes a few frames to update
    }

    Utils::frameDelay(start);
  }
}

}
#endif
