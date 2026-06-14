#ifndef _PAUSE_MENU_
#define _PAUSE_MENU_

#include <stdint.h>

namespace InGamePauseMenu {

    uint8_t getSelectedMenuOption(uint8_t& selectedIndex);
    void waitForUserExit();
    bool process();

    enum OPTIONS_PM{
        RESUME,
        SAVE_SNA,
        SLOWMO,
        CHEATS,
        SCREENSHOT,
        MEM_VIEW,
        EXIT
    };

}

#endif
