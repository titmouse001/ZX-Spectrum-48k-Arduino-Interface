#ifndef MENU_H
#define MENU_H

#include <stdint.h>
#include "Constants.h"

class Menu {
public:
  enum Button_t {
    BUTTON_NONE,
    BUTTON_BACK,
    BUTTON_SELECT,
    BUTTON_ADVANCE
  };
  enum MenuAction_t {
    ACTION_NONE,
    ACTION_SELECT_FILE,
    ACTION_MOVE_UP,
    ACTION_MOVE_DOWN,
    ACTION_REFRESH_LIST
  };
  static constexpr uint8_t SCREEN_TEXT_ROWS = 24;
  static constexpr uint8_t ZX_FILENAME_MAX_DISPLAY_LEN = 42;
  static constexpr uint8_t FONT_HEIGHT_WITH_GAP = SmallFont::FNT_HEIGHT + SmallFont::FNT_GAP;
  static constexpr uint16_t MAX_REPEAT_KEY_DELAY = 300;
  static uint16_t selectFileMenu(uint16_t totalFiles);
  static Button_t getButton();
  static MenuAction_t getMenuAction(uint16_t totalFiles);

private:
  static uint32_t lastButtonPress;
  static uint32_t lastButtonHoldTime;
  static uint16_t buttonDelay;
  static uint16_t oldHighlightAddress;
  static uint16_t currentFileIndex;
  static uint16_t startFileIndex;
  static bool buttonHeld;
  static void fileList(uint16_t startFileIndex);

};

#endif