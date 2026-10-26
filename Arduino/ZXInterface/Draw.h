#ifndef DRAW_H
#define DRAW_H

#include <stdint.h>
#include "WString.h" // __FlashStringHelper

namespace Draw {

    void textCore(uint8_t xpos, uint8_t ypos, const void *str, bool isFlash);
        inline void text_P(uint8_t xpos, uint8_t ypos, const __FlashStringHelper *flashStr) {
        textCore(xpos, ypos, flashStr, true);
    }
    inline void text(uint8_t xpos, uint8_t ypos, const char *message) {
        textCore(xpos, ypos, message, false);
    }
   // void textLineOLD(uint8_t ypos, const char *message);
    void textLine(uint16_t addr, const char *message);
    void textLine(const char *message,  uint8_t *workingData);

    void drawTextInternal(uint8_t xpos, uint8_t ypos, uint8_t charCount, uint8_t *outputLine);
}



// Draw - it's nothing more than a util class... but was hoping namespace would save a few bytes!!!!
// as a class : sketch uses 29432 bytes (95%)
// as namespace : Sketch uses 29432 bytes (95%) 

#endif