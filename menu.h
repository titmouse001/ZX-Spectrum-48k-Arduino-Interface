#ifndef MENU_H
#define MENU_H

#include "pin.h"
#include "draw.h"

unsigned long lastButtonPress = 0;     // Store the last time a button press was processed
unsigned long buttonDelay = 300;       // Initial delay between button actions in milliseconds
unsigned long lastButtonHoldTime = 0;  // Track how long the button has been held
bool buttonHeld = false;               // Track if the button is being held

uint16_t oldHighlightAddress = 0;      // Last highlighted screen position for clearing away
uint16_t currentFileIndex = 0;         // The currently selected file index in the list
uint16_t startFileIndex = 0;           // The start file index of the current viewing window

enum { BUTTON_NONE,
       BUTTON_BACK,
       BUTTON_SELECT,
       BUTTON_ADVANCE,
       BUTTON_BACK_REFRESH_LIST,
       BUTTON_ADVANCE_REFRESH_LIST };

#include <Wire.h>  //  I2C devices
#include "SSD1306AsciiAvrI2c.h"  //  I2C displays, oled

extern SSD1306AsciiAvrI2c oled;

__attribute__((optimize("-Ofast"))) 
void fileList(uint16_t startFileIndex) {
  root.rewind();
  uint8_t clr = 0;
  uint8_t count = 0;
  while (file.openNext(&root, O_RDONLY)) {
    if (file.isFile()) {
      if (file.fileSize() == 49179) {

        if ((count >= startFileIndex) && (count < startFileIndex + SCREEN_TEXT_ROWS)) {
          int len = file.getName7(fileName, 64);
          if (len == 0) { file.getSFN(fileName, 20); }

          if (len > 42) {
            fileName[40] = '.';
            fileName[41] = '.';
            fileName[42] = '\0';
          }

          Draw::textLine(0, ((count - startFileIndex) * 8), fileName);
          clr++;
        }
        count++;
      }
    }
    file.close();
    if (clr == SCREEN_TEXT_ROWS) {
      break;
    }
  }

  // Clear the remaining screen after last list item when needed.
  fileName[0] = ' ';  // Empty file selector slots (just wipes area)
  fileName[1] = '\0';
  for (uint8_t i = clr; i < SCREEN_TEXT_ROWS; i++) {
    Draw::textLine(0, (i * 8), fileName);
  }
}

byte getCommonButton() {
  // Kempston joystick bitmask: "000FUDLR" (Fire, Up, Down, Left, Right)
  const uint8_t joy = Utils::readJoystick();
  if (joy & (B00010000 | B01000000)) return BUTTON_SELECT;
  if (joy & B00000100) return BUTTON_ADVANCE;
  if (joy & B00001000) return BUTTON_BACK;

  switch (Z80Bus::GetKeyPulses()) {
    case 11: return BUTTON_BACK;
    case 6: return BUTTON_ADVANCE;
    case 31: return BUTTON_SELECT;
  }
  return BUTTON_NONE;
}


byte processButtons(uint16_t totalFiles) {
 
  byte button = getCommonButton();
  if (button == BUTTON_SELECT) {
    return BUTTON_SELECT;
  }

  if (button == BUTTON_NONE) {
    buttonHeld = false;
    buttonDelay = 300;  // Reset repeat delay
  } else {
    const unsigned long currentMillis = millis();
    if (buttonHeld && (currentMillis - lastButtonHoldTime >= buttonDelay)) {
      buttonDelay = (buttonDelay > 40) ? buttonDelay - 20 : 40;
      lastButtonHoldTime = currentMillis;
    }
    if (!buttonHeld || (currentMillis - lastButtonPress >= buttonDelay)) {
      byte action = BUTTON_NONE;
      if ((button == BUTTON_ADVANCE)) {
        if (currentFileIndex < startFileIndex + SCREEN_TEXT_ROWS - 1 && currentFileIndex < totalFiles - 1) {
          currentFileIndex++;
          action = BUTTON_ADVANCE;
        } else {
          action = BUTTON_ADVANCE_REFRESH_LIST;
          startFileIndex = (startFileIndex + SCREEN_TEXT_ROWS < totalFiles) ? startFileIndex + SCREEN_TEXT_ROWS : 0;
          currentFileIndex = startFileIndex;
        }
      } else if ((button == BUTTON_BACK)) {
        if (currentFileIndex > startFileIndex) {
          currentFileIndex--;
          action = BUTTON_BACK;
        } else {
          action = BUTTON_BACK_REFRESH_LIST;
          if (startFileIndex >= SCREEN_TEXT_ROWS) {
            startFileIndex -= SCREEN_TEXT_ROWS;
            currentFileIndex = startFileIndex + SCREEN_TEXT_ROWS - 1;
          } else {
            startFileIndex = totalFiles - (totalFiles % SCREEN_TEXT_ROWS);
            currentFileIndex = totalFiles - 1;
          }
        }
      }

      if (action != BUTTON_NONE) {
        buttonHeld = true;
        lastButtonPress = currentMillis;
        lastButtonHoldTime = currentMillis;
        return action;
      }
    }
  }
  return BUTTON_NONE;
}


uint16_t doMenu(uint16_t totalFiles) {
  fileList(startFileIndex);
  Z80Bus::highlightSelection(currentFileIndex, startFileIndex, oldHighlightAddress);

  while (true) {
    unsigned long start = millis();
    byte btn = processButtons(totalFiles);
    switch (btn) {
      case BUTTON_SELECT:
        return currentFileIndex;
      case BUTTON_ADVANCE:
      case BUTTON_BACK:
        Z80Bus::highlightSelection(currentFileIndex, startFileIndex, oldHighlightAddress);
        break;
      case BUTTON_ADVANCE_REFRESH_LIST:
      case BUTTON_BACK_REFRESH_LIST:
        Z80Bus::highlightSelection(currentFileIndex, startFileIndex, oldHighlightAddress);
        fileList(startFileIndex);
        break;
      default:
        break;
    }
    Utils::frameDelay(start);
  }
}

#endif
