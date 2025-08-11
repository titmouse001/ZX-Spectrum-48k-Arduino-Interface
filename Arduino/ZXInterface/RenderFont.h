#ifndef SMALLFONT_H
#define SMALLFONT_H

#include <stdint.h>

namespace RenderFont {
    
void processCharacter(uint8_t* finalOutput, const uint8_t* fontPtr, uint16_t basePos);
uint8_t prepareTextGraphics(uint8_t* finalOutput, const char* message);
uint8_t prepareTextGraphics_P(uint8_t* finalOutput, const __FlashStringHelper* flashStr);

}

#endif
