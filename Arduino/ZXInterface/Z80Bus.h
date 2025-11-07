#ifndef Z80BUS_H
#define Z80BUS_H

#include <stdint.h>
#include "pin.h"
#include "SdFat.h"

namespace Z80Bus {

void setupPins();
extern void waitHalt();
extern void resetZ80();
extern void resetToSnaRom();
extern void waitRelease_NMI();
extern void sendBytes(uint8_t* data, uint16_t size);
extern void sendSnaHeader(uint8_t* header);
extern void fillScreenAttributes(const uint8_t col);
extern void sendFillCommand(uint16_t address, uint16_t amount, uint8_t color);
extern void sendSmallFillCommand(uint16_t address, uint8_t amount, uint8_t color);
extern void sendCopyCommand(uint16_t address, uint8_t amount);
extern void sendWaitVBLCommand();
extern void sendStackCommand(uint16_t addr);
extern void highlightSelection(uint16_t currentFileIndex, uint16_t startFileIndex, uint16_t& oldHighlightAddress);

//extern uint8_t GetKeyPulses_NO_LONGER_USED();
extern uint8_t getByte();

extern void encodeTransferPacket(uint16_t input_len, uint16_t addr, bool borderLoadingEffect);
extern void transferSnaData(FatFile* pFile, bool borderLoadingEffect = false);
extern void synchronizeForExecution();
extern void executeSnapshot();
extern boolean bootFromSnapshot(FatFile* pFile);
extern void clearScreen(uint8_t col = 0);


extern void syncWithZ80();
extern void triggerNMI();

}

#endif