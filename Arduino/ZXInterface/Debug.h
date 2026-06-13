#ifndef _DEBUG_H_
#define _DEBUG_H_

// ----------------------------------------------------------------------------------
// Put these back to enable debug options (I've - not used this in ages - should work!!!)

//#define DEBUG_OLED
//#define SERIAL_DEBUG

// Having both these enabled will EAT your flash program memory!!!
// (reduce buffer sizes for testing)

// ----------------------------------------------------------------------------------

namespace Debug {
    bool setupOled();
    void setupSerial();
}

#endif