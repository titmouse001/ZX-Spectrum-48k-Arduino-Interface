#include <stdint.h>
#include "PacketBuilder.h"
#include "PacketTypes.h"
//#include "CommandRegistry.h"


// Transfers with flashing border during load
uint8_t PacketBuilder::buildTransferCommand(uint8_t* buf, uint16_t address, uint8_t length) {
    TransferPacket* pkt = reinterpret_cast<TransferPacket*>(buf);
    pkt->cmd_high = static_cast<uint8_t>(cmd_addr(CMD_Transfer) >> 8);
    pkt->cmd_low  = static_cast<uint8_t>(cmd_addr(CMD_Transfer) & 0xFF);
    pkt->cmd_amount = length;
    pkt->dest_addr_high = static_cast<uint8_t>(address >> 8);
    pkt->dest_addr_low  = static_cast<uint8_t>(address & 0xFF);
    return sizeof(TransferPacket);
}

// Like buildTransferCommand but without flashing border during load
uint8_t PacketBuilder::buildCopyCommand(uint8_t* buf, uint16_t address, uint8_t length) {
    CopyPacket* pkt = reinterpret_cast<CopyPacket*>(buf);
    pkt->cmd_high = static_cast<uint8_t>(cmd_addr(CMD_Copy) >> 8);
    pkt->cmd_low  = static_cast<uint8_t>(cmd_addr(CMD_Copy) & 0xFF);
    pkt->amount = length;
    pkt->dest_addr_high = static_cast<uint8_t>(address >> 8);
    pkt->dest_addr_low  = static_cast<uint8_t>(address & 0xFF);
    return sizeof(CopyPacket);
}

uint8_t PacketBuilder::build_command_fill8(uint8_t* buf , uint16_t address, uint8_t amount, uint8_t value) {
    // NOTE: The original added amount to address because the Z80 routine fills backwards.
    address += amount;

    Fill8Packet* pkt = reinterpret_cast<Fill8Packet*>(buf);
   // pkt->cmd_high = static_cast<uint8_t>(CommandRegistry::command_fill_mem_bytecount >> 8);
  //  pkt->cmd_low  = static_cast<uint8_t>(CommandRegistry::command_fill_mem_bytecount & 0xFF);
    pkt->addr_high = static_cast<uint8_t>(address >> 8);
    pkt->addr_low  = static_cast<uint8_t>(address & 0xFF);
    pkt->amount = amount;
    pkt->fill_value = value;
    return sizeof(Fill8Packet);
}
