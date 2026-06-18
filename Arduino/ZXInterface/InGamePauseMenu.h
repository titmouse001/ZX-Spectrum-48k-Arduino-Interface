#ifndef _PAUSE_MENU_
#define _PAUSE_MENU_

#include <stdint.h>

namespace InGamePauseMenu {

    uint8_t getSelectedMenuOption(uint8_t& selectedIndex);
    void waitForUserExit();
    bool process();
    int32_t readNumericInput(uint8_t maxDigits, int xPos, int yPos, const char* name,  uint16_t min, uint16_t max) ;

    enum OPTIONS_PM{
        RESUME,
        SAVE_SNA,
        SLOWMO,
        POKE,
        SCREENSHOT,
        MEM_VIEW,
        EXIT
    };

}

#endif
