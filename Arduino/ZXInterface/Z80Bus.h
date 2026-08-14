#ifndef Z80BUS_H
#define Z80BUS_H

#include <stdint.h>
#include "src/fatlib/SdFat.h"
#include "digitalWriteFast.h"
#include "pin.h"
#include "utils.h"

namespace Z80Bus {

void setupPins();

//extern void sendBytes(uint8_t* data, uint16_t size);
extern void sendBytes8(uint8_t* data, uint8_t size);
extern void sendByte(uint8_t* byte);

extern void sendSnaHeader(Z80Registers* regs);
extern void sendFillCommand(uint16_t address, uint16_t amount, uint8_t color);
extern void sendWaitVBLCommand();
extern void setStackCommand(uint16_t addr);

extern uint8_t getKeyboard();
extern uint8_t get_IO_Byte();

extern void rleOptimisedTransfer(uint8_t input_len, uint16_t addr, uint8_t* buf, uint8_t cmd);
extern void transferSnaData(FatFile* pFile, bool borderLoadingEffect = false, uint8_t skipIntialBytes=0);
extern void executeSnapshot(Z80Registers* snaHeaderPacket);

//extern void resetZ80();
extern void setSnaRom();
extern void setStockRom();

extern void waitHalt_syncWithZ80();
extern void triggerZ80NMI();
extern void hasZ80Resumed();

extern void Z80_NOP();


}

#endif