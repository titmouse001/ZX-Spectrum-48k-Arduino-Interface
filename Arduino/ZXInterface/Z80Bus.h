#ifndef Z80BUS_H
#define Z80BUS_H

#include <stdint.h>
#include "pin.h"
#include "SdFat.h"

namespace Z80Bus {

void setupPins();

extern void sendBytes(uint8_t* data, uint16_t size);
extern void sendSnaHeader(uint8_t* header);
extern void sendFillCommand(uint16_t address, uint16_t amount, uint8_t color);
extern void sendWaitVBLCommand();
extern void sendStackCommand(uint16_t addr, uint8_t action);
extern void highlightSelection(uint16_t currentFileIndex, uint16_t startFileIndex, uint16_t& oldHighlightAddress);

extern uint8_t getKeyboard();
extern uint8_t get_IO_Byte();

extern void rleOptimisedTransfer(uint16_t input_len, uint16_t addr, bool borderLoadingEffect);
extern void transferSnaData(FatFile* pFile, bool borderLoadingEffect = false);
extern void executeSnapshot();
extern boolean bootFromSnapshot(FatFile* pFile);

extern void resetZ80();
extern void resetToSnaRom();
extern void waitHalt_syncWithZ80();
extern void triggerZ80NMI();
extern void waitForZ80Resume();
extern void waitRelease_NMI();

}

#endif