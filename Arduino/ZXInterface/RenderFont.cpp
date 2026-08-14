#include <stdint.h>
#include "Arduino.h"
#include "FontData.h"
#include "Constants.h"
#include "RenderFont.h"

// OLD METHOD -Ofast = 169
// NEW METHOD -O2 = 130, -Ofast = 124
__attribute__((optimize("-Ofast")))
uint8_t RenderFont::prepareTextInternal(uint8_t* finalOutput, const char* message, bool inFlash) {
  memset(finalOutput, 0, SmallFont::FNT_BUFFER_SIZE * SmallFont::FNT_HEIGHT);
  if (message == NULL) return 0;
  uint8_t charCount = 0;
  uint8_t basePos = 0;
  while (true) {
    const char ch = inFlash ? pgm_read_byte(&message[charCount]) : message[charCount];
    if (!ch) break;
    if (ch != ' ') {
      constexpr uint8_t SKIP_SPACE = 1; 
      const uint8_t idx = (ch - (0x20 + SKIP_SPACE) );
      const uint8_t* fontPtr = &precalced_font5x7[idx * 7];
      uint8_t bitOffset = basePos & 7;
      uint8_t* outPtr = finalOutput + (basePos >> 3);
      for (uint8_t r = 0; r < SmallFont::FNT_HEIGHT; ++r) {
        uint8_t rowData = pgm_read_byte(fontPtr++);
        // Because the character pitch is 6, bitOffset will ONLY ever be 6, 4, 2 or 0.
        switch (bitOffset) {
          case 0:
            *outPtr |= (rowData << 2);
            break;
          case 6:
            *outPtr |= (rowData >> 4);
            *(outPtr + 1) |= (rowData << 4);
            break;
          case 4:
            *outPtr |= (rowData >> 2);
            *(outPtr + 1) |= (rowData << 6);
            break;
          case 2:
            *outPtr |= rowData;
            break;
        }
        outPtr += SmallFont::FNT_BUFFER_SIZE;  // down one row
      }
    }
    charCount++;
    basePos += SmallFont::FNT_CHAR_PITCH;
  }
  return charCount;
}

uint8_t RenderFont::prepareTextGraphics(uint8_t* finalOutput, const char* message) {
    return prepareTextInternal(finalOutput, message, false); 
}

uint8_t RenderFont::prepareTextGraphics_P(uint8_t* finalOutput, const __FlashStringHelper* flashStr) {
    return prepareTextInternal(finalOutput, (const char*)flashStr, true);  
}

// OLD MTHOD WITH ON THE FLY TRANSPOSE (uses fudged_Adafruit5x7)
//  TIMING : 169

// #include <stdint.h>
// #include "Arduino.h"
// #include "FontData.h"
// #include "utils.h"
// #include "RenderFont.h"

// #define PROCESS_ROW(r) do { \
//     const uint8_t transposedRow = \
//         ((d0 & 1) << 4) | \
//         ((d1 & 1) << 3) | \
//         ((d2 & 1) << 2) | \
//         ((d3 & 1) << 1) | \
//          (d4 & 1); \
//     const uint16_t bitPosition = (SmallFont::FNT_BUFFER_SIZE * r) * 8 + basePos; \
//     Utils::join6Bits(finalOutput, transposedRow, bitPosition); \
//     d0 >>= 1; d1 >>= 1; d2 >>= 1; d3 >>= 1; d4 >>= 1; \
// } while(0)

// __attribute__((optimize("-Ofast")))
// void RenderFont::processCharacter(uint8_t* finalOutput, const uint8_t *fontPtr, uint16_t basePos) {
//     uint8_t d0 = pgm_read_byte(&fontPtr[0]);
//     uint8_t d1 = pgm_read_byte(&fontPtr[1]);
//     uint8_t d2 = pgm_read_byte(&fontPtr[2]);
//     uint8_t d3 = pgm_read_byte(&fontPtr[3]);
//     uint8_t d4 = pgm_read_byte(&fontPtr[4]);
    
//     PROCESS_ROW(0); PROCESS_ROW(1); PROCESS_ROW(2); 
//     PROCESS_ROW(3); PROCESS_ROW(4); PROCESS_ROW(5); 
//     PROCESS_ROW(6);
// }

// __attribute__((optimize("-Ofast"))) 
// uint8_t RenderFont::prepareTextInternal( uint8_t* finalOutput, const char* message, bool inFlash) {
//   Utils::memsetZero(finalOutput, SmallFont::FNT_BUFFER_SIZE * SmallFont::FNT_HEIGHT);
//   if (message == NULL) return 0;

//   uint8_t charCount = 0;
//   uint16_t basePos = 0;
//   while (true) {
//     const char ch = inFlash ? pgm_read_byte(&message[charCount]) : message[charCount];
//     if (!ch) break;  // end line early on null char
//     const uint8_t idx = (ch - 0x20);
//     const uint8_t* fontPtr = &fudged_Adafruit5x7[idx * 5];
//     processCharacter(finalOutput, fontPtr, basePos);
//     charCount++;
//     basePos += SmallFont::FNT_CHAR_PITCH;
//   }
//   return charCount;
// }

// __attribute__((optimize("-Ofast"))) 
// uint8_t RenderFont::prepareTextGraphics(uint8_t* finalOutput, const char* message) {
//     return prepareTextInternal(finalOutput, message, false); // read from RAM
// }

// __attribute__((optimize("-Ofast"))) 
// uint8_t RenderFont::prepareTextGraphics_P(uint8_t* finalOutput, const __FlashStringHelper* flashStr) {
//     return prepareTextInternal(finalOutput, (const char*)flashStr, true);  // read from Flas
// }