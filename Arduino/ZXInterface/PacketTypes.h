#ifndef PACKET_TYPES_H
#define PACKET_TYPES_H

#include <stdint.h>
#include "CommandRegistry.h"

#pragma pack(push, 1)

#define ASSERT_Z80_PACKET_SIZE(size,packet_type) \
    static_assert(sizeof(packet_type) == size, "Wrong packet size for Z80 assembly code")

struct NOP_Packet {
    uint8_t cmd_high;
    uint8_t cmd_low;
    
    NOP_Packet() : cmd_high(CommandRegistry::command_NOP >> 8), 
                   cmd_low(CommandRegistry::command_NOP & 0xff) {}
};
ASSERT_Z80_PACKET_SIZE(2,NOP_Packet);

struct ReceiveKeyboardPacket {
    uint8_t cmd_high;
    uint8_t cmd_low;
    
    ReceiveKeyboardPacket() : cmd_high(CommandRegistry::command_TransmitKey >> 8), 
                              cmd_low(CommandRegistry::command_TransmitKey & 0xff) {}
};
ASSERT_Z80_PACKET_SIZE(2,ReceiveKeyboardPacket);

struct ExecutePacket {
    uint8_t cmd_high;
    uint8_t cmd_low;

    ExecutePacket() : cmd_high(CommandRegistry::command_Execute >> 8), 
                      cmd_low(CommandRegistry::command_Execute & 0xff) {}
};
ASSERT_Z80_PACKET_SIZE(2,ExecutePacket);

struct TransferPacket {
    uint8_t cmd_high;
    uint8_t cmd_low;
    uint8_t cmd_amount;
    uint8_t dest_addr_high;
    uint8_t dest_addr_low;
};
ASSERT_Z80_PACKET_SIZE(5,TransferPacket);

struct CopyPacket {
    uint8_t cmd_high;
    uint8_t cmd_low;
    uint8_t amount;
    uint8_t dest_addr_high;
    uint8_t dest_addr_low;
};
ASSERT_Z80_PACKET_SIZE(5,CopyPacket);

struct FillPacket {
    uint8_t cmd_high;
    uint8_t cmd_low;
    uint8_t amount_high;
    uint8_t amount_low;
    uint8_t start_addr_high;
    uint8_t start_addr_low;
    uint8_t fill_value;

    FillPacket(uint16_t amount, uint16_t address, uint8_t value) : 
                cmd_high(CommandRegistry::command_Fill >> 8), 
                cmd_low(CommandRegistry::command_Fill & 0xff),
                amount_high(static_cast<uint8_t>(amount >> 8)),
                amount_low(static_cast<uint8_t>(amount & 0xFF)),
                start_addr_high(static_cast<uint8_t>(address >> 8)),
                start_addr_low(static_cast<uint8_t>(address & 0xFF)),
                fill_value(value) {}
};
ASSERT_Z80_PACKET_SIZE(7,FillPacket);

struct Fill8Packet {
    uint8_t cmd_high;
    uint8_t cmd_low;
    uint8_t addr_high;
    uint8_t addr_low;
    uint8_t amount;
    uint8_t fill_value;

    Fill8Packet() : cmd_high(CommandRegistry::command_fill_mem_bytecount >> 8), 
                    cmd_low(CommandRegistry::command_fill_mem_bytecount & 0xff) {}
};
ASSERT_Z80_PACKET_SIZE(6,Fill8Packet);

struct StackPacket {
    uint8_t cmd_high;
    uint8_t cmd_low;
    uint8_t sp_addr_high;
    uint8_t sp_addr_low;
    uint8_t action;

    StackPacket(uint16_t addr, uint8_t action ) : 
        cmd_high(CommandRegistry::command_Stack >> 8) , cmd_low(CommandRegistry::command_Stack & 0xff) ,
        sp_addr_high(static_cast<uint8_t>(addr >> 8)), sp_addr_low(static_cast<uint8_t>(addr & 0xFF)) ,
        action(action) {}
};
ASSERT_Z80_PACKET_SIZE(5,StackPacket);

// copies a fixed amount of 32 bytes
struct Copy32Packet {
    uint8_t cmd_high;
    uint8_t cmd_low;
    uint8_t dest_addr_high;
    uint8_t dest_addr_low;

    Copy32Packet() : cmd_high(CommandRegistry::command_Copy32 >> 8), 
                     cmd_low(CommandRegistry::command_Copy32 & 0xff) {}
};
ASSERT_Z80_PACKET_SIZE(4,Copy32Packet);

struct WaitVBLPacket {
    uint8_t cmd_high;
    uint8_t cmd_low;
    
    WaitVBLPacket() : cmd_high(CommandRegistry::command_VBL_Wait >> 8), 
                      cmd_low(CommandRegistry::command_VBL_Wait & 0xff) {}
};
ASSERT_Z80_PACKET_SIZE(2,WaitVBLPacket);

struct RequestSendDataPacket {
    uint8_t cmd_high;
    uint8_t cmd_low;
    uint8_t amount_high;
    uint8_t amount_low;
    uint8_t start_addr_high;
    uint8_t start_addr_low;

    RequestSendDataPacket(uint16_t amount, uint16_t address) : 
        cmd_high(CommandRegistry::command_SendData >> 8), cmd_low(CommandRegistry::command_SendData & 0xff), 
        amount_high(static_cast<uint8_t>(amount >> 8)), amount_low(static_cast<uint8_t>(amount & 0xFF)),
        start_addr_high(static_cast<uint8_t>(address >> 8)), start_addr_low(static_cast<uint8_t>(address & 0xFF))
        {}
};
ASSERT_Z80_PACKET_SIZE(6,RequestSendDataPacket);


#pragma pack(pop)


#endif