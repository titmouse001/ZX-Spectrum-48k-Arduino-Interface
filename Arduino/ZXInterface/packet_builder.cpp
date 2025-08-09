#include "packet_builder.h"
#include "command_registry.h"

namespace PacketBuilder {

    uint8_t buildTransferCommand(uint8_t* buf, uint16_t address, uint8_t length) {
    buf[E(TransferPacket::CMD_HIGH)] = (uint8_t)(CommandRegistry::command_Transfer >> 8);
    buf[E(TransferPacket::CMD_LOW)] = (uint8_t)(CommandRegistry::command_Transfer & 0xFF);
    buf[E(TransferPacket::CMD_LEN)] = length;
    buf[E(TransferPacket::CMD_DEST_ADDR_HIGH)] = (uint8_t)(address >> 8);
    buf[E(TransferPacket::CMD_DEST_ADDR_LOW)] = (uint8_t)(address & 0xFF);
    return E(TransferPacket::PACKET_LEN);
    }

    uint8_t buildSmallFillCommand(uint8_t* buf, uint8_t amount, uint16_t address, uint8_t value) {
    buf[E(SmallFillPacket::CMD_HIGH)] = (uint8_t)((CommandRegistry::command_SmallFill) >> 8);
    buf[E(SmallFillPacket::CMD_LOW)] = (uint8_t)((CommandRegistry::command_SmallFill)&0xFF);
    buf[E(SmallFillPacket::CMD_AMOUNT)] = amount;
    buf[E(SmallFillPacket::CMD_START_ADDR_HIGH)] = (uint8_t)((address) >> 8);
    buf[E(SmallFillPacket::CMD_START_ADDR_LOW)] = (uint8_t)((address)&0xFF);
    buf[E(SmallFillPacket::CMD_FILL_VALUE)] = value;
    return E(SmallFillPacket::PACKET_LEN);
    }

    uint8_t buildFillCommand(uint8_t* buf, uint16_t amount, uint16_t address, uint8_t value) {
    buf[E(FillPacket::CMD_HIGH)] = (uint8_t)((CommandRegistry::command_Fill) >> 8);
    buf[E(FillPacket::CMD_LOW)] = (uint8_t)((CommandRegistry::command_Fill)&0xFF);
    buf[E(FillPacket::CMD_AMOUNT_HIGH)] = (uint8_t)((amount) >> 8);
    buf[E(FillPacket::CMD_AMOUNT_LOW)] = (uint8_t)((amount)&0xFF);
    buf[E(FillPacket::CMD_START_ADDR_HIGH)] = (uint8_t)((address) >> 8);
    buf[E(FillPacket::CMD_START_ADDR_LOW)] = (uint8_t)((address)&0xFF);
    buf[E(FillPacket::CMD_FILL_VALUE)] = value;
    return E(FillPacket::PACKET_LEN);
    }

    uint8_t buildStackCommand(uint8_t* buf, uint16_t address) {
    buf[E(StackPacket::CMD_HIGH)] = (uint8_t)(CommandRegistry::command_Stack >> 8);
    buf[E(StackPacket::CMD_LOW)] = (uint8_t)(CommandRegistry::command_Stack & 0xFF);
    buf[E(StackPacket::CMD_SP_ADDR_HIGH)] = (uint8_t)(address >> 8);
    buf[E(StackPacket::CMD_SP_ADDR_LOW)] = (uint8_t)(address & 0xFF);
    return E(StackPacket::PACKET_LEN);
    }

    uint8_t buildCopyCommand(uint8_t* buf, uint16_t address, uint8_t length) {
    buf[E(CopyPacket::CMD_HIGH)] = (uint8_t)((CommandRegistry::command_Copy) >> 8);
    buf[E(CopyPacket::CMD_LOW)] = (uint8_t)((CommandRegistry::command_Copy)&0xFF);
    buf[E(CopyPacket::CMD_LEN)] = length;
    buf[E(CopyPacket::CMD_DEST_ADDR_HIGH)] = (uint8_t)((address) >> 8);
    buf[E(CopyPacket::CMD_DEST_ADDR_LOW)] = (uint8_t)((address)&0xFF);
    return E(CopyPacket::PACKET_LEN);
    }

    uint8_t buildWaitCommand(uint8_t* buf) {
    buf[E(WaitPacket::CMD_HIGH)] = (uint8_t)((CommandRegistry::command_Wait) >> 8);
    buf[E(WaitPacket::CMD_LOW)] = (uint8_t)((CommandRegistry::command_Wait)&0xFF);
    return E(WaitPacket::PACKET_LEN);
    }

    uint8_t buildExecuteCommand(uint8_t* buf) {
    buf[E(ExecutePacket::CMD_HIGH)] = (uint8_t)((CommandRegistry::command_Execute) >> 8);
    buf[E(ExecutePacket::CMD_LOW)] = (uint8_t)((CommandRegistry::command_Execute)&0xFF);
    return E(ExecutePacket::PACKET_LEN);
    }

}