#ifndef PACKET_TYPES_H
#define PACKET_TYPES_H

#include <stdint.h>

// NOTE: The static_asserts are a safe gard to make sure these packet structs have the 
// same size as the Z80 assembly code running on the ZX Inerface.

#pragma pack(push, 1)

struct ReceiveKeyboardPacket {
    uint8_t cmd_high;
    uint8_t cmd_low;
};
static_assert(sizeof(ReceiveKeyboardPacket) == 2, "Wrong packet size");

struct ExecutePacket {
    uint8_t cmd_high;
    uint8_t cmd_low;
};
static_assert(sizeof(ExecutePacket) == 2, "Wrong packet size");

struct TransferPacket {
    uint8_t cmd_high;
    uint8_t cmd_low;
    uint8_t cmd_amount;
    uint8_t dest_addr_high;
    uint8_t dest_addr_low;
};
static_assert(sizeof(TransferPacket) == 5, "Wrong packet size");

struct FillPacket {
    uint8_t cmd_high;
    uint8_t cmd_low;
    uint8_t amount_high;
    uint8_t amount_low;
    uint8_t start_addr_high;
    uint8_t start_addr_low;
    uint8_t fill_value;
};
static_assert(sizeof(FillPacket) == 7, "Wrong packet size");

struct Fill8Packet {
    uint8_t cmd_high;
    uint8_t cmd_low;
    uint8_t addr_high;
    uint8_t addr_low;
    uint8_t amount;
    uint8_t fill_value;
};
static_assert(sizeof(Fill8Packet) == 6, "Wrong packet size");

struct StackPacket {
    uint8_t cmd_high;
    uint8_t cmd_low;
    uint8_t sp_addr_high;
    uint8_t sp_addr_low;
    uint8_t action;
};
static_assert(sizeof(StackPacket) == 5, "Wrong packet size");

struct CopyPacket {
    uint8_t cmd_high;
    uint8_t cmd_low;
    uint8_t amount;
    uint8_t dest_addr_high;
    uint8_t dest_addr_low;
};
static_assert(sizeof(CopyPacket) == 5, "Wrong packet size");

struct Copy32Packet {
    uint8_t cmd_high;
    uint8_t cmd_low;
    uint8_t dest_addr_high;
    uint8_t dest_addr_low;
};
static_assert(sizeof(Copy32Packet) == 4, "Wrong packet size");

struct WaitVBLPacket {
    uint8_t cmd_high;
    uint8_t cmd_low;
};
static_assert(sizeof(WaitVBLPacket) == 2, "Wrong packet size");

struct RequestSendDataPacket {
    uint8_t cmd_high;
    uint8_t cmd_low;
    uint8_t amount_high;
    uint8_t amount_low;
    uint8_t start_addr_high;
    uint8_t start_addr_low;
};
static_assert(sizeof(RequestSendDataPacket) == 6, "Wrong packet size");


// check that all packets fit in a global buffer... NO LONGER USED
//constexpr uint8_t GLOBAL_MAX_PACKET_LEN = 7;  // largest packet is FillPacket (7 bytes)
//static_assert(GLOBAL_MAX_PACKET_LEN == sizeof(FillPacket), "Update GLOBAL_MAX_PACKET_LEN if needed");

#pragma pack(pop)




#endif