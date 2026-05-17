#ifndef PACKET_TYPES_H
#define PACKET_TYPES_H

#include <stdint.h>

#pragma pack(push, 1)

#define ASSERT_Z80_PACKET_SIZE(size,packet_type) \
    static_assert(sizeof(packet_type) == size, "Wrong packet size for Z80 assembly code")

struct ReceiveKeyboardPacket {
    uint8_t cmd_high;
    uint8_t cmd_low;
};
ASSERT_Z80_PACKET_SIZE(2,ReceiveKeyboardPacket);

struct ExecutePacket {
    uint8_t cmd_high;
    uint8_t cmd_low;
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

struct FillPacket {
    uint8_t cmd_high;
    uint8_t cmd_low;
    uint8_t amount_high;
    uint8_t amount_low;
    uint8_t start_addr_high;
    uint8_t start_addr_low;
    uint8_t fill_value;
};
ASSERT_Z80_PACKET_SIZE(7,FillPacket);

struct Fill8Packet {
    uint8_t cmd_high;
    uint8_t cmd_low;
    uint8_t addr_high;
    uint8_t addr_low;
    uint8_t amount;
    uint8_t fill_value;
};
ASSERT_Z80_PACKET_SIZE(6,Fill8Packet);

struct StackPacket {
    uint8_t cmd_high;
    uint8_t cmd_low;
    uint8_t sp_addr_high;
    uint8_t sp_addr_low;
    uint8_t action;
};
ASSERT_Z80_PACKET_SIZE(5,StackPacket);

struct CopyPacket {
    uint8_t cmd_high;
    uint8_t cmd_low;
    uint8_t amount;
    uint8_t dest_addr_high;
    uint8_t dest_addr_low;
};
ASSERT_Z80_PACKET_SIZE(5,CopyPacket);

struct Copy32Packet {
    uint8_t cmd_high;
    uint8_t cmd_low;
    uint8_t dest_addr_high;
    uint8_t dest_addr_low;
};
ASSERT_Z80_PACKET_SIZE(4,Copy32Packet);

struct WaitVBLPacket {
    uint8_t cmd_high;
    uint8_t cmd_low;
};
ASSERT_Z80_PACKET_SIZE(2,WaitVBLPacket);

struct RequestSendDataPacket {
    uint8_t cmd_high;
    uint8_t cmd_low;
    uint8_t amount_high;
    uint8_t amount_low;
    uint8_t start_addr_high;
    uint8_t start_addr_low;
};
ASSERT_Z80_PACKET_SIZE(6,RequestSendDataPacket);

#pragma pack(pop)


#endif