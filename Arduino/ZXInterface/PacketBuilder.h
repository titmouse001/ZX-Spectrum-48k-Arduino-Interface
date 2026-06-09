#ifndef PACKET_BUILDER_H
#define PACKET_BUILDER_H

#include <stdint.h>

namespace PacketBuilder {
  uint8_t buildTransferCommand(uint8_t* buf, uint16_t address, uint8_t length);
  uint8_t buildCopyCommand(uint8_t* buf, uint16_t address, uint8_t length);
  uint8_t build_command_fill8(uint8_t* buf, uint16_t address,  uint8_t amount, uint8_t value);
}

#endif