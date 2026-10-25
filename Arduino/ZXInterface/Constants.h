#pragma once

#include <stdint.h>

// ----------------------------------------------------------------
// Arduino Firmware
#define VERSION ("0.28")  
// ----------------------------------------------------------------

// This Arduino firmware requires a minimum PCB revision of 2.5.
// see: ZX-Spectrum-48k-Arduino-Interface\Documents\Gerbers\V2.50\
        Gerber_ZX-Spectrum-48k-Arduino-Interface-Game-Loader-V2_5_PCB_ZX-Spectrum-Interface_2026-08-11.zip

// Comment out '_DEBUG_POOL_SIZE_ENABLED_' for release
//#define _DEBUG_POOL_SIZE_ENABLED_  // adds/removes: vars + limit error + screen logging

// Z80 CPU: Noticed 48K Spectrums will reset OK with a pulse around 10ms, but a 
// 128k spectrum like a +2B needs a longer time around 250ms.
constexpr uint16_t Z80_RESET_TIME = 250;  // milliseconds

// How deep we can navigate into sub-folders
constexpr uint8_t FOLDER_NAV_DEPTH = 5;

// Saves the current game state when navigating to the in-game pause menu
static constexpr const char* SCRATCH_FILE = "scratch16384.SCR";

// Screen shots folder
static const char* SHOTS_FOLDER = "SHOTS";
// New folder Metadata
static constexpr char GAMEINFO_DAT[] = "GameInfo.dat";

/* -------------------------------------------------
 * ZX Spectrum Screen Attribute Byte Format
 * -------------------------------------------------
 * Bit:   7   6   5    4    3    2    1    0
 *       [F | B | P2 | P1 | P0 | I2 | I1 | I0]
 *        |   |  PAPER (3bits) |  INK (3 bits)
 *     FLASH BRIGHT
 *
 *   000 = Black, 001 = Blue, 010 = Red, 011 = Magenta,
 *   100 = Green, 101 = Cyan, 110 = Yellow, 111 = White
 */

namespace COL {
    // 1. Base ZX Spectrum Colors (3 bits: Green | Red | Blue)
    constexpr uint8_t BLACK = 0;
    constexpr uint8_t BLUE = 1;
    constexpr uint8_t RED = 2;
    constexpr uint8_t MAGENTA = 3;
    constexpr uint8_t GREEN = 4;
    constexpr uint8_t CYAN = 5;
    constexpr uint8_t YELLOW = 6;
    constexpr uint8_t WHITE = 7;
    constexpr uint8_t BRIGHT = 0x40;  // Bit 6: Brightness
    constexpr uint8_t FLASH = 0x80;   // Bit 7: Flashing

    constexpr uint8_t Attr(uint8_t paper, uint8_t ink, bool bright = false,
                           bool flash = false) {
      return (flash ? FLASH : 0) | (bright ? BRIGHT : 0) |
             ((paper & 0x07) << 3) | (ink & 0x07);
    }

    //                <BRIGHT>_PAPER_INK
    constexpr uint8_t BLACK_WHITE = Attr(BLACK, WHITE);
    constexpr uint8_t BLACK_GREEN = Attr(BLACK, GREEN);
    constexpr uint8_t BLACK_MAGENTA = Attr(BLACK, MAGENTA);
    constexpr uint8_t BLACK_BLUE = Attr(BLACK, BLUE);
    
    constexpr uint8_t GREEN_WHITE = Attr(GREEN, WHITE);
    constexpr uint8_t MAGENTA_BLACK = Attr(MAGENTA, BLACK);
    constexpr uint8_t BLUE_BLACK = Attr(BLUE, BLACK);
    constexpr uint8_t CYAN_BLACK = Attr(CYAN, BLACK);
    constexpr uint8_t BLACK_BLACK = Attr(BLACK, BLACK);  // for hiding screen
    constexpr uint8_t FLASH_BLACK_RED = Attr(BLACK, RED, false, true);
    constexpr uint8_t FLASH_RED_WHITE = Attr(RED, WHITE, false, true);
    constexpr uint8_t FLASH_RED_BLUE = Attr(RED, BLUE, false, true);
    constexpr uint8_t BLUE_WHITE = Attr(BLUE, WHITE);
    constexpr uint8_t BRIGHT_BLUE_WHITE = Attr(BLUE, WHITE, true);
    constexpr uint8_t GREEN_BLACK = Attr(GREEN, BLACK);
    constexpr uint8_t YELLOW_BLACK = Attr(YELLOW, BLACK);
    constexpr uint8_t MAGENTA_WHITE = Attr(MAGENTA, WHITE);
    constexpr uint8_t BRIGHT_BLACK_WHITE = Attr(BLACK, WHITE, true);
    constexpr uint8_t BRIGHT_BLACK_BLUE = Attr(BLACK, BLUE, true);
    constexpr uint8_t BRIGHT_CYAN_BLACK = Attr(CYAN, BLACK, true);
    constexpr uint8_t BLACK_CYAN = Attr(BLACK,CYAN);
    constexpr uint8_t BRIGHT_BLACK_CYAN = Attr(BLACK,CYAN,true);
    constexpr uint8_t BRIGHT_BLUE_BLACK = Attr(BLUE, BLACK, true);
    constexpr uint8_t BRIGHT_MAGENTA_BLACK = Attr(MAGENTA, BLACK, true);
    constexpr uint8_t BRIGHT_YELLOW_RED = Attr(YELLOW, RED, true);
    constexpr uint8_t BRIGHT_BLACK_GREEN = Attr(BLACK, GREEN, true);
    constexpr uint8_t BRIGHT_GREEN_BLACK = Attr(GREEN, BLACK,true);
    constexpr uint8_t BRIGHT_YELLOW_BLACK = Attr(YELLOW, BLACK,true);
}  // namespace COL

//-----------------------------------------
// BUTTON INPUTS
constexpr uint8_t INPUT_MASK = 0b00111111;
constexpr uint8_t INPUT_FIRE1 = 0b00010000;
constexpr uint8_t INPUT_FIRE2 = 0b00100000;
//constexpr uint8_t JOYSTICK_DOWN = 0b00000100;
constexpr uint8_t INPUT_SELECT = 0b01000000;
constexpr uint8_t MAX_BUTTON_READ_MILLISECONDS  = 1000 / 50;  // 50 FPS

//-----------------------------------------
// ZX SPECTRUM 48k MACHINE CONSTANTS
//-----------------------------------------
constexpr uint16_t ZX_SCREEN_ADDRESS_START      = 0x4000;
constexpr uint16_t ZX_SCREEN_BITMAP_SIZE        = 6144;  // 192 x (255/8)
constexpr uint16_t ZX_SCREEN_ATTR_ADDRESS_START = ZX_SCREEN_ADDRESS_START + ZX_SCREEN_BITMAP_SIZE;  
constexpr uint16_t ZX_SCREEN_ATTR_SIZE          = 768;   // (192/8) x (255/8);
constexpr uint16_t ZX_SCREEN_TOTAL_SIZE         = ZX_SCREEN_BITMAP_SIZE + ZX_SCREEN_ATTR_SIZE;
constexpr uint16_t ZX_SPECTRUM_48K_TOTAL_MEMORY = 1024UL * 48;   // 48k (above 16k ROM) 
constexpr uint16_t ZX_SCREEN_WIDTH_PIXELS        = 256;
constexpr uint16_t ZX_SCREEN_HEIGHT_PIXELS       = 192;
constexpr uint16_t ZX_SCREEN_WIDTH_BYTES        = ZX_SCREEN_WIDTH_PIXELS/8;
constexpr uint16_t ZX_SCREEN_HEIGHT_BYTES        = ZX_SCREEN_HEIGHT_PIXELS/8;

//-----------------------------------------
// FONT SUPPORT
//-----------------------------------------

namespace SmallFont {
    constexpr uint8_t FNT_WIDTH         = 5;   // character width in pixels
    constexpr uint8_t FNT_HEIGHT        = 7;   // character height in pixels
    constexpr uint8_t FNT_GAP           = 1;   // horizontal spacing
    constexpr uint8_t FNT_BUFFER_SIZE   = 32;  // size of the on‐screen text buffer
    constexpr uint8_t FNT_CHAR_PITCH    = FNT_WIDTH + FNT_GAP; // 6 pixels per character slot
    constexpr uint8_t FNT_HEADER_SIZE   = 6;
}

static constexpr uint8_t FONT_HEIGHT_WITH_GAP = SmallFont::FNT_HEIGHT + SmallFont::FNT_GAP;
static constexpr uint8_t FONT_WIDTH_WITH_GAP = SmallFont::FNT_WIDTH + SmallFont::FNT_GAP;
static constexpr uint8_t SCREEN_TEXT_ROWS = ZX_SCREEN_HEIGHT_PIXELS / FONT_HEIGHT_WITH_GAP; //24;

static constexpr uint16_t MAX_REPEAT_KEY_DELAY = 300;
static constexpr uint16_t MIN_REPEAT_KEY_DELAY = 40;
static constexpr uint16_t ADJUST_REPEAT_KEY_DELAY = 20;

static constexpr uint8_t FILE_READ_BUFFER_SIZE = 128;
static constexpr uint8_t MAX_FILENAME_LEN = 64; 
static constexpr uint8_t ZX_FILENAME_MAX_DISPLAY_LEN = ZX_SCREEN_WIDTH_PIXELS / FONT_WIDTH_WITH_GAP;

