#pragma once

#include <stdint.h>

#define VERSION ("0.26")  // Arduino firmware

// Comment out '_DEBUG_POOL_SIZE_ENABLED_' for release
//#define _DEBUG_POOL_SIZE_ENABLED_  // adds/removes: vars + limit error + screen logging

// Z80 CPU: Noticed 48K Spectrums will reset OK with a pulse around 10ms, but a 
// 128k spectrum like a +2B needs a longer time around 250ms.
constexpr uint16_t Z80_RESET_TIME = 250;  // milliseconds

// How deep we can navigate into sub-folders
constexpr uint8_t FOLDER_NAV_DEPTH = 5;

// Saves the current game state when navigating to the in-game pause menu
static constexpr const char* SCRATCH_FILE = "scratch16384.SCR";

// Screen shots folder -  incrementing "SHOT0000.SCR" files are saved here.
//static constexpr const char* SHOTS_FOLDER = "SHOTS";
static const char* SHOTS_FOLDER = "SHOTS";
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
    constexpr uint8_t BRIGHT_BLUE_BLACK = Attr(BLUE, BLACK, true);
    constexpr uint8_t BRIGHT_MAGENTA_BLACK = Attr(MAGENTA, BLACK, true);
    constexpr uint8_t BRIGHT_YELLOW_RED = Attr(YELLOW, RED, true);
    constexpr uint8_t BRIGHT_BLACK_GREEN = Attr(BLACK, GREEN, true);
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
// .Z80 FILE FORMAT - HEADER ver1
//-----------------------------------------
constexpr uint8_t Z80_V1_AF_HIGH = 0;
constexpr uint8_t Z80_V1_AF_LOW = 1;
constexpr uint8_t Z80_V1_BC_LOW = 2;
constexpr uint8_t Z80_V1_BC_HIGH = 3;
constexpr uint8_t Z80_V1_HL_LOW = 4;
constexpr uint8_t Z80_V1_HL_HIGH = 5;
constexpr uint8_t Z80_V1_PC_LOW = 6;
constexpr uint8_t Z80_V1_PC_HIGH = 7;
constexpr uint8_t Z80_V1_SP_LOW = 8;
constexpr uint8_t Z80_V1_SP_HIGH = 9;
constexpr uint8_t Z80_V1_I = 10;
constexpr uint8_t Z80_V1_R_7BITS = 11;
constexpr uint8_t Z80_V1_FLAGS1 = 12; // Bit 0: R bit 7; Bits 1-3: Border; Bit 5: Compressed (0=no, 1=yes)
constexpr uint8_t Z80_V1_DE_LOW = 13;
constexpr uint8_t Z80_V1_DE_HIGH = 14;
constexpr uint8_t Z80_V1_BC_PRIME_LOW = 15;
constexpr uint8_t Z80_V1_BC_PRIME_HIGH = 16;
constexpr uint8_t Z80_V1_DE_PRIME_LOW = 17;
constexpr uint8_t Z80_V1_DE_PRIME_HIGH = 18;
constexpr uint8_t Z80_V1_HL_PRIME_LOW = 19;
constexpr uint8_t Z80_V1_HL_PRIME_HIGH = 20;
constexpr uint8_t Z80_V1_AF_PRIME_HIGH = 21;
constexpr uint8_t Z80_V1_AF_PRIME_LOW = 22;
constexpr uint8_t Z80_V1_IY_LOW = 23;
constexpr uint8_t Z80_V1_IY_HIGH = 24;
constexpr uint8_t Z80_V1_IX_LOW = 25;
constexpr uint8_t Z80_V1_IX_HIGH = 26;
constexpr uint8_t Z80_V1_IFF1 = 27;
constexpr uint8_t Z80_V1_IFF2 = 28;
constexpr uint8_t Z80_V1_IM_AND_FLAGS2 = 29; // Bits 0-1: IM; Bit 2: Issue 2; Bit 3: Double int; Bits 4-5: Video sync; Bits 6-7: Joystick
constexpr uint8_t Z80_V1_HEADERLENGTH = 30;  // TOTAL Ver1 ITEMS

//-----------------------------------------
// .Z80 FILE FORMAT - EXTENDED HEADERS ver2/3
//-----------------------------------------
constexpr uint8_t Z80_EXT_PC_LOW = 0;
constexpr uint8_t Z80_EXT_PC_HIGH = 1;
constexpr uint8_t Z80_EXT_HW_MODE = 2;
constexpr uint8_t Z80_EXT_7FFD_PORT = 3; // Last OUT to 0x7FFD (128K machine) or SamRam (48K)
constexpr uint8_t Z80_EXT_IF1_PAGE_STATUS = 4; // 0xFF if IF1 ROM is paged, 0x00 if not
constexpr uint8_t Z80_EXT_FLAGS3 = 5; // Bit 0: R bit 7 (different from V1); Bit 1: LDIR active; Bit 2: AY-3-8912 active (48K)
constexpr uint8_t Z80_EXT_FFFD_PORT = 6; // Last OUT to 0xFFFD
constexpr uint8_t Z80_EXT_AY_REGS_START = 7; // AY-3-8912 registers (16 bytes)

constexpr uint8_t Z80_V2_HEADERLENGTH = 23; // Length of the extended header for V2
constexpr uint8_t Z80_V3_HEADERLENGTH = 54; // Length of the extended header for V3
constexpr uint8_t Z80_V3X_HEADERLENGTH = 55; //

//-----------------------------------------
// .SNA FILE FORMAT - (27 byte header)
//-----------------------------------------

#define SNA_I           0
#define SNA_HL_PRIME_LOW 1
#define SNA_HL_PRIME_HIGH 2
#define SNA_DE_PRIME_LOW 3
#define SNA_DE_PRIME_HIGH 4
#define SNA_BC_PRIME_LOW 5
#define SNA_BC_PRIME_HIGH 6
#define SNA_AF_PRIME_LOW 7
#define SNA_AF_PRIME_HIGH 8
#define SNA_HL_LOW      9
#define SNA_HL_HIGH     10
#define SNA_DE_LOW      11
#define SNA_DE_HIGH     12
#define SNA_BC_LOW      13
#define SNA_BC_HIGH     14
#define SNA_IY_LOW      15
#define SNA_IY_HIGH     16
#define SNA_IX_LOW      17
#define SNA_IX_HIGH     18
#define SNA_IFF2_BIT    19 // IFF2 bit (bit 2 of this byte)
#define SNA_R_REGISTER  20
#define SNA_AF_LOW      21
#define SNA_AF_HIGH     22
#define SNA_SP_LOW      23
#define SNA_SP_HIGH     24
#define SNA_IM_MODE     25
#define SNA_BORDER_COLOUR 26
#define SNA_TOTAL_ITEMS    27  // TOTAL ITEMS

constexpr uint16_t SNAPSHOT_FILE_SIZE = ZX_SPECTRUM_48K_TOTAL_MEMORY + SNA_TOTAL_ITEMS;  // total: 49179 for all .sna files

//-----------------------------------------


enum BlockReadResult : uint8_t {
    BLOCK_SUCCESS = 0,
    BLOCK_END_OF_FILE = 1,
    BLOCK_UNSUPPORTED_PAGE = 2,
    BLOCK_ERROR = 13
};

enum MachineType : uint8_t{
    MACHINE_48K,
    MACHINE_128K,
    MACHINE_UNKNOWN
};

enum Z80HeaderVersion : uint8_t {
    Z80_VERSION_UNKNOWN = 0,
    Z80_VERSION_1 = 1,
    Z80_VERSION_2 = 2,
    Z80_VERSION_3 = 3
};

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

//-----------------------------------------

//static constexpr uint16_t FILE_READ_BUFFER_SIZE = 128;
static constexpr uint8_t FILE_READ_BUFFER_SIZE = 128;
static constexpr uint8_t MAX_FILENAME_LEN = 64; 
static constexpr uint8_t ZX_FILENAME_MAX_DISPLAY_LEN = ZX_SCREEN_WIDTH_PIXELS / FONT_WIDTH_WITH_GAP;


//-----------------------------------------

enum PauseMenu : uint8_t {
  Resume,
  SaveSNA,
  SlowMo,
  Cheats,
  Screenshot,
  MemView,
  Exit
};


//-----------------------------------------


