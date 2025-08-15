#ifndef PACKET_TYPES_H
#define PACKET_TYPES_H

#include <stdint.h>

#define E(e) static_cast<uint8_t>(e)
constexpr uint8_t GLOBAL_MAX_PACKET_LEN = 7;

enum class TransmitKeyPacket : uint8_t {
  CMD_HIGH = 0,
  CMD_LOW = 1,
  CMD_DELAY = 2,
  PACKET_LEN
};

enum class ExecutePacket : uint8_t {
  CMD_HIGH = 0,
  CMD_LOW = 1,
  PACKET_LEN
};

enum class TransferPacket : uint8_t {
  CMD_HIGH = 0,
  CMD_LOW = 1,
  CMD_LEN = 2,
  CMD_DEST_ADDR_HIGH = 3,
  CMD_DEST_ADDR_LOW = 4,
  PACKET_LEN
};

enum class FillPacket : uint8_t {
  CMD_HIGH = 0,
  CMD_LOW = 1,
  CMD_AMOUNT_HIGH = 2,
  CMD_AMOUNT_LOW = 3,
  CMD_START_ADDR_HIGH = 4,
  CMD_START_ADDR_LOW = 5,
  CMD_FILL_VALUE = 6,
  PACKET_LEN
};

enum class SmallFillPacket : uint8_t {
  CMD_HIGH = 0,
  CMD_LOW = 1,
  CMD_AMOUNT = 2,
  CMD_START_ADDR_HIGH = 3,
  CMD_START_ADDR_LOW = 4,
  CMD_FILL_VALUE = 5,
  PACKET_LEN
};


enum class SmallFillVariableEvenPacket : uint8_t {
  CMD_HIGH = 0,
  CMD_LOW = 1,
  CMD_ADDR_HIGH = 2,
  CMD_ADDR_LOW = 3,
  CMD_AMOUNT = 4,
  CMD_FILL_VALUE = 5,
  PACKET_LEN
};
enum class SmallFillVariableOddPacket : uint8_t {
  CMD_HIGH = 0,
  CMD_LOW = 1,
  CMD_ADDR_HIGH = 2,
  CMD_ADDR_LOW = 3,
  CMD_AMOUNT = 4,
  CMD_FILL_VALUE = 5,
  PACKET_LEN
};



enum class StackPacket : uint8_t {
  CMD_HIGH = 0,
  CMD_LOW = 1,
  CMD_SP_ADDR_HIGH = 2,
  CMD_SP_ADDR_LOW = 3,
  PACKET_LEN
};

enum class CopyPacket : uint8_t {
  CMD_HIGH = 0,
  CMD_LOW = 1,
  CMD_LEN = 2,
  CMD_DEST_ADDR_HIGH = 3,
  CMD_DEST_ADDR_LOW = 4,
  PACKET_LEN
};

enum class Copy32Packet : uint8_t {
  CMD_HIGH = 0,
  CMD_LOW = 1,
  CMD_DEST_ADDR_HIGH = 2,
  CMD_DEST_ADDR_LOW = 3,
  PACKET_LEN
};

enum class WaitVBLPacket : uint8_t {
  CMD_HIGH = 0,
  CMD_LOW = 1,
  PACKET_LEN
};

#endif