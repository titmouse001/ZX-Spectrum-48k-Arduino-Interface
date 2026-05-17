#include <stdint.h>
#include "PacketBuilder.h"
#include "PacketTypes.h"
#include "CommandRegistry.h"

// Transfers with flashing border during load
uint8_t PacketBuilder::buildTransferCommand(uint8_t* buf, uint16_t address, uint8_t length) {
    TransferPacket* pkt = reinterpret_cast<TransferPacket*>(buf);
    pkt->cmd_high = static_cast<uint8_t>(CommandRegistry::command_Transfer >> 8);
    pkt->cmd_low  = static_cast<uint8_t>(CommandRegistry::command_Transfer & 0xFF);
    pkt->cmd_amount = length;
    pkt->dest_addr_high = static_cast<uint8_t>(address >> 8);
    pkt->dest_addr_low  = static_cast<uint8_t>(address & 0xFF);
    return sizeof(TransferPacket);
}

// Like buildTransferCommand but without flashing border during load
uint8_t PacketBuilder::buildCopyCommand(uint8_t* buf, uint16_t address, uint8_t length) {
    CopyPacket* pkt = reinterpret_cast<CopyPacket*>(buf);
    pkt->cmd_high = static_cast<uint8_t>(CommandRegistry::command_Copy >> 8);
    pkt->cmd_low  = static_cast<uint8_t>(CommandRegistry::command_Copy & 0xFF);
    pkt->amount = length;
    pkt->dest_addr_high = static_cast<uint8_t>(address >> 8);
    pkt->dest_addr_low  = static_cast<uint8_t>(address & 0xFF);
    return sizeof(CopyPacket);
}

uint8_t PacketBuilder::build_command_fill_mem_bytecount(uint8_t* buf, uint16_t address, uint8_t amount, uint8_t value) {
    // NOTE: The original added amount to address because the Z80 routine works backwards.
    address += amount;

    Fill8Packet* pkt = reinterpret_cast<Fill8Packet*>(buf);
    pkt->cmd_high = static_cast<uint8_t>(CommandRegistry::command_fill_mem_bytecount >> 8);
    pkt->cmd_low  = static_cast<uint8_t>(CommandRegistry::command_fill_mem_bytecount & 0xFF);
    pkt->addr_high = static_cast<uint8_t>(address >> 8);
    pkt->addr_low  = static_cast<uint8_t>(address & 0xFF);
    pkt->amount = amount;
    pkt->fill_value = value;
    return sizeof(Fill8Packet);
}

// Fill supports a maximum fill amount of 0xFFFF bytes
uint8_t PacketBuilder::buildFillCommand(uint8_t* buf, uint16_t amount, uint16_t address, uint8_t value) {
    FillPacket* pkt = reinterpret_cast<FillPacket*>(buf);
    pkt->cmd_high = static_cast<uint8_t>(CommandRegistry::command_Fill >> 8);
    pkt->cmd_low  = static_cast<uint8_t>(CommandRegistry::command_Fill & 0xFF);
    pkt->amount_high = static_cast<uint8_t>(amount >> 8);
    pkt->amount_low  = static_cast<uint8_t>(amount & 0xFF);
    pkt->start_addr_high = static_cast<uint8_t>(address >> 8);
    pkt->start_addr_low  = static_cast<uint8_t>(address & 0xFF);
    pkt->fill_value = value;
    return sizeof(FillPacket);
}

// Sets the Z80's stack pointer and then halts
uint8_t PacketBuilder::buildStackCommand(uint8_t* buf, uint16_t address, uint8_t action) {
    StackPacket* pkt = reinterpret_cast<StackPacket*>(buf);
    pkt->cmd_high = static_cast<uint8_t>(CommandRegistry::command_Stack >> 8);
    pkt->cmd_low  = static_cast<uint8_t>(CommandRegistry::command_Stack & 0xFF);
    pkt->sp_addr_high = static_cast<uint8_t>(address >> 8);
    pkt->sp_addr_low  = static_cast<uint8_t>(address & 0xFF);
    pkt->action = action;
    return sizeof(StackPacket);
}

// Waits for the Speccy's vertical blanking Line and then halts
uint8_t PacketBuilder::buildWaitVBLCommand(uint8_t* buf) {
    WaitVBLPacket* pkt = reinterpret_cast<WaitVBLPacket*>(buf);
    pkt->cmd_high = static_cast<uint8_t>(CommandRegistry::command_VBL_Wait >> 8);
    pkt->cmd_low  = static_cast<uint8_t>(CommandRegistry::command_VBL_Wait & 0xFF);
    return sizeof(WaitVBLPacket);
}

// Builds an Execute command packet which triggers the Z80 to restore game state
uint8_t PacketBuilder::buildExecuteCommand(uint8_t* buf) {
    ExecutePacket* pkt = reinterpret_cast<ExecutePacket*>(buf);
    pkt->cmd_high = static_cast<uint8_t>(CommandRegistry::command_Execute >> 8);
    pkt->cmd_low  = static_cast<uint8_t>(CommandRegistry::command_Execute & 0xFF);
    return sizeof(ExecutePacket);
}

uint8_t PacketBuilder::build_Request_CommandSendData(uint8_t* buf, uint16_t amount, uint16_t address) {
    RequestSendDataPacket* pkt = reinterpret_cast<RequestSendDataPacket*>(buf);
    pkt->cmd_high = static_cast<uint8_t>(CommandRegistry::command_SendData >> 8);
    pkt->cmd_low  = static_cast<uint8_t>(CommandRegistry::command_SendData & 0xFF);
    pkt->amount_high = static_cast<uint8_t>(amount >> 8);
    pkt->amount_low  = static_cast<uint8_t>(amount & 0xFF);
    pkt->start_addr_high = static_cast<uint8_t>(address >> 8);
    pkt->start_addr_low  = static_cast<uint8_t>(address & 0xFF);
    return sizeof(RequestSendDataPacket);
}

