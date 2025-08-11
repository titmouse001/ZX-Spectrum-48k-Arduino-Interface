#ifndef DRAW_H
#define DRAW_H

#include <stdint.h>
#include "WString.h"   // __FlashStringHelper

namespace Draw {

void textLine(int ypos, const char *message);
void drawTextInternal(int xpos, int ypos, uint8_t charCount);
void text_P(int xpos, int ypos, const __FlashStringHelper *flashStr);
void text(int xpos, int ypos, const char *message);

}

#endif