#ifndef SMALLFONT_H
#define SMALLFONT_H

#include <Arduino.h>
#include "fudgefont.h" 

namespace SmallFont {
 
  static const uint8_t FNT_WIDTH         = 5;   // character width in pixels
  static const uint8_t FNT_HEIGHT        = 7;   // character height in pixels
  static const uint8_t FNT_GAP           = 1;   // horizontal spacing
  static const uint8_t FNT_BUFFER_SIZE   = 32;  // size of the on‐screen text buffer (enough bytes to hold one full line)
  static const uint8_t CHAR_PITCH        = FNT_WIDTH + FNT_GAP; // 6 pixels per character slot

// Need to transpose the fudged_Adafruit5x7 matrix and convert columns to rows
// to display text on the Speccy's horizontal scanlines.

__attribute__((optimize("-Ofast")))
uint8_t prepareTextGraphics(byte* finalOutput, const char *message) {
  Utils::memsetZero(&finalOutput[0], FNT_BUFFER_SIZE * FNT_HEIGHT);
  
  uint8_t charCount = 0;
  for (uint8_t i = 0; message[i] != '\0'; i++) {   
    const uint8_t *ptr = &fudged_Adafruit5x7[((message[i] - 0x20) * FNT_WIDTH) + 6];
    
    // Read all columns once and store
    const uint8_t d0 = pgm_read_byte(&ptr[0]);
    const uint8_t d1 = pgm_read_byte(&ptr[1]);
    const uint8_t d2 = pgm_read_byte(&ptr[2]);
    const uint8_t d3 = pgm_read_byte(&ptr[3]);
    const uint8_t d4 = pgm_read_byte(&ptr[4]);
    
    const uint16_t basePos = i * CHAR_PITCH;

    // Unroll the row loop for better performance (7 rows instead of 8)
    #define PROCESS_ROW(r) do { \
      const uint8_t transposedRow = \
          (((d0 >> r) & 1) << 4) | \
          (((d1 >> r) & 1) << 3) | \
          (((d2 >> r) & 1) << 2) | \
          (((d3 >> r) & 1) << 1) | \
          ((d4 >> r) & 1); \
      const uint16_t bitPosition = (FNT_BUFFER_SIZE * r) * 8 + basePos; \
      Utils::join6Bits(finalOutput, transposedRow, bitPosition); \
    } while(0)

    PROCESS_ROW(0); PROCESS_ROW(1); PROCESS_ROW(2); PROCESS_ROW(3);
    PROCESS_ROW(4); PROCESS_ROW(5); PROCESS_ROW(6);
    
    #undef PROCESS_ROW
    charCount++;
  }
  return charCount;
}

}


#endif 