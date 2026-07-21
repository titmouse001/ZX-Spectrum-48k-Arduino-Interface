#ifndef _PAUSE_MENU_
#define _PAUSE_MENU_

#include <stdint.h>

struct Z80Registers;

namespace InGamePauseMenu {

    uint8_t getSelectedMenuOption_Blocking(uint8_t& selectedIndex);
    void waitForUserExit(uint8_t borderColour);
    bool process(uint8_t borderColour);
    int32_t readNumericInput(uint8_t maxDigits, int xPos, int yPos, const char* name,  uint16_t min, uint16_t max) ;

    void handlePokeMenu();
    void handleScreenshotMenu();
    void handleSaveSnapshot(Z80Registers* z80Registers, const char* dirName);

    enum OPTIONS_PM : uint8_t {
        RESUME,
        SAVE_SNA,
        POKE,
        SCREENSHOT,
        MEM_VIEW,
        EXIT
    };
}

#endif
