#ifndef DRAW_H
#define DRAW_H

#include <stdint.h>
#include "WString.h" // __FlashStringHelper

namespace Draw {

        // The unified core function
    void textCore(int xpos, int ypos, const void *str, bool isFlash);
    
    inline void text_P(int xpos, int ypos, const __FlashStringHelper *flashStr) {
        textCore(xpos, ypos, flashStr, true);
    }
    
    inline void text(int xpos, int ypos, const char *message) {
        textCore(xpos, ypos, message, false);
    }
    
    void textLine(int ypos, const char *message);
    void drawTextInternal(int xpos, int ypos, uint8_t charCount, uint8_t *outputLine);
}



// Draw - it's nothing more than a util class... but was hoping namespace would save a few bytes!!!!
// as a class : sketch uses 29432 bytes (95%)
// as namespace : Sketch uses 29432 bytes (95%) 

#endif