#ifndef MENU_H
#define MENU_H

#include <stdint.h>
#include "Constants.h"
#include "FontData.h"
//#include "SdFat.h" 

class FatFile;

class Menu {
public:
  enum Button_t {
    BUTTON_NONE,
    BUTTON_BACK,
    BUTTON_MENU,
    BUTTON_ADVANCE
  };
  enum MenuAction_t {
    ACTION_NONE,
    ACTION_SELECT_FILE,
    ACTION_MOVE_UP,
    ACTION_MOVE_DOWN,
    ACTION_REFRESH_LIST
  };

  static FatFile* handleMenu();
  static Button_t getButton();

  __attribute__((optimize("-Os"), noinline)) 
  static inline void waitForRelease() { while (Menu::getButton() != Menu::BUTTON_NONE);  }
  __attribute__((optimize("-Os"), noinline)) 
  static inline void waitForAnyKey() { while (Menu::getButton() == Menu::BUTTON_NONE); waitForRelease(); }

  static void resetToRoot();
  static bool inSubFolder;
private:
  static uint32_t lastButtonPressTime;
  static uint32_t lastButtonHoldTime;
  static uint16_t buttonDelay;
  static uint16_t currentFileIndex;
  static uint16_t startFileIndex;
  static bool buttonHeld;


  static void displayFileList();
  static uint16_t scanFolder(bool reset = false);
  static MenuAction_t getMenuAction(uint16_t totalFiles);

};

#endif