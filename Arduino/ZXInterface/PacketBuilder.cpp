#include <stdint.h>
#include "PacketBuilder.h"
#include "PacketTypes.h"
#include "CommandRegistry.h"

// // Transfers with flashing border during load
// uint8_t PacketBuilder::buildTransferCommand(uint8_t* buf, uint16_t address, uint8_t length) {
//   buf[E(TransferPacket::CMD_HIGH)] = (uint8_t)(CommandRegistry::command_Transfer >> 8);
//   buf[E(TransferPacket::CMD_LOW)] = (uint8_t)(CommandRegistry::command_Transfer & 0xFF);
//   buf[E(TransferPacket::CMD_AMOUNT)] = length;
//   buf[E(TransferPacket::CMD_DEST_ADDR_HIGH)] = (uint8_t)(address >> 8);
//   buf[E(TransferPacket::CMD_DEST_ADDR_LOW)] = (uint8_t)(address & 0xFF);
//   return E(TransferPacket::PACKET_LEN);
// }

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

// // Like buildTransferCommand but without flashing border during load
// uint8_t PacketBuilder::buildCopyCommand(uint8_t* buf, uint16_t address, uint8_t length) {
//   buf[E(CopyPacket::CMD_HIGH)] = (uint8_t)((CommandRegistry::command_Copy) >> 8);
//   buf[E(CopyPacket::CMD_LOW)] = (uint8_t)((CommandRegistry::command_Copy)&0xFF);
//   buf[E(CopyPacket::CMD_AMOUNT)] = length;
//   buf[E(CopyPacket::CMD_DEST_ADDR_HIGH)] = (uint8_t)((address) >> 8);
//   buf[E(CopyPacket::CMD_DEST_ADDR_LOW)] = (uint8_t)((address)&0xFF);
//   return E(CopyPacket::PACKET_LEN);
// }

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


// uint8_t PacketBuilder::build_command_fill_mem_bytecount(uint8_t* buf, uint16_t address, uint8_t amount, uint8_t value) {

//   address+=amount;  // adding amount as fill is optimised to work backwards

//   buf[E(Fill8Packet::CMD_HIGH)] = (uint8_t)((CommandRegistry::command_fill_mem_bytecount) >> 8);
//   buf[E(Fill8Packet::CMD_LOW)]  = (uint8_t)((CommandRegistry::command_fill_mem_bytecount)&0xFF);
//   buf[E(Fill8Packet::CMD_ADDR_HIGH)] = (uint8_t)((address) >> 8);
//   buf[E(Fill8Packet::CMD_ADDR_LOW)] = (uint8_t)((address)&0xFF);
//   buf[E(Fill8Packet::CMD_AMOUNT)] = amount;    // 1 to 255, only byte size count supported
//   buf[E(Fill8Packet::CMD_FILL_VALUE)] = value; // byte size value
//   return E(Fill8Packet::PACKET_LEN);
// }


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


// // Fill supports a maximum fill amount of 0xFFFF bytes
// uint8_t PacketBuilder::buildFillCommand(uint8_t* buf, uint16_t amount, uint16_t address, uint8_t value) {
//   buf[E(FillPacket::CMD_HIGH)] = (uint8_t)((CommandRegistry::command_Fill) >> 8);
//   buf[E(FillPacket::CMD_LOW)] = (uint8_t)((CommandRegistry::command_Fill)&0xFF);
//   buf[E(FillPacket::CMD_AMOUNT_HIGH)] = (uint8_t)((amount) >> 8);
//   buf[E(FillPacket::CMD_AMOUNT_LOW)] = (uint8_t)((amount)&0xFF);
//   buf[E(FillPacket::CMD_START_ADDR_HIGH)] = (uint8_t)((address) >> 8);
//   buf[E(FillPacket::CMD_START_ADDR_LOW)] = (uint8_t)((address)&0xFF);
//   buf[E(FillPacket::CMD_FILL_VALUE)] = value;
//   return E(FillPacket::PACKET_LEN);
// }

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

// // Sets the Z80's stack pointer and then halts
// uint8_t PacketBuilder::buildStackCommand(uint8_t* buf, uint16_t address,uint8_t action) {
//   buf[E(StackPacket::CMD_HIGH)] = (uint8_t)(CommandRegistry::command_Stack >> 8);
//   buf[E(StackPacket::CMD_LOW)] = (uint8_t)(CommandRegistry::command_Stack & 0xFF);
//   buf[E(StackPacket::CMD_SP_ADDR_HIGH)] = (uint8_t)(address >> 8);
//   buf[E(StackPacket::CMD_SP_ADDR_LOW)] = (uint8_t)(address & 0xFF);
//   buf[E(StackPacket::CMD_ACTION)] = action;
//   return E(StackPacket::PACKET_LEN);
// }


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


// // Waits for the Speccy's vertical blanking Line and then halts
// uint8_t PacketBuilder::buildWaitVBLCommand(uint8_t* buf) {
//   /*
//    * Z80 call looks like this:
//    *    IM 1    ; MASKABLE INTERRUPT (IM 1) - Vector: 0x0038  
//    *    EI 
//    *    HALT 		; Signal Z80 halt line for Arduino to read
//    */
//   buf[E(WaitVBLPacket::CMD_HIGH)] = (uint8_t)((CommandRegistry::command_VBL_Wait) >> 8);
//   buf[E(WaitVBLPacket::CMD_LOW)] = (uint8_t)((CommandRegistry::command_VBL_Wait)&0xFF);
//   return E(WaitVBLPacket::PACKET_LEN);
// }


// Waits for the Speccy's vertical blanking Line and then halts
uint8_t PacketBuilder::buildWaitVBLCommand(uint8_t* buf) {
    WaitVBLPacket* pkt = reinterpret_cast<WaitVBLPacket*>(buf);
    pkt->cmd_high = static_cast<uint8_t>(CommandRegistry::command_VBL_Wait >> 8);
    pkt->cmd_low  = static_cast<uint8_t>(CommandRegistry::command_VBL_Wait & 0xFF);
    return sizeof(WaitVBLPacket);
}


// // Builds an Execute command packet which triggers the Z80 to restore all CPU state
// // (registers, stack pointer, interrupt flags, border color) from a saved snapshot
// // and then jump to the relocated start address in screen memory to resume execution
// uint8_t PacketBuilder::buildExecuteCommand(uint8_t* buf) {
//   buf[E(ExecutePacket::CMD_HIGH)] = (uint8_t)((CommandRegistry::command_Execute) >> 8);
//   buf[E(ExecutePacket::CMD_LOW)] = (uint8_t)((CommandRegistry::command_Execute)&0xFF);
//   return E(ExecutePacket::PACKET_LEN);
// }


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

// uint8_t PacketBuilder::build_Request_CommandSendData(uint8_t* buf, uint16_t amount, uint16_t address) {
//   // After this command call get_IO_Byte() for each byte requested.
//   buf[E(RequestSendDataPacket::CMD_HIGH)] = (uint8_t)((CommandRegistry::command_SendData) >> 8);
//   buf[E(RequestSendDataPacket::CMD_LOW)] = (uint8_t)((CommandRegistry::command_SendData)&0xFF);
//   buf[E(RequestSendDataPacket::CMD_AMOUNT_HIGH)] = (uint8_t)((amount) >> 8);
//   buf[E(RequestSendDataPacket::CMD_AMOUNT_LOW)] = (uint8_t)((amount)&0xFF);
//   buf[E(RequestSendDataPacket::CMD_START_ADDR_HIGH)] = (uint8_t)((address) >> 8);
//   buf[E(RequestSendDataPacket::CMD_START_ADDR_LOW)] = (uint8_t)((address)&0xFF);
//   return E(RequestSendDataPacket::PACKET_LEN);
// }
