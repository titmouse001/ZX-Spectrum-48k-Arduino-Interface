#ifndef BUFFERS_H
#define BUFFERS_H

#include "constants.h"

#define E(e) static_cast<uint8_t>(e)

enum class TransmitKeyPacket : uint8_t {
  CMD_HIGH  = 0,
  CMD_LOW   = 1,
  CMD_DELAY = 2,
  PACKET_LEN 
};

enum class ExecutePacket : uint8_t{
  CMD_HIGH  = 0,
  CMD_LOW   = 1,
  PACKET_LEN 
};

enum class TransferPacket : uint8_t {
  CMD_HIGH  = 0,
  CMD_LOW   = 1,
  CMD_LEN   = 2,
  CMD_DEST_ADDR_HIGH   = 3,
  CMD_DEST_ADDR_LOW  = 4,
  PACKET_LEN 
};

enum class FillPacket : uint8_t {
  CMD_HIGH  = 0,
  CMD_LOW   = 1,
  CMD_AMOUNT_HIGH = 2,
  CMD_AMOUNT_LOW  = 3,
  CMD_START_ADDR_HIGH = 4,
  CMD_START_ADDR_LOW  = 5,
  CMD_FILL_VALUE  = 6,
  PACKET_LEN 
};

enum class SmallFillPacket : uint8_t {
  CMD_HIGH  = 0,
  CMD_LOW   = 1,
  CMD_AMOUNT   = 2,
  CMD_START_ADDR_HIGH = 3,
  CMD_START_ADDR_LOW  = 4,
  CMD_FILL_VALUE  = 5,
  PACKET_LEN 
};

enum class StackPacket : uint8_t {
  CMD_HIGH  = 0,
  CMD_LOW   = 1,
  CMD_SP_ADDR_HIGH = 2,
  CMD_SP_ADDR_LOW  = 3,
  PACKET_LEN 
};


enum class CopyPacket : uint8_t {
  CMD_HIGH  = 0,
  CMD_LOW   = 1,
  CMD_LEN   = 2,
  CMD_DEST_ADDR_HIGH   = 3,
  CMD_DEST_ADDR_LOW  = 4,
  PACKET_LEN 
};


enum class WaitPacket : uint8_t{
  CMD_HIGH  = 0,
  CMD_LOW   = 1,
  PACKET_LEN 
};


// --- Buffer Segment Definitions ---
constexpr uint16_t COMMAND_PAYLOAD_SECTION_SIZE = 255; 
constexpr uint16_t FILE_READ_BUFFER_SIZE = 128; 

constexpr uint16_t FILE_READ_BUFFER_OFFSET = 5 + COMMAND_PAYLOAD_SECTION_SIZE;
constexpr uint16_t TOTAL_PACKET_BUFFER_SIZE = 5 + COMMAND_PAYLOAD_SECTION_SIZE + FILE_READ_BUFFER_SIZE;

uint8_t packetBuffer[TOTAL_PACKET_BUFFER_SIZE];
uint8_t head27_Execute[SNA_TOTAL_ITEMS + static_cast<uint8_t>(ExecutePacket::PACKET_LEN) ]; 
byte TextBuffer[SmallFont::FNT_BUFFER_SIZE * SmallFont::FNT_HEIGHT] = { 0 }; // SmallFont 5x7

namespace Buffers {

uint16_t command_TransmitKey;
uint16_t command_Fill;
uint16_t command_SmallFill;
uint16_t command_Transfer;
uint16_t command_Copy;
uint16_t command_Copy32;
uint16_t command_Wait;
uint16_t command_Stack;
uint16_t command_Execute;

// 1,000,000 microseconds to a second
// T-Staes: 4 + 4 + 12 = 20 ish (NOP,dec,jr)
// 1 T-state on a ZX Spectrum 48K is approximately 0.2857 microseconds.
// 20 T-States / 0.285714 = 70 t-states

// GetValueFromPulseStream: Used to get the command functions (addresses) from the speccy.
// The speccy broadcasts all the functions at power up.
uint16_t GetValueFromPulseStream() {
  constexpr uint16_t PULSE_TIMEOUT_US = 70;
  uint16_t value = 0;
  for (uint8_t i = 0; i < 16; i++) {
    uint8_t pulseCount = 0;
    uint32_t lastPulseTime = 0;
    while (1) {
      // Service current HALT if active
      if ((PINB & (1 << PINB0)) == 0) {  // Waits for Z80 HALT line to go HIGH
        // Pulse the Z80’s /NMI line: LOW -> HIGH to un-halt the CPU.
        digitalWriteFast(Pin::Z80_NMI, LOW);
        digitalWriteFast(Pin::Z80_NMI, HIGH);
        pulseCount++;
        lastPulseTime = micros();  // reset timer, allow another pulse to be sampled
      }
      // Detect end of transmission (delay timeout after last halt)
      if ((pulseCount > 0) && ((micros() - lastPulseTime) > PULSE_TIMEOUT_US)) {
        break;
      }
    }
    if (pulseCount == 2) {
      value += 1 << (15 - i);
    }
  }
  return value;
}
  
void setupFunctions() {
  command_TransmitKey = GetValueFromPulseStream();
  command_Fill = GetValueFromPulseStream();
  command_SmallFill = GetValueFromPulseStream();
  command_Transfer = GetValueFromPulseStream();
  command_Copy = GetValueFromPulseStream();
  command_Copy32 = GetValueFromPulseStream();
  command_Wait = GetValueFromPulseStream();
  command_Stack = GetValueFromPulseStream();
  command_Execute = GetValueFromPulseStream();
  //  oled.println(command_TransmitKey);
}

static inline void buildTransferCommand(uint8_t* buf, uint16_t address, uint8_t length) {
	buf[static_cast<uint8_t>(TransferPacket::CMD_HIGH)] = (uint8_t)((command_Transfer) >> 8);
	buf[static_cast<uint8_t>(TransferPacket::CMD_LOW)] = (uint8_t)((command_Transfer)&0xFF);
	buf[static_cast<uint8_t>(TransferPacket::CMD_LEN)] = length;
	buf[static_cast<uint8_t>(TransferPacket::CMD_DEST_ADDR_HIGH)] = (uint8_t)((address) >> 8);
	buf[static_cast<uint8_t>(TransferPacket::CMD_DEST_ADDR_LOW)] = (uint8_t)((address)&0xFF);
}

static inline void buildSmallFillCommand(uint8_t* buf,uint16_t address, uint8_t runAmount, uint8_t value) {
	buf[static_cast<uint8_t>(SmallFillPacket::CMD_HIGH)] = (uint8_t)((command_SmallFill) >> 8);
	buf[static_cast<uint8_t>(SmallFillPacket::CMD_LOW)] = (uint8_t)((command_SmallFill)&0xFF);
	buf[static_cast<uint8_t>(SmallFillPacket::CMD_AMOUNT)] = runAmount;
	buf[static_cast<uint8_t>(SmallFillPacket::CMD_START_ADDR_HIGH)] = (uint8_t)((address) >> 8);
	buf[static_cast<uint8_t>(SmallFillPacket::CMD_START_ADDR_LOW)] = (uint8_t)((address)&0xFF);
	buf[static_cast<uint8_t>(SmallFillPacket::CMD_FILL_VALUE)] = value;
}

static inline void buildStackCommand(uint8_t* buf,uint16_t address) {
	buf[static_cast<uint8_t>(StackPacket::CMD_HIGH)] = (uint8_t)(command_Stack >> 8);
	buf[static_cast<uint8_t>(StackPacket::CMD_LOW)] = (uint8_t)(command_Stack & 0xFF);
	buf[static_cast<uint8_t>(StackPacket::CMD_SP_ADDR_HIGH)] = (uint8_t)(address >> 8);
	buf[static_cast<uint8_t>(StackPacket::CMD_SP_ADDR_LOW)] = (uint8_t)(address & 0xFF);
}

static inline void buildCopyCommand(uint8_t* buf, uint16_t address, uint8_t length) {
    buf[static_cast<uint8_t>(CopyPacket::CMD_HIGH)] = (uint8_t)((command_Copy) >> 8);
    buf[static_cast<uint8_t>(CopyPacket::CMD_LOW)] = (uint8_t)((command_Copy) & 0xFF);
    buf[static_cast<uint8_t>(CopyPacket::CMD_LEN)] = length;
    buf[static_cast<uint8_t>(CopyPacket::CMD_DEST_ADDR_HIGH)] = (uint8_t)((address) >> 8);
    buf[static_cast<uint8_t>(CopyPacket::CMD_DEST_ADDR_LOW)] = (uint8_t)((address) & 0xFF);
}

static inline void buildFillCommand(uint8_t* buf, uint16_t amount, uint16_t address, uint8_t value) {
    buf[static_cast<uint8_t>(FillPacket::CMD_HIGH)] = (uint8_t)((command_Fill) >> 8);
    buf[static_cast<uint8_t>(FillPacket::CMD_LOW)] = (uint8_t)((command_Fill) & 0xFF);
    buf[static_cast<uint8_t>(FillPacket::CMD_AMOUNT_HIGH)] = (uint8_t)((amount) >> 8);
    buf[static_cast<uint8_t>(FillPacket::CMD_AMOUNT_LOW)] = (uint8_t)((amount) & 0xFF);
    buf[static_cast<uint8_t>(FillPacket::CMD_START_ADDR_HIGH)] = (uint8_t)((address) >> 8);
    buf[static_cast<uint8_t>(FillPacket::CMD_START_ADDR_LOW)] = (uint8_t)((address) & 0xFF);
    buf[static_cast<uint8_t>(FillPacket::CMD_FILL_VALUE)] = value;
}

static inline void buildWaitCommand(uint8_t* buf) {
    buf[static_cast<uint8_t>(WaitPacket::CMD_HIGH)] = (uint8_t)((command_Wait) >> 8);
    buf[static_cast<uint8_t>(WaitPacket::CMD_LOW)] = (uint8_t)((command_Wait) & 0xFF);
}

static inline void buildExecuteCommand(uint8_t* buf) {
    buf[static_cast<uint8_t>(ExecutePacket::CMD_HIGH)] = (uint8_t)((command_Execute) >> 8);
    buf[static_cast<uint8_t>(ExecutePacket::CMD_LOW)] = (uint8_t)((command_Execute) & 0xFF);
}


}  // namespace Buffers

#endif
