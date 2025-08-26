#ifndef Z80_NAPSHOT_H
#define Z80_NAPSHOT_H

#include <stdint.h>
#include "Constants.h"
#include "SdFat.h" 

namespace SnapZ80 {

struct Z80HeaderInfo {
	int16_t version;
	uint8_t pc_low;
	uint8_t pc_high;
	uint8_t hw_mode;
	bool isV1Compressed;
};

MachineType getMachineDetails(int16_t z80_version, uint8_t Z80_EXT_HW_MODE);

Z80CheckResult checkZ80FileValidity(FatFile* pFile,const Z80HeaderInfo* headerInfo);
int16_t readZ80Header(FatFile* pFile, Z80HeaderInfo* headerInfo);
bool findMarkerOptimized(FatFile* pFile, int32_t start_pos, uint32_t& rle_data_length);
BlockReadResult z80_readAndWriteBlock(FatFile* pFile, uint8_t* page_number_out);
int16_t convertZ80toSNA_impl(FatFile* pFile,Z80HeaderInfo* headerInfo);
int16_t convertZ80toSNA(FatFile* pFile);
void decodeRLE_core(FatFile* pFile, uint16_t sourceLengthLimit, uint16_t currentAddress);

}

#endif
