#include <stdint.h>
#include "PacketBuilder.h"
#include "PacketTypes.h"
#include "CommandRegistry.h"

// Transfers with flashing border during load
uint8_t PacketBuilder::buildTransferCommand(uint8_t* buf, uint16_t address, uint8_t length) {
  buf[E(TransferPacket::CMD_HIGH)] = (uint8_t)(CommandRegistry::command_Transfer >> 8);
  buf[E(TransferPacket::CMD_LOW)] = (uint8_t)(CommandRegistry::command_Transfer & 0xFF);
  buf[E(TransferPacket::CMD_LEN)] = length;
  buf[E(TransferPacket::CMD_DEST_ADDR_HIGH)] = (uint8_t)(address >> 8);
  buf[E(TransferPacket::CMD_DEST_ADDR_LOW)] = (uint8_t)(address & 0xFF);
  return E(TransferPacket::PACKET_LEN);
}

// Like buildTransferCommand but without flashing border during load
uint8_t PacketBuilder::buildCopyCommand(uint8_t* buf, uint16_t address, uint8_t length) {
  buf[E(CopyPacket::CMD_HIGH)] = (uint8_t)((CommandRegistry::command_Copy) >> 8);
  buf[E(CopyPacket::CMD_LOW)] = (uint8_t)((CommandRegistry::command_Copy)&0xFF);
  buf[E(CopyPacket::CMD_LEN)] = length;
  buf[E(CopyPacket::CMD_DEST_ADDR_HIGH)] = (uint8_t)((address) >> 8);
  buf[E(CopyPacket::CMD_DEST_ADDR_LOW)] = (uint8_t)((address)&0xFF);
  return E(CopyPacket::PACKET_LEN);
}

// SmallFill supports a maximum fill amount of 256 bytes (0 wraps to 256)
uint8_t PacketBuilder::buildSmallFillCommand(uint8_t* buf, uint8_t amount, uint16_t address, uint8_t value) {
  buf[E(SmallFillPacket::CMD_HIGH)] = (uint8_t)((CommandRegistry::command_SmallFill) >> 8);
  buf[E(SmallFillPacket::CMD_LOW)] = (uint8_t)((CommandRegistry::command_SmallFill)&0xFF);
  buf[E(SmallFillPacket::CMD_AMOUNT)] = amount;
  buf[E(SmallFillPacket::CMD_START_ADDR_HIGH)] = (uint8_t)((address) >> 8);
  buf[E(SmallFillPacket::CMD_START_ADDR_LOW)]  = (uint8_t)((address)&0xFF);
  buf[E(SmallFillPacket::CMD_FILL_VALUE)] = value;
  return E(SmallFillPacket::PACKET_LEN);
}

uint8_t PacketBuilder::build_command_fill_mem_bytecount(uint8_t* buf, uint16_t address, uint8_t amount, uint8_t value) {
  buf[E(SmallFillVariableOddPacket::CMD_HIGH)] = (uint8_t)((CommandRegistry::command_fill_mem_bytecount) >> 8);
  buf[E(SmallFillVariableOddPacket::CMD_LOW)]  = (uint8_t)((CommandRegistry::command_fill_mem_bytecount)&0xFF);
  buf[E(SmallFillVariableOddPacket::CMD_ADDR_HIGH)] = (uint8_t)((address) >> 8);
  buf[E(SmallFillVariableOddPacket::CMD_ADDR_LOW)] = (uint8_t)((address)&0xFF);
  buf[E(SmallFillVariableOddPacket::CMD_AMOUNT)] = amount;    // 1 to 255, only byte size count supported
  buf[E(SmallFillVariableOddPacket::CMD_FILL_VALUE)] = value; // byte size value
  return E(SmallFillVariableOddPacket::PACKET_LEN);
}

// Fill supports a maximum fill amount of 0xFFFF bytes
uint8_t PacketBuilder::buildFillCommand(uint8_t* buf, uint16_t amount, uint16_t address, uint8_t value) {
  buf[E(FillPacket::CMD_HIGH)] = (uint8_t)((CommandRegistry::command_Fill) >> 8);
  buf[E(FillPacket::CMD_LOW)] = (uint8_t)((CommandRegistry::command_Fill)&0xFF);
  buf[E(FillPacket::CMD_AMOUNT_HIGH)] = (uint8_t)((amount) >> 8);
  buf[E(FillPacket::CMD_AMOUNT_LOW)] = (uint8_t)((amount)&0xFF);
  buf[E(FillPacket::CMD_START_ADDR_HIGH)] = (uint8_t)((address) >> 8);
  buf[E(FillPacket::CMD_START_ADDR_LOW)] = (uint8_t)((address)&0xFF);
  buf[E(FillPacket::CMD_FILL_VALUE)] = value;
  return E(FillPacket::PACKET_LEN);
}

// Sets the Z80's stack pointer and then halts
uint8_t PacketBuilder::buildStackCommand(uint8_t* buf, uint16_t address,uint8_t action) {
  buf[E(StackPacket::CMD_HIGH)] = (uint8_t)(CommandRegistry::command_Stack >> 8);
  buf[E(StackPacket::CMD_LOW)] = (uint8_t)(CommandRegistry::command_Stack & 0xFF);
  buf[E(StackPacket::CMD_SP_ADDR_HIGH)] = (uint8_t)(address >> 8);
  buf[E(StackPacket::CMD_SP_ADDR_LOW)] = (uint8_t)(address & 0xFF);
  buf[E(StackPacket::CMD_ACTION)] = action;
  return E(StackPacket::PACKET_LEN);
}

// Waits for the Speccy's vertical blanking Line (also called vertical blanking interval) and then halts
uint8_t PacketBuilder::buildWaitVBLCommand(uint8_t* buf) {
  buf[E(WaitVBLPacket::CMD_HIGH)] = (uint8_t)((CommandRegistry::command_VBL_Wait) >> 8);
  buf[E(WaitVBLPacket::CMD_LOW)] = (uint8_t)((CommandRegistry::command_VBL_Wait)&0xFF);
  return E(WaitVBLPacket::PACKET_LEN);
}

// Builds an Execute command packet which triggers the Z80 to restore all CPU state
// (registers, stack pointer, interrupt flags, border color) from a saved snapshot
// and then jump to the relocated start address in screen memory to resume execution
uint8_t PacketBuilder::buildExecuteCommand(uint8_t* buf) {
  buf[E(ExecutePacket::CMD_HIGH)] = (uint8_t)((CommandRegistry::command_Execute) >> 8);
  buf[E(ExecutePacket::CMD_LOW)] = (uint8_t)((CommandRegistry::command_Execute)&0xFF);
  return E(ExecutePacket::PACKET_LEN);
}

