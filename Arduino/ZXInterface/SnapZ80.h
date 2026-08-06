#ifndef Z80_NAPSHOT_H
#define Z80_NAPSHOT_H

#include <stdint.h>
#include "Constants.h"
#include "src/fatlib/SdFat.h"
#include "utils.h"

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
bool locateV1Terminator(FatFile* pFile, uint32_t start_pos, uint32_t& rle_data_length);
BlockReadResult readAndWriteBlock(FatFile* pFile);
bool convertSendZ80toSNA(FatFile* pFile, Z80HeaderInfo* headerInfo, Z80Registers* regs);
void decodeRLE_core(FatFile* pFile, uint16_t sourceLengthLimit, uint16_t currentAddress);
void sendRawBytes_core(FatFile* pFile, uint16_t length, uint16_t currentAddress);

void convertZ80HeaderToSna(const Z80V1Header* z80Header_v1, Z80Registers* regs);

}

#endif
