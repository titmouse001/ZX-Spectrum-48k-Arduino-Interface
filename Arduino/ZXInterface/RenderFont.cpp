#include <stdint.h>
#include "Arduino.h"
#include "FontData.h"
#include "utils.h"
#include "constants.h"
#include "RenderFont.h"


#define PROCESS_ROW(r) do { \
    const uint8_t transposedRow = \
        ((d0 & 1) << 4) | \
        ((d1 & 1) << 3) | \
        ((d2 & 1) << 2) | \
        ((d3 & 1) << 1) | \
         (d4 & 1); \
    const uint16_t bitPosition = (SmallFont::FNT_BUFFER_SIZE * r) * 8 + basePos; \
    Utils::join6Bits(finalOutput, transposedRow, bitPosition); \
    d0 >>= 1; d1 >>= 1; d2 >>= 1; d3 >>= 1; d4 >>= 1; \
} while(0)

__attribute__((optimize("-Ofast")))
void RenderFont::processCharacter(uint8_t* finalOutput, const uint8_t *fontPtr, uint16_t basePos) {
    // Read fonts directly into local variables
    uint8_t d0 = pgm_read_byte(&fontPtr[0]);
    uint8_t d1 = pgm_read_byte(&fontPtr[1]);
    uint8_t d2 = pgm_read_byte(&fontPtr[2]);
    uint8_t d3 = pgm_read_byte(&fontPtr[3]);
    uint8_t d4 = pgm_read_byte(&fontPtr[4]);
    
    PROCESS_ROW(0); PROCESS_ROW(1); PROCESS_ROW(2); 
    PROCESS_ROW(3); PROCESS_ROW(4); PROCESS_ROW(5); 
    PROCESS_ROW(6);
}

__attribute__((optimize("-Ofast"))) 
uint8_t RenderFont::prepareTextGraphics(uint8_t* finalOutput, const char *message) {
    Utils::memsetZero(finalOutput, SmallFont::FNT_BUFFER_SIZE * SmallFont::FNT_HEIGHT);
    uint8_t charCount = 0;
    uint16_t basePos = 0;
    while (true) {
        const char ch = message[charCount];
        if (!ch) break;   // end line early on null char
        const uint8_t idx = (ch - 0x20);
        const uint8_t* fontPtr = &fudged_Adafruit5x7[(idx << 2) + idx + SmallFont::FNT_HEADER_SIZE];  // trick for 5 x idx
        processCharacter(finalOutput, fontPtr, basePos);
        charCount++;
        basePos += SmallFont::FNT_CHAR_PITCH;  
    }
    return charCount;
}

__attribute__((optimize("-Os"))) 
uint8_t RenderFont::prepareTextGraphics_P(uint8_t* finalOutput, const __FlashStringHelper *flashStr) {
    Utils::memsetZero(&finalOutput[0], SmallFont::FNT_BUFFER_SIZE * SmallFont::FNT_HEIGHT);
    const char *message = (const char *)flashStr;
    uint8_t charCount = 0;
    uint16_t basePos = 0; 
    char ch;
    
    while ((ch = pgm_read_byte(message++)) != 0) {
        const uint8_t idx = (ch - 0x20);
        const uint8_t *fontPtr = &fudged_Adafruit5x7[(idx << 2) + idx + SmallFont::FNT_HEADER_SIZE]; // id x 5
        processCharacter(finalOutput, fontPtr, basePos);
        charCount++;
        basePos += SmallFont::FNT_CHAR_PITCH;
    }
    return charCount;
}


