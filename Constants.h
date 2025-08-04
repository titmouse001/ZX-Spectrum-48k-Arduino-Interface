#pragma once

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

constexpr uint8_t Z80_V1_HEADERLENGTH = 30; // CORRECTED length from constants.h

// Z80 Snapshot Header V2/V3 extended header offsets (for picking out specific bytes)
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

// SNA header defines (from constants.h)
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
#define SNA_TOTAL_ITEMS    27


constexpr uint16_t SNAPSHOT_FILE_SIZE = (1024UL * 48) + (SNA_TOTAL_ITEMS);  //49179 (.sna files)


// New enum for Z80 file validity check results
enum Z80CheckResult {
    Z80_CHECK_SUCCESS = 0,
    Z80_CHECK_ERROR_OPEN_FILE = -1,
    Z80_CHECK_ERROR_READ_HEADER = -2,
    Z80_CHECK_ERROR_V1_MARKER_NOT_FOUND = -3,
    Z80_CHECK_ERROR_BLOCK_STRUCTURE = -4, // For V2/V3 block read issues
    Z80_CHECK_ERROR_UNEXPECTED_EOF = -5, // For general file truncation
    Z80_CHECK_ERROR_UNSUPPORTED_TYPE = -6, // For machine type check
    Z80_CHECK_ERROR_EOF = -7,
    Z80_CHECK_ERROR_SEEK = -8   
};

enum BlockReadResult {
    BLOCK_SUCCESS = 0,
    BLOCK_END_OF_FILE = 1,
    BLOCK_UNSUPPORTED_PAGE = 2,
    BLOCK_ERROR_READ_LENGTH = -21,
    BLOCK_ERROR_READ_PAGE = -22,
    BLOCK_ERROR_READ_DATA = -23,
    BLOCK_ERROR_VERSION = -24,
    BLOCK_ERROR_NO_V1_MARKER = -25
};

enum MachineType {
    MACHINE_48K,
    MACHINE_128K,
    MACHINE_UNKNOWN
};

enum Z80HeaderVersion {
    Z80_VERSION_UNKNOWN = -1,
    Z80_VERSION_1 = 1,
    Z80_VERSION_2 = 2,
    Z80_VERSION_3 = 3
};