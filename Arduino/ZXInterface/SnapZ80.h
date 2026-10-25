#ifndef Z80_NAPSHOT_H
#define Z80_NAPSHOT_H

#include <stdint.h>
#include "Constants.h"
#include "src/fatlib/SdFat.h"
#include "utils.h"

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
// Z80 FILE FORMAT
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


namespace SnapZ80 {
    
	// 30-byte .Z80 Version 1 Header
struct __attribute__((packed)) Z80V1Header {
    uint8_t a, f;             // Bytes 0-1
    uint8_t c, b;             // Bytes 2-3
    uint8_t l, h;             // Bytes 4-5
    uint8_t pc_lo, pc_hi;     // Bytes 6-7
    uint8_t sp_lo, sp_hi;     // Bytes 8-9
    uint8_t i;                // Byte 10
    uint8_t r;                // Byte 11 (Low 7 bits)
    uint8_t flags1;           // Byte 12
    uint8_t e, d;             // Bytes 13-14 (Main DE)
    uint8_t c_prime, b_prime; // Bytes 15-16 (BC')
    uint8_t e_prime, d_prime; // Bytes 17-18 (DE')
    uint8_t l_prime, h_prime; // Bytes 19-20 (HL')
    uint8_t a_prime, f_prime; // Bytes 21-22 (AF')
    uint8_t iyl, iyh;         // Bytes 23-24
    uint8_t ixl, ixh;         // Bytes 25-26
    uint8_t iff1;             // Byte 27
    uint8_t iff2;             // Byte 28
    uint8_t flags2;           // Byte 29
};
static_assert(sizeof(Z80V1Header) == 30, "Z80V1Header must be exactly 30 bytes");


// Z80HeaderInfo: internal working structure - does not map to the Z80Snapshot formats
struct Z80HeaderInfo {
	Z80HeaderVersion version;
	uint8_t pc_low;
	uint8_t pc_high;
	uint8_t hw_mode;
	bool isV1Compressed;
	uint32_t v1PayloadLength = 0;

	//uint8_t headerV1Data[Z80_V1_HEADERLENGTH];
	Z80V1Header headerV1Data;
};

MachineType getMachineDetails(int8_t z80_version, uint8_t Z80_EXT_HW_MODE);
bool checkZ80FileValidity(FatFile* pFile, Z80HeaderInfo* headerInfo);
Z80HeaderVersion readZ80Header(FatFile* pFile, Z80HeaderInfo* headerInfo);
uint32_t locateV1Terminator(FatFile* pFile, uint32_t start_pos);
BlockReadResult readAndWriteBlock(FatFile* pFile);
bool convertSendZ80toSNA(FatFile* pFile, Z80HeaderInfo* headerInfo, Z80Registers* regs);
void decodeRLE_core(FatFile* pFile, uint16_t sourceLengthLimit, uint16_t currentAddress);
void sendRawBytes_core(FatFile* pFile, uint16_t length, uint16_t currentAddress);

void convertZ80HeaderToSna(const Z80V1Header* z80Header_v1, Z80Registers* regs);

}

#endif
