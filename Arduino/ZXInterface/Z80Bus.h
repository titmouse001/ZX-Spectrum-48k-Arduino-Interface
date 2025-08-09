#ifndef Z80BUS_H
#define Z80BUS_H

#include <Arduino.h>
#include "pin.h"
#include "digitalWriteFast.h"
#include "packet_types.h"
#include "buffer_manager.h"
#include "packet_builder.h"
#include "Command_registry.h"

namespace Z80Bus {

constexpr uint8_t COMMAND_ADDR_SIZE = 2;

//-------------------------------------------------
// Z80 Data Transfer and Control Routines
//-------------------------------------------------

void setupPins() {

  // best do ROM_HALF setting first
  pinModeFast(Pin::ROM_HALF, OUTPUT);
  digitalWriteFast(Pin::ROM_HALF, LOW);  //  Switch over to Sna ROM.

  pinModeFast(Pin::Z80_HALT, INPUT);
  pinModeFast(Pin::Z80_NMI, OUTPUT);
  pinModeFast(Pin::Z80_REST, OUTPUT);
  pinModeFast(Pin::Z80_D0Pin, OUTPUT);
  pinModeFast(Pin::Z80_D1Pin, OUTPUT);
  pinModeFast(Pin::Z80_D2Pin, OUTPUT);
  pinModeFast(Pin::Z80_D3Pin, OUTPUT);
  pinModeFast(Pin::Z80_D4Pin, OUTPUT);
  pinModeFast(Pin::Z80_D5Pin, OUTPUT);
  pinModeFast(Pin::Z80_D6Pin, OUTPUT);
  pinModeFast(Pin::Z80_D7Pin, OUTPUT);

  // setup /NMI - Z80 /NMI, used to trigger a 'HALT' so the Z80 can be released and resume
  digitalWriteFast(Pin::Z80_NMI, HIGH);  // put into a default state
}

// waitHalt: Waits for the Z80 CPU to complete a HALT cycle
// The HALT signal is active-low, so we wait for a falling edge (LOW) followed by a rising edge (HIGH)
void waitHalt() {   
  // Together these synchronise with the end of the Z80's HALT state
  while (digitalReadFast(Pin::Z80_HALT) != 0) {};  // wait until HALT goes LOW (Z80 is halted)
  while (digitalReadFast(Pin::Z80_HALT) == 0) {};  // wait until HALT goes HIGH (Z80 resumes)
}

void resetZ80() {
  digitalWriteFast(Pin::Z80_REST, LOW);   // 
  // REDUCE THIS... SPECCY WILL BE POWERED, NO CAPS TO DRAIN.
  delay(10);                              // 3 speccy T-States!!!
  digitalWriteFast(Pin::Z80_REST, HIGH);  // reboot
}

inline void resetToSnaRom() {
  digitalWriteFast(Pin::ROM_HALF, LOW);   // LOW = Custom ROM
  resetZ80();
}

void waitRelease_NMI() {
  // Wait for Z80 HALT line to go LOW (active low)
  while (digitalReadFast(Pin::Z80_HALT) != 0) {};
  // Pulse the Z80’s /NMI line: LOW -> HIGH to un-halt the CPU.
  digitalWriteFast(Pin::Z80_NMI, LOW);
  //   asm volatile(
  //   "nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n"  // 8 NOPs = 500ns @ 16MHz
  //   "nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n"  // 16 total NOPs
  // );
  digitalWriteFast(Pin::Z80_NMI, HIGH);
  // Wait for HALT line to return HIGH again (shows Z80 has resumed)
  while (digitalReadFast(Pin::Z80_HALT) == 0) {};
}

// TO-DO: measure the difference with cheap oscilloscope , would be interesting to see! (or call tons - results to screen or SD card)
__attribute__((optimize("-Ofast"))) 
void sendBytes(byte* data, uint16_t size) {
  cli();  // maybe saves a tiny bit, guess it depends on number on interrupts during this send.
  for (uint16_t i = 0; i < size; i++) {
    // Wait for Z80 HALT line to go LOW (active low)
    while (digitalReadFast(Pin::Z80_HALT) != 0) {};
    PORTD = data[i];  // Send Arduino d0-d7 to Z80 d0-d7
    // Pulse the Z80’s /NMI line: LOW -> HIGH to un-halt the CPU.
    digitalWriteFast(Pin::Z80_NMI, LOW);
    //   asm volatile(
    //     "nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n"  // 8 NOPs = 500ns @ 16MHz
    //    "nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n"  // 16 total NOPs
    //  );
    digitalWriteFast(Pin::Z80_NMI, HIGH);
    // Wait for HALT line to return HIGH again (shows Z80 has resumed)
    while (digitalReadFast(Pin::Z80_HALT) == 0) {};
  }
  sei();
}

void sendSnaHeader(byte* header) {
  // NOTE: Order is critical - restoring registers destroys others needed for the restoration process
  // Each byte sent is a planned maneuver to keep the deck of cards from falling over

  constexpr uint8_t PKT_LEN = E(ExecutePacket::PACKET_LEN);
  Z80Bus::sendBytes(&header[0       +      SNA_I], PKT_LEN+1+2+2+2+2);  // COMMAND ADDR, I,HL',DE',BC',AF'
  Z80Bus::sendBytes(&header[PKT_LEN + SNA_IY_LOW], 2+2+1+1);            // IY,IX,IFF2,R 
  Z80Bus::sendBytes(&header[PKT_LEN + SNA_SP_LOW], 2);                  // The rest aren't in sequence...   
  Z80Bus::sendBytes(&header[PKT_LEN + SNA_HL_LOW], 2);                   
  Z80Bus::sendBytes(&header[PKT_LEN + SNA_IM_MODE], 1);                  
  Z80Bus::sendBytes(&header[PKT_LEN + SNA_BORDER_COLOUR], 1);            
  Z80Bus::sendBytes(&header[PKT_LEN + SNA_DE_LOW], 2);             
  Z80Bus::sendBytes(&header[PKT_LEN + SNA_BC_LOW], 2);             
  Z80Bus::sendBytes(&header[PKT_LEN + SNA_AF_LOW], 2);             
}

void fillScreenAttributes(const uint8_t col) {
  uint8_t packetLen = PacketBuilder::buildFillCommand(BufferManager::packetBuffer, ZX_SCREEN_ATTR_SIZE, ZX_SCREEN_ATTR_ADDRESS_START, col);
  Z80Bus::sendBytes(BufferManager::packetBuffer, packetLen);
}

void sendFillCommand(uint16_t address, uint16_t amount, uint8_t color) {
  uint8_t packetLen = PacketBuilder::buildFillCommand(BufferManager::packetBuffer, amount, address, color);
  Z80Bus::sendBytes(BufferManager::packetBuffer, packetLen);
}

void sendSmallFillCommand(uint16_t address, uint8_t amount, uint8_t color) {
  uint8_t packetLen = PacketBuilder::buildSmallFillCommand(BufferManager::packetBuffer, amount, address, color);
  Z80Bus::sendBytes(BufferManager::packetBuffer, packetLen);
}

void sendCopyCommand( uint16_t address, uint8_t amount) {
  uint8_t packetLen = PacketBuilder::buildCopyCommand(BufferManager::packetBuffer, address, amount);
  Z80Bus::sendBytes(BufferManager::packetBuffer, packetLen + amount);
}

void sendWaitVBLCommand() {
    uint8_t packetLen = PacketBuilder::buildWaitVBLCommand(BufferManager::packetBuffer);  
    Z80Bus::sendBytes(BufferManager::packetBuffer, packetLen);     
}

void sendStackCommand(uint16_t addr) {
    uint8_t packetLen = PacketBuilder::buildStackCommand(BufferManager::packetBuffer, addr);   
    Z80Bus::sendBytes(BufferManager::packetBuffer, packetLen );
}

void highlightSelection(uint16_t currentFileIndex, uint16_t startFileIndex, uint16_t& oldHighlightAddress) {
  const uint16_t fillAddr = ZX_SCREEN_ATTR_ADDRESS_START + ((currentFileIndex - startFileIndex) * ZX_SCREEN_WIDTH_BYTES);
  if (oldHighlightAddress != fillAddr) {   // Clear old highlight if it's different
    sendFillCommand(oldHighlightAddress, ZX_SCREEN_WIDTH_BYTES, COL::BLACK_WHITE);
    oldHighlightAddress = fillAddr;
  }
  sendFillCommand(fillAddr, ZX_SCREEN_WIDTH_BYTES, COL::CYAN_BLACK);
}

// TO DO - upgrade to use 6bit binary
uint8_t GetKeyPulses() {

  constexpr uint8_t DELAY_ITERATIONS_PARAM = 20;  // 20 loops of 25 t-states
  constexpr uint16_t PULSE_TIMEOUT_US = 70;

  BufferManager::packetBuffer[static_cast<uint8_t>(TransmitKeyPacket::CMD_HIGH)] = (uint8_t)((CommandRegistry::command_TransmitKey) >> 8);
  BufferManager::packetBuffer[static_cast<uint8_t>(TransmitKeyPacket::CMD_LOW)] = (uint8_t)((CommandRegistry::command_TransmitKey)&0xFF);
  BufferManager::packetBuffer[static_cast<uint8_t>(TransmitKeyPacket::CMD_DELAY)] = DELAY_ITERATIONS_PARAM;  // delay use as end marker
  Z80Bus::sendBytes(BufferManager::packetBuffer, static_cast<uint8_t>(TransmitKeyPacket::PACKET_LEN));

  uint8_t pulseCount = 0;
  uint32_t lastPulseTime = 0;
  while (1) {                          // Tally halts, noting the end marker is just a time gap
    if ((PINB & (1 << PINB0)) == 0) {  // halt active
      // Signal the speccy to un-halt the Z80 - Pulse the Z80’s /NMI line: LOW -> HIGH
      digitalWriteFast(Pin::Z80_NMI, LOW);
      digitalWriteFast(Pin::Z80_NMI, HIGH);
      lastPulseTime = micros();  // reset timer, allow another pulse to be sampled
      pulseCount++;
    }

    // Detect end of transmission (delay timeout after last halt)
    if ((pulseCount > 0) && ((micros() - lastPulseTime) > PULSE_TIMEOUT_US)) {
      return pulseCount - 1;
    }
  }
}

// RLE and send with - TransferPacket and FillPacket (both send max of 255 bytes)
// The Speccy will reconstruct the RLE at its end.
void encodeTransferPacket(uint16_t input_len, uint16_t addr) {
  constexpr uint16_t MAX_RUN_LENGTH = 255;
  constexpr uint16_t MAX_RAW_LENGTH = 255;
  constexpr uint8_t MIN_RUN_LENGTH = 5;  // about where RLE pays off over raw

  uint8_t* input = &BufferManager::packetBuffer[E(TransferPacket::PACKET_LEN)];

  if (input_len == 0) return;
  uint16_t i = 0;
  while (i < input_len) {
    uint8_t value = input[i];
    uint16_t remaining = input_len - i;
    uint16_t max_run = (remaining > MAX_RUN_LENGTH) ? MAX_RUN_LENGTH : remaining;
    uint16_t run_len = 1;

    // Check for run
    while (run_len < max_run && input[i + run_len] == value) {
      run_len++;
    }
    if (run_len >= MIN_RUN_LENGTH) {  // run found (with payoff)
      uint8_t* pFill = &BufferManager::packetBuffer[TOTAL_PACKET_BUFFER_SIZE - E(SmallFillPacket::PACKET_LEN)];      // send [PB-6] to [PB-1]
      uint8_t packetLen = PacketBuilder::buildSmallFillCommand(pFill, run_len,addr, value);
      Z80Bus::sendBytes(pFill, packetLen);
    
      addr += run_len;
      i += run_len;
    } else {  // No run found - raw data
      uint16_t raw_start = i;
      uint16_t max_raw = (remaining > MAX_RAW_LENGTH) ? MAX_RAW_LENGTH : remaining;
      uint16_t raw_len = 1;  // We know current byte isn't part of a run
      i++;
      while (raw_len < max_raw && i < input_len) {
        if (raw_len + 1 < max_raw && input[i] == input[i + 1]) {
          break;  // stop - run found
        }
        raw_len++;
        i++;
      }
      uint8_t* pTransfer = &BufferManager::packetBuffer[raw_start];      // send [0] to [255]
      uint8_t packetLen = PacketBuilder::buildTransferCommand(pTransfer, addr, raw_len);
      Z80Bus::sendBytes(pTransfer, packetLen + raw_len);
      addr += raw_len;
    }
  }
}


}

#endif