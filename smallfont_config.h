#ifndef SMALLFONT_CONFIG_H
#define SMALLFONT_CONFIG_H

#include <stdint.h>

namespace SmallFont {
    constexpr uint8_t FNT_WIDTH         = 5;   // character width in pixels
    constexpr uint8_t FNT_HEIGHT        = 7;   // character height in pixels
    constexpr uint8_t FNT_GAP           = 1;   // horizontal spacing
    constexpr uint8_t FNT_BUFFER_SIZE   = 32;  // size of the on‐screen text buffer
    constexpr uint8_t CHAR_PITCH        = FNT_WIDTH + FNT_GAP; // 6 pixels per character slot
}

#endif
