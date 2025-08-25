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
uint16_t Menu::oldHighlightAddress = 0;
uint16_t Menu::currentFileIndex = 0;
uint16_t Menu::startFileIndex = 0;
bool Menu::buttonHeld = false;
bool Menu::inSubFolder = false;



#include "SSD1306AsciiAvrI2c.h"
extern SSD1306AsciiAvrI2c oled;

void Menu::displayItemList(uint16_t startFileIndex) {
  FatFile& file = SdCardSupport::file;
  FatFile& root = SdCardSupport::root;
  root.rewind();

  char* nameBuffer = (char*)&BufferManager::packetBuffer[E(Copy32Packet::PACKET_LEN) + SmallFont::FNT_BUFFER_SIZE];
  uint16_t linesDrawn = 0;
  uint16_t filesSkipped = 0;

  if (inSubFolder && startFileIndex == 0) {
    Draw::textLine(0, "[..]");
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

      uint16_t len = file.getName7(displayName, 64);
      if (len == 0) file.getSFN(displayName, 20);

      if (len > ZX_FILENAME_MAX_DISPLAY_LEN) {  // Trim long names
        displayName[ZX_FILENAME_MAX_DISPLAY_LEN - 2] = '.';
        displayName[ZX_FILENAME_MAX_DISPLAY_LEN - 1] = '.';
        displayName[ZX_FILENAME_MAX_DISPLAY_LEN] = '\0';
      }

      else if (file.isDir()) {
        nameBuffer[len + 1] = ']';  // directory "]" suffix
        nameBuffer[len + 2] = '\0';
      }

      Draw::textLine(linesDrawn * FONT_HEIGHT_WITH_GAP, nameBuffer);
      linesDrawn++;
    }

    file.close();
  }

  // Clear remaining rows
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
  // check and account for the extra [..] entry
  uint16_t virtualTotalFiles = totalFiles + (inSubFolder ? 1 : 0);

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

  if (!buttonHeld || (currentMillis - lastButtonPressTime >= buttonDelay)) {
    MenuAction_t action = ACTION_NONE;
    if (button == BUTTON_ADVANCE) {
      if (currentFileIndex < startFileIndex + SCREEN_TEXT_ROWS - 1 && currentFileIndex < virtualTotalFiles - 1) {
        currentFileIndex++;
        action = ACTION_MOVE_DOWN;
      } else {
        startFileIndex = (startFileIndex + SCREEN_TEXT_ROWS < virtualTotalFiles) ? startFileIndex + SCREEN_TEXT_ROWS : 0;
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
          startFileIndex = virtualTotalFiles - (virtualTotalFiles % SCREEN_TEXT_ROWS);
          currentFileIndex = virtualTotalFiles - 1;
        }
        action = ACTION_REFRESH_LIST;
      }
    }
    if (action != ACTION_NONE) {
      buttonHeld = true;
      lastButtonPressTime = currentMillis;
      lastButtonHoldTime = currentMillis;
      return action;
    }
  }
  return ACTION_NONE;
}

uint16_t Menu::handleMenu(uint16_t totalFiles) {

  displayItemList(startFileIndex);

  Z80Bus::highlightSelection(currentFileIndex, startFileIndex, oldHighlightAddress);
  while (true) {
    const uint32_t start = millis();
    const MenuAction_t action = getMenuAction(totalFiles);
    if (action != ACTION_NONE) {
      Z80Bus::highlightSelection(currentFileIndex, startFileIndex, oldHighlightAddress);
    }
    if (action == ACTION_SELECT_FILE) {


      if (inSubFolder && currentFileIndex == 0) {
        // Go back to root
        SdCardSupport::root.close();
        if (SdCardSupport::root.open("/")) {
          inSubFolder = false;
          oldHighlightAddress = 0;
          currentFileIndex = 0;
          startFileIndex = 0;

          totalFiles = SdCardSupport::countSnapshotFiles();
          Z80Bus::clearScreen(COL::BLACK_WHITE);
          displayItemList(startFileIndex);

          oled.print("ROOT OK");
        } else {
          Draw::text_P(80, 90, F("ROOT FAILED"));
          delay(1000);
        }
        continue;
      }


      uint16_t actualFileIndex = inSubFolder ? currentFileIndex - 1 : currentFileIndex;

      SdCardSupport::openFileByIndex(actualFileIndex);

      FatFile& file = SdCardSupport::file;
      if (file.isDir()) {

        char* nameWithPath = SdCardSupport::getFileNameWithSlash((char*)&BufferManager::packetBuffer[FILE_READ_BUFFER_OFFSET]);

        oled.print(nameWithPath);

        SdCardSupport::root.close();
        if (!SdCardSupport::root.open(nameWithPath)) {
          Draw::text_P(80, 90, F("FAILED"));
          delay(1000);
        } else {
          oled.print(",OK");
          inSubFolder = true;
        }

        oldHighlightAddress = 0;
        currentFileIndex = 0;
        startFileIndex = 0;

        SdCardSupport::file.close();

        totalFiles = SdCardSupport::countSnapshotFiles();
        oled.print(",");
        oled.print(totalFiles);

        Z80Bus::clearScreen(COL::BLACK_WHITE);
        displayItemList(startFileIndex);

      } else {
        return actualFileIndex;
      }

    } else if (action == ACTION_REFRESH_LIST) {
      displayItemList(startFileIndex);
    }
    Utils::frameDelay(start);
  }
}
