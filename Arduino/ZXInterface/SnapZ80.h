#ifndef Z80_NAPSHOT_H
#define Z80_NAPSHOT_H

#include <stdint.h>
#include "Constants.h"
#include "SdFat.h" 

namespace SnapZ80 {

// Z80HeaderInfo: internal working structure - does not map to the Z80Snapshot formats
struct Z80HeaderInfo {
	Z80HeaderVersion version;
	uint8_t pc_low;
	uint8_t pc_high;
	uint8_t hw_mode;
	bool isV1Compressed;
	uint32_t v1PayloadLength = 0;

	uint8_t headerV1Data[Z80_V1_HEADERLENGTH];
};

MachineType getMachineDetails(int16_t z80_version, uint8_t Z80_EXT_HW_MODE);
bool checkZ80FileValidity(FatFile* pFile, Z80HeaderInfo* headerInfo);
Z80HeaderVersion readZ80Header(FatFile* pFile, Z80HeaderInfo* headerInfo);
Z80HeaderVersion readZ80HeaderInternal(FatFile* pFile, Z80HeaderInfo* headerInfo, uint8_t* buf);
bool locateV1Terminator(FatFile* pFile, uint32_t start_pos, uint32_t& rle_data_length);
bool locateV1TerminatorInternal(FatFile* pFile, uint32_t start_pos, uint32_t& rle_data_length, uint8_t* search_buffer);
BlockReadResult readAndWriteBlock(FatFile* pFile);
bool convertSendZ80toSNA(FatFile* pFile, Z80HeaderInfo* headerInfo, uint8_t* snaHeader);
void decodeRLE_core(FatFile* pFile, uint16_t sourceLengthLimit, uint16_t currentAddress);
void sendRawBytes_core(FatFile* pFile, uint16_t length, uint16_t currentAddress); 

}

#endif
