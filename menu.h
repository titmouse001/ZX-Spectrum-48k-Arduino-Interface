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

// int getStableAnalogRead(int pin) {
//   long sum = 0;
//   const int samples = 10;
//   for (int i = 0; i < samples; i++) {
//     sum += analogRead(pin);
//     delay(2);  // small delay between samples
//   }
//   return sum / samples;
// }
uint8_t getAnalogButton2(int but) {
  
  static constexpr int BUTTON_ADVANCE_THRESHOLD = 122;    // (1st left button on PCB Board)
  static constexpr int BUTTON_SELECT_THRESHOLD = 424;
  static constexpr int BUTTON_BACK_THRESHOLD = 610;

//  int but = analogRead(Pin::BUTTON_PIN);
//  int but = getStableAnalogRead(Pin::BUTTON_PIN);

  if (but < BUTTON_ADVANCE_THRESHOLD) {
    return BUTTON_ADVANCE;
  } else if (but < BUTTON_SELECT_THRESHOLD) {
    return BUTTON_SELECT;
  } else if (but < BUTTON_BACK_THRESHOLD) {
    return BUTTON_BACK;
  }
  return BUTTON_NONE;
}


uint8_t getAnalogButton() {
  
  static constexpr int BUTTON_ADVANCE_THRESHOLD = 122;    // (1st left button on PCB Board)
  static constexpr int BUTTON_SELECT_THRESHOLD = 424;
  static constexpr int BUTTON_BACK_THRESHOLD = 610;

  int but = analogRead(Pin::BUTTON_PIN);
//  int but = getStableAnalogRead(Pin::BUTTON_PIN);

  if (but < BUTTON_ADVANCE_THRESHOLD) {
    return BUTTON_ADVANCE;
  } else if (but < BUTTON_SELECT_THRESHOLD) {
    return BUTTON_SELECT;
  } else if (but < BUTTON_BACK_THRESHOLD) {
    return BUTTON_BACK;
  }
  return BUTTON_NONE;
}

byte readButtons(uint16_t totalFiles) {
  // ANALOGUE VALUES ARE AROUND... 1024,510,324,22
  byte ret = BUTTON_NONE;
  //int but = analogRead(Pin::BUTTON_PIN);
  uint8_t but = getAnalogButton();
  uint8_t joy = Utils::readJoystick();  // Kempston standard, "000FUDLR" bit pattern

  if (but != BUTTON_NONE || (joy & B00011100)) {
    unsigned long currentMillis = millis();  // Get the current time
    if (buttonHeld && (currentMillis - lastButtonHoldTime >= buttonDelay)) {
      buttonDelay = max(40, buttonDelay - 20);  // speed-up file selection over time
      lastButtonHoldTime = currentMillis;
    }

    if ((!buttonHeld) || (currentMillis - lastButtonPress >= buttonDelay)) {
      if ((but == BUTTON_ADVANCE) || (joy & B00000100)) {  // 1st button
        if (currentFileIndex < startFileIndex + SCREEN_TEXT_ROWS - 1 && currentFileIndex < totalFiles - 1) {
          currentFileIndex++;
          ret = BUTTON_ADVANCE;
        } else if (startFileIndex + SCREEN_TEXT_ROWS < totalFiles) {
          startFileIndex += SCREEN_TEXT_ROWS;
          currentFileIndex = startFileIndex;
          ret = BUTTON_ADVANCE_REFRESH_LIST;
        } else {
          startFileIndex = 0;
          currentFileIndex = 0;
          ret = BUTTON_ADVANCE_REFRESH_LIST;
        }
      } else if ((but == BUTTON_SELECT) || (joy & B00010000)) {  // 2nd button
        ret = BUTTON_SELECT;
      } else if ((but == BUTTON_BACK) || (joy & B00001000)) {  // 3rd button
        if (currentFileIndex > startFileIndex) {
          currentFileIndex--;
          ret = BUTTON_BACK;
        } else if (startFileIndex >= SCREEN_TEXT_ROWS) {
          startFileIndex -= SCREEN_TEXT_ROWS;
          currentFileIndex = startFileIndex + SCREEN_TEXT_ROWS - 1;
          ret = BUTTON_BACK_REFRESH_LIST;
        } else {
          startFileIndex = totalFiles - (totalFiles % SCREEN_TEXT_ROWS);
          currentFileIndex = totalFiles - 1;  // (totalFiles%SCREEN_TEXT_ROWS);
          ret = BUTTON_BACK_REFRESH_LIST;
        }
      }
      buttonHeld = true;
      lastButtonPress = currentMillis;
      lastButtonHoldTime = currentMillis;
    }
  } else { /* no button is pressed - reset delay */
    buttonHeld = false;
    buttonDelay = 300;  // Reset to the initial delay
  }
  return ret;
}


uint16_t doMenu(uint16_t totalFiles) {

  fileList(startFileIndex);
  Z80Bus::highlightSelection(currentFileIndex, startFileIndex, oldHighlightAddress);

  while (true) {
    unsigned long start = millis();

    byte btn = readButtons(totalFiles);
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
