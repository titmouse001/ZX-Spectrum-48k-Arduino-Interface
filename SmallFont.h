#ifndef SMALLFONT_H
#define SMALLFONT_H

#include "fudgefont.h"  // Based on the Adafruit5x7 font, with '!' to '(' changed to work as a VU BAR (8 chars)

namespace SmallFont {
 
  /* Fixed‐width bitmap font: */
  static const uint8_t FNT_WIDTH         = 5;   // character width in pixels (5 px per char) 
  static const uint8_t FNT_HEIGHT        = 7;   // character height in pixels (7 px per char)
  static const uint8_t FNT_GAP           = 1;   // horizontal spacing between adjacent characters (1 px)
  static const uint8_t FNT_BUFFER_SIZE   = 32;  // size of the on‐screen text buffer (enough bytes to hold one full line)

__attribute__((optimize("-Ofast")))
uint8_t prepareTextGraphics(byte* finalOutput, const char *message) {
  Utils::memsetZero(&finalOutput[0], FNT_BUFFER_SIZE * FNT_HEIGHT);
  uint8_t charCount = 0;

  for (uint8_t i = 0; message[i] != '\0'; i++) {
    // font characters in flash
    const uint8_t *ptr = &fudged_Adafruit5x7[((message[i] - 0x20) * FNT_WIDTH) + 6];

    const uint8_t d0 = pgm_read_byte(&ptr[0]);
    const uint8_t d1 = pgm_read_byte(&ptr[1]);
    const uint8_t d2 = pgm_read_byte(&ptr[2]);
    const uint8_t d3 = pgm_read_byte(&ptr[3]);
    const uint8_t d4 = pgm_read_byte(&ptr[4]);

    // build each row’s transposed byte
    for (uint8_t row = 0; row < FNT_HEIGHT; row++) {
      // bit 4 comes from column 0, bit 3 from col 1 ... bit 0 from col 4
      const uint8_t transposedRow =
          (((d0 >> row) & 0x01) << 4) |
          (((d1 >> row) & 0x01) << 3) |
          (((d2 >> row) & 0x01) << 2) |
          (((d3 >> row) & 0x01) << 1) |
          (((d4 >> row) & 0x01) << 0);

      // compute bit-offset into the big output buffer
      const uint16_t bitPosition = (FNT_BUFFER_SIZE * row) * 8 + (i * (FNT_WIDTH + FNT_GAP));
      Utils::joinBits(finalOutput,transposedRow, FNT_WIDTH + FNT_GAP, bitPosition);
    }
    charCount++;
  }
  return charCount;
}

}


#endif 