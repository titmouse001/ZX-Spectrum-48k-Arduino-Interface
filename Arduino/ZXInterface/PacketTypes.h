#ifndef PACKET_TYPES_H
#define PACKET_TYPES_H

#include <stdint.h>
//#include "CommandRegistry.h"


// NOTE:  Z80 has Jump Table place at ORG $00D0
constexpr uint16_t CMD_BASE = 0x00D0;
// 3xn, each part of the jump table is 3 bytes e.g. 00D0:C34E01	-> JP 014E
constexpr uint16_t cmd_addr(uint8_t n) { return CMD_BASE + (3 * n); }

constexpr uint16_t CMD_NOP=0;
constexpr uint16_t CMD_TransmitKey=1;
constexpr uint16_t CMD_Fill=2;
constexpr uint16_t CMD_Transfer=3;
constexpr uint16_t CMD_Copy=4;
constexpr uint16_t CMD_Copy32=5;
constexpr uint16_t CMD_VBL_Wait=6;
constexpr uint16_t CMD_Stack=7;
constexpr uint16_t CMD_Execute=8;
constexpr uint16_t CMD_Fill8=9;
constexpr uint16_t CMD_SendData=10;

#pragma pack(push, 1)

#define ASSERT_Z80_PACKET_SIZE(size,packet_type) \
    static_assert(sizeof(packet_type) == size, "Wrong packet size for Z80 assembly code")

struct NOP_Packet {
    uint8_t cmd_high;
    uint8_t cmd_low;
    
    NOP_Packet() : cmd_high(cmd_addr(CMD_NOP) >> 8), 
                   cmd_low(cmd_addr(CMD_NOP) & 0xff) {}
};
ASSERT_Z80_PACKET_SIZE(2,NOP_Packet);

struct ReceiveKeyboardPacket {
    uint8_t cmd_high;
    uint8_t cmd_low;
    
    ReceiveKeyboardPacket() : cmd_high(cmd_addr(CMD_TransmitKey) >> 8), 
                              cmd_low(cmd_addr(CMD_TransmitKey) & 0xff) {}
};
ASSERT_Z80_PACKET_SIZE(2,ReceiveKeyboardPacket);

struct ExecutePacket {
    uint8_t cmd_high;
    uint8_t cmd_low;

    ExecutePacket() : cmd_high(cmd_addr(CMD_Execute) >> 8), 
                      cmd_low(cmd_addr(CMD_Execute) & 0xff) {}
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
                cmd_high(cmd_addr(CMD_Fill) >> 8), 
                cmd_low(cmd_addr(CMD_Fill) & 0xff),
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

    Fill8Packet() : cmd_high(cmd_addr(CMD_Fill8) >> 8), 
                    cmd_low(cmd_addr(CMD_Fill8) & 0xff) {}
};
ASSERT_Z80_PACKET_SIZE(6,Fill8Packet);

struct StackPacket {
    uint8_t cmd_high;
    uint8_t cmd_low;
    uint8_t sp_addr_high;
    uint8_t sp_addr_low;

    StackPacket(uint16_t addr ) : 
        cmd_high(cmd_addr(CMD_Stack) >> 8), 
        cmd_low(cmd_addr(CMD_Stack) & 0xff) ,
        sp_addr_high(static_cast<uint8_t>(addr >> 8)), sp_addr_low(static_cast<uint8_t>(addr & 0xFF))  {}
};
ASSERT_Z80_PACKET_SIZE(4,StackPacket);

// copies a fixed amount of 32 bytes
struct Copy32Packet {
    uint8_t cmd_high;
    uint8_t cmd_low;
    uint8_t dest_addr_high;
    uint8_t dest_addr_low;

    Copy32Packet() : cmd_high(cmd_addr(CMD_Copy32) >> 8), 
                     cmd_low( cmd_addr(CMD_Copy32) & 0xff) {}
};
ASSERT_Z80_PACKET_SIZE(4,Copy32Packet);

struct WaitVBLPacket {
    uint8_t cmd_high;
    uint8_t cmd_low;
    
    WaitVBLPacket() : cmd_high(cmd_addr(CMD_VBL_Wait) >> 8), 
                      cmd_low(cmd_addr(CMD_VBL_Wait) & 0xff) {}
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
        cmd_high(cmd_addr(CMD_SendData) >> 8), 
        cmd_low(cmd_addr(CMD_SendData) & 0xff), 
        amount_high(static_cast<uint8_t>(amount >> 8)), amount_low(static_cast<uint8_t>(amount & 0xFF)),
        start_addr_high(static_cast<uint8_t>(address >> 8)), start_addr_low(static_cast<uint8_t>(address & 0xFF))
        {}
};
ASSERT_Z80_PACKET_SIZE(6,RequestSendDataPacket);


#pragma pack(pop)


#endif