#ifndef MENU_H
#define MENU_H

#include <stdint.h>
#include "Constants.h"
#include "SdFat.h" 

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

  static constexpr uint8_t FONT_HEIGHT_WITH_GAP = SmallFont::FNT_HEIGHT + SmallFont::FNT_GAP;
  static constexpr uint8_t FONT_WIDTH_WITH_GAP = SmallFont::FNT_WIDTH + SmallFont::FNT_GAP;
  static constexpr uint8_t SCREEN_TEXT_ROWS = ZX_SCREEN_HEIGHT_PIXELS / FONT_HEIGHT_WITH_GAP; //24;

  static constexpr uint8_t ZX_FILENAME_MAX_DISPLAY_LEN = ZX_SCREEN_WIDTH_PIXELS / FONT_WIDTH_WITH_GAP;
  //static constexpr uint8_t MAX_FILENAME_LEN = 64;  // ... maybe allow for future text scroll for longer lines
  static constexpr uint8_t MAX_SHORT_FILENAME_LEN = 13;

  static constexpr uint16_t MAX_REPEAT_KEY_DELAY = 300;
  static constexpr uint16_t MIN_REPEAT_KEY_DELAY = 40;
  static constexpr uint16_t ADJUST_REPEAT_KEY_DELAY = 20;

  static FatFile* handleMenu();
  static Button_t getButton();
  static MenuAction_t getMenuAction(uint16_t totalFiles);

private:
  static uint32_t lastButtonPressTime;
  static uint32_t lastButtonHoldTime;
  static uint16_t buttonDelay;
  static uint16_t oldHighlightAddress;
  static uint16_t currentFileIndex;
  static uint16_t startFileIndex;
  static bool buttonHeld;
  static bool inSubFolder;

  static void displayItemList(uint16_t startFileIndex);

  static uint16_t rescanFolder(bool reset = false);


};

#endif