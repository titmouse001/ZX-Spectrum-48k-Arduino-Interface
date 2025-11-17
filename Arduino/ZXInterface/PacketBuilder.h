#ifndef PACKET_BUILDER_H
#define PACKET_BUILDER_H

#include <stdint.h>

namespace PacketBuilder {

uint8_t buildTransferCommand(uint8_t* buf, uint16_t address, uint8_t length);
uint8_t buildCopyCommand(uint8_t* buf, uint16_t address, uint8_t length);
uint8_t buildSmallFillCommand(uint8_t* buf, uint8_t amount, uint16_t address, uint8_t value);
uint8_t buildFillCommand(uint8_t* buf, uint16_t amount, uint16_t address, uint8_t value);
uint8_t buildStackCommand(uint8_t* buf, uint16_t address,uint8_t action );
uint8_t buildWaitVBLCommand(uint8_t* buf);
uint8_t buildExecuteCommand(uint8_t* buf);

uint8_t build_command_fill_mem_bytecount(uint8_t* buf, uint16_t address,  uint8_t amount, uint8_t value);

}

#endif