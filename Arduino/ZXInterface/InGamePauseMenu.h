#ifndef _PAUSE_MENU_
#define _PAUSE_MENU_

#include <stdint.h>

namespace InGamePauseMenu {

    uint8_t getSelectedMenuOption_Blocking(uint8_t& selectedIndex);
    void waitForUserExit(uint8_t borderColour);
    bool process(uint8_t borderColour);
    int32_t readNumericInput(uint8_t maxDigits, int xPos, int yPos, const char* name,  uint16_t min, uint16_t max) ;

    void handlePokeMenu();
    void handleScreenshotMenu();

    enum OPTIONS_PM{
        RESUME,
        SAVE_SNA,
        POKE,
        SCREENSHOT,
        MEM_VIEW,
        EXIT
    };

}

#endif
