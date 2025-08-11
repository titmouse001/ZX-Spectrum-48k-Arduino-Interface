#include <stdint.h>
#include "Arduino.h"
#include "FontData.h"
#include "utils.h"
#include "constants.h"
#include "RenderFont.h"

__attribute__((optimize("-Ofast")))
void RenderFont::processCharacter(uint8_t* finalOutput, const uint8_t *fontPtr, uint16_t basePos) {
    const uint8_t d0 = pgm_read_byte(&fontPtr[0]);
    const uint8_t d1 = pgm_read_byte(&fontPtr[1]);
    const uint8_t d2 = pgm_read_byte(&fontPtr[2]);
    const uint8_t d3 = pgm_read_byte(&fontPtr[3]);
    const uint8_t d4 = pgm_read_byte(&fontPtr[4]);

    #define PROCESS_ROW(r) do { \
        const uint8_t transposedRow = \
            (((d0 >> r) & 1) << 4) | \
            (((d1 >> r) & 1) << 3) | \
            (((d2 >> r) & 1) << 2) | \
            (((d3 >> r) & 1) << 1) | \
            ((d4 >> r) & 1); \
        const uint16_t bitPosition = (SmallFont::FNT_BUFFER_SIZE * r) * 8 + basePos; \
        Utils::join6Bits(finalOutput, transposedRow, bitPosition); \
    } while(0)

    PROCESS_ROW(0); PROCESS_ROW(1); PROCESS_ROW(2); PROCESS_ROW(3);
    PROCESS_ROW(4); PROCESS_ROW(5); PROCESS_ROW(6);
    
    #undef PROCESS_ROW
}

__attribute__((optimize("-Ofast")))
uint8_t RenderFont::prepareTextGraphics(uint8_t* finalOutput, const char *message) {
    Utils::memsetZero(finalOutput, SmallFont::FNT_BUFFER_SIZE * SmallFont::FNT_HEIGHT);

    uint8_t charCount = 0;
    uint16_t basePos = 0;
    const char firstChar = message[0];
//    if (firstChar == '\0') return 0; 
    // NOTE: With this logic font width is fixed at 5 pixels. 
    const uint8_t *fontPtr=&fudged_Adafruit5x7[((firstChar-0x20)<<2)+(firstChar-0x20)+SmallFont::FNT_HEADER_SIZE];

    while (true) {
        processCharacter(finalOutput, fontPtr, basePos);
        charCount++;
        basePos += SmallFont::FNT_CHAR_PITCH;
        fontPtr += SmallFont::FNT_WIDTH; // Move to next glyph
        const char ch = message[charCount];
        if (ch=='\0') break;
        // NOTE: Font width is fixed at 5 pixels. 
        fontPtr = &fudged_Adafruit5x7[((ch-0x20)<<2)+(ch-0x20)+SmallFont::FNT_HEADER_SIZE];
    }
    return charCount;
}


// __attribute__((optimize("-Ofast")))
// inline uint8_t prepareTextGraphics(byte* finalOutput, const char *message) {
//     Utils::memsetZero(&finalOutput[0], SmallFont::FNT_BUFFER_SIZE * SmallFont::FNT_HEIGHT);
//     uint8_t charCount = 0;
//     for (uint8_t i = 0; message[i] != '\0'; i++) {   
//         const uint8_t *fontPtr = &fudged_Adafruit5x7[((message[i] - 0x20) * SmallFont::FNT_WIDTH) + SmallFont::FNT_HEADER_SIZE];
//         const uint16_t basePos = i * SmallFont::FNT_CHAR_PITCH;
//         processCharacter(finalOutput, fontPtr, basePos);
//         charCount++;
//     }
//     return charCount;
// }

uint8_t RenderFont::prepareTextGraphics_P(uint8_t* finalOutput, const __FlashStringHelper *flashStr) {
    Utils::memsetZero(&finalOutput[0], SmallFont::FNT_BUFFER_SIZE * SmallFont::FNT_HEIGHT);
    
    const char *message = (const char *)flashStr;
    uint8_t charCount = 0;
    
    char ch;
    while ((ch = pgm_read_byte(message++)) != 0) {
        const uint8_t *fontPtr = &fudged_Adafruit5x7[((ch - 0x20) * SmallFont::FNT_WIDTH) + SmallFont::FNT_HEADER_SIZE];
        const uint16_t basePos = charCount * SmallFont::FNT_CHAR_PITCH;
        
        processCharacter(finalOutput, fontPtr, basePos);
        charCount++;
    }
    return charCount;
}


