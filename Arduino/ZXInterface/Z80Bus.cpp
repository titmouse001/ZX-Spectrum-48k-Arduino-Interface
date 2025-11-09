#include <stdint.h>
#include "digitalWriteFast.h"
#include "SdCardSupport.h"
#include "pin.h"
#include "Z80Bus.h"
#include "PacketBuilder.h"
#include "CommandRegistry.h"
#include "PacketTypes.h"
#include "BufferManager.h"
#include "Utils.h"
#include "draw.h"

constexpr uint8_t COMMAND_ADDR_SIZE = 2;

//-------------------------------------------------
// Z80 Data Transfer and Control Routines
//-------------------------------------------------

__attribute__((optimize("-Os")))
void Z80Bus::setupPins() {
 
  pinModeFast(Pin::ROM_HALF, OUTPUT);  // set ROM_HALF first
  digitalWriteFast(Pin::ROM_HALF, LOW);  //  Switch over to Sna ROM.
  pinModeFast(PIN_A5, OUTPUT);
  digitalWriteFast(PIN_A5,HIGH); 
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

__attribute__((optimize("-Os")))
void Z80Bus::resetZ80() {
  digitalWriteFast(Pin::Z80_REST, LOW);  // begin reset
  // delay(250);   // best guess !!! TO-DO maybe self detect pause needed and also have a config override
  delay(10);
  // Noticed a 48K Spectrum will boot with a very short reset pulse less than 10ms,
  // but a Spectrum +2B needs a much longer hold time, around 250ms.
  digitalWriteFast(Pin::Z80_REST, HIGH);  // release RESET (Z80 restarts)
  delay(10);
}

__attribute__((optimize("-Os")))
void Z80Bus::resetToSnaRom() {
  digitalWriteFast(Pin::ROM_HALF, LOW);  // LOW = Custom ROM
  resetZ80();
}

// // --------------------------------------------------------------------------------------------------------
// // waitRelease_NMI:
// // NMI (Non-Maskable Interrupt) on falling edge of the /NMI pin.
// // Used to synchronize the Z80 with Arduino.
// // --------------------------------------------------------------------------------------------------------
// __attribute__((optimize("-Ofast")))
// void Z80Bus::waitRelease_NMI() {
//   syncWithZ80();
//   triggerZ80NMI();
//   waitForZ80Resume();
// }

// // --------------------------------------------------------------------------------------------------------
// // waitHalt:
// // Waits for the Z80 CPU to complete a HALT cycle
// // The HALT signal is active-low, so we wait for a falling edge (LOW) followed by a rising edge (HIGH)
// // --------------------------------------------------------------------------------------------------------
// __attribute__((optimize("-Ofast")))
// void Z80Bus::waitHalt() {
//   syncWithZ80();
//   waitForZ80Resume();
// }

// TO-DO: measure the difference with cheap oscilloscope , would be interesting to see! (or call tons - results to screen or SD card)
__attribute__((optimize("-Ofast")))
void Z80Bus::sendBytes(uint8_t* data, uint16_t size) {
  // cli();  // maybe saves a tiny bit, guess it depends on number on interrupts during this send.
  for (uint16_t i = 0; i < size; i++) {
    syncWithZ80();
    PORTD = data[i];  // Send to Z80 data lines
    triggerZ80NMI();
    waitForZ80Resume();
  }
  //  sei();
}

__attribute__((optimize("-Os")))
void Z80Bus::sendSnaHeader(uint8_t* header) {
  // NOTE: Order is critical - restoring registers destroys others needed for the restoration process
  // Each byte sent is a planned maneuver to keep the deck of cards from falling over
  constexpr uint8_t PKT_LEN = E(ExecutePacket::PACKET_LEN);
  sendBytes(&header[0 + SNA_I], PKT_LEN + 1 + 2 + 2 + 2 + 2);  // COMMAND ADDR, I,HL',DE',BC',AF'
  sendBytes(&header[PKT_LEN + SNA_IY_LOW], 2 + 2 + 1 + 1);     // IY,IX,IFF2,R
  sendBytes(&header[PKT_LEN + SNA_SP_LOW], 2);                 // The rest aren't in sequence...
  sendBytes(&header[PKT_LEN + SNA_HL_LOW], 2);
  sendBytes(&header[PKT_LEN + SNA_IM_MODE], 1);
  sendBytes(&header[PKT_LEN + SNA_BORDER_COLOUR], 1);
  sendBytes(&header[PKT_LEN + SNA_DE_LOW], 2);
  sendBytes(&header[PKT_LEN + SNA_BC_LOW], 2);
  sendBytes(&header[PKT_LEN + SNA_AF_LOW], 2);
}

void Z80Bus::fillScreenAttributes(const uint8_t col) {
  uint8_t packetLen = PacketBuilder::buildFillCommand(BufferManager::packetBuffer, ZX_SCREEN_ATTR_SIZE, ZX_SCREEN_ATTR_ADDRESS_START, col);
  Z80Bus::sendBytes(BufferManager::packetBuffer, packetLen);
}

void Z80Bus::sendFillCommand(uint16_t address, uint16_t amount, uint8_t color) {
  uint8_t packetLen = PacketBuilder::buildFillCommand(BufferManager::packetBuffer, amount, address, color);
  Z80Bus::sendBytes(BufferManager::packetBuffer, packetLen);
}

void Z80Bus::sendSmallFillCommand(uint16_t address, uint8_t amount, uint8_t color) {
  uint8_t packetLen = PacketBuilder::buildSmallFillCommand(BufferManager::packetBuffer, amount, address, color);
  Z80Bus::sendBytes(BufferManager::packetBuffer, packetLen);
}

void Z80Bus::sendCopyCommand(uint16_t address, uint8_t amount) {
  uint8_t packetLen = PacketBuilder::buildCopyCommand(BufferManager::packetBuffer, address, amount);
  Z80Bus::sendBytes(BufferManager::packetBuffer, packetLen + amount);
}

void Z80Bus::sendWaitVBLCommand() {
  uint8_t packetLen = PacketBuilder::buildWaitVBLCommand(BufferManager::packetBuffer);
  Z80Bus::sendBytes(BufferManager::packetBuffer, packetLen);
}

void Z80Bus::sendStackCommand(uint16_t addr) {
  uint8_t packetLen = PacketBuilder::buildStackCommand(BufferManager::packetBuffer, addr);
  Z80Bus::sendBytes(BufferManager::packetBuffer, packetLen);
}

__attribute__((optimize("-Os")))
void Z80Bus::highlightSelection(uint16_t currentFileIndex, uint16_t startFileIndex, uint16_t& oldHighlightAddress) {
  const uint16_t fillAddr = ZX_SCREEN_ATTR_ADDRESS_START + ((currentFileIndex - startFileIndex) * ZX_SCREEN_WIDTH_BYTES);
  if (oldHighlightAddress != fillAddr) {  // Clear old highlight if it's different
    sendFillCommand(oldHighlightAddress, ZX_SCREEN_WIDTH_BYTES, COL::BLACK_WHITE);
    oldHighlightAddress = fillAddr;
  }
  sendFillCommand(fillAddr, ZX_SCREEN_WIDTH_BYTES, COL::CYAN_BLACK);
}

uint8_t Z80Bus::getByte() {
  // Command Z80 to send us a byte via the 'out' instruction.
  BufferManager::packetBuffer[static_cast<uint8_t>(ReceiveKeyboardPacket::CMD_HIGH)] = (uint8_t)((CommandRegistry::command_TransmitKey) >> 8);
  BufferManager::packetBuffer[static_cast<uint8_t>(ReceiveKeyboardPacket::CMD_LOW)] = (uint8_t)((CommandRegistry::command_TransmitKey) & 0xFF);
  Z80Bus::sendBytes(BufferManager::packetBuffer, static_cast<uint8_t>(ReceiveKeyboardPacket::PACKET_LEN));

  // -------------------------------------------------------------
  // Prepare to read from Latch IC (74HC574)
  // Set Nano data pins to input mode before Z80 writes to latch.
  syncWithZ80();      // wait for Z80 to HALT
  DDRD = 0x00;        // all inputs
  triggerZ80NMI();    // Unhalt Z80 to allow it to do a I/O out instruction
  waitForZ80Resume(); // Wait for Z80 to complete the out instruction
  // -------------------------------------------------------------

  // -------------------------------------------------------------
  // Enable and read the new latched data
  digitalWriteFast(PIN_A5, LOW); // Enable: #OE on the latch IC

  // Timing delay: 74HC574 needs around 38ns (5V) for output enable time.
  // The Nano can execute the next instruction before latch outputs are ready!
  __asm__ __volatile__("nop\n\t"   // single nop is about 62ns @16MHz
                       "nop\n\t"); // playing it safe with 124 ns (!!TIMINGS FOR NANO ONLY!!)
  uint8_t byte = PIND;             // Read all 8 bits from latch
  // -------------------------------------------------------------

  // -------------------------------------------------------------
  // Cleanup: disable latch and restore Nano to output mode
  digitalWriteFast(PIN_A5, HIGH); // Disable latch (#OE high=tri-state)
  DDRD = 0xFF;                    // Set all PORTD pins as outputs
  // -------------------------------------------------------------
  return byte;
}

// Encode/send RLE-compressed data to the Speccy which is decompresses its side.
// Uses TransferPacket for raw blocks and SmallFillPacket for repeated byte runs.
// Both packet types max out at 255 payload bytes.
__attribute__((optimize("-Ofast")))
void Z80Bus::encodeTransferPacket(uint16_t input_len, uint16_t addr, bool borderLoadingEffect) {
  // {
  //  DEBUG FOR TESTING WITHOUT RLE (RLE JUST HELPS SPEEDUP TRANSFER)
  //   uint8_t* pTransfer = &BufferManager::packetBuffer[0];
  //   uint8_t packetLen = PacketBuilder::buildTransferCommand(pTransfer, addr, input_len);
  //   Z80Bus::sendBytes(pTransfer, packetLen + input_len);
  //   return ;
  // }
  constexpr uint16_t MAX_RUN_LENGTH = 255;
  constexpr uint16_t MAX_RAW_LENGTH = 255;
  constexpr uint8_t MIN_RUN_LENGTH = 3;  // about where RLE pays off over raw

  // Starts after the reserved packet header!
  uint8_t* input = &BufferManager::packetBuffer[GLOBAL_MAX_PACKET_LEN];

  if (input_len == 0) return;
  uint16_t i = 0;
  while (i < input_len) {
    uint8_t value = input[i];
    uint16_t remaining = input_len - i;
    uint16_t max_run = (remaining > MAX_RUN_LENGTH) ? MAX_RUN_LENGTH : remaining;
    uint16_t run_len = 1;

    // Check for a run of repeated values
    while (run_len < max_run && input[i + run_len] == value) {
      run_len++;
    }
    if (run_len >= MIN_RUN_LENGTH) {  // run found (with payoff)
      if (value != 0) {               // ignore zero as we cleared memory at start
        // --- Send as SmallFillPacket ---
        // Using offset after input header and data so we don't touch the lower part of packetBuffer as it still holds input to be processed
        uint8_t* pFill = &BufferManager::packetBuffer[COMMAND_PAYLOAD_SECTION_SIZE + GLOBAL_MAX_PACKET_LEN];
        if ((run_len & 0x01) == 0) {  // even
                                      // number of PUSH operations ((count-1)/2)
          uint8_t packetLen = PacketBuilder::buildFillVariableEvenCommand(pFill, addr + run_len, run_len / 2, value);
          Z80Bus::sendBytes(pFill, packetLen);
        } else {
          // number of PUSH operations (count/2)
          uint8_t packetLen = PacketBuilder::buildFillVariableOddCommand(pFill, addr + run_len, (run_len - 1) / 2, value);
          Z80Bus::sendBytes(pFill, packetLen);
        }
      }
      addr += run_len;
      i += run_len;
    } else {
      // --- Send as raw TransferPacket ---
      uint16_t raw_start = i;
      uint16_t max_raw = (remaining > MAX_RAW_LENGTH) ? MAX_RAW_LENGTH : remaining;
      // Start with current byte, then gather more until a run is detected or max length hit
      uint16_t raw_len = 1;
      i++;
      while (raw_len < max_raw && i < input_len) {
        if (raw_len + 1 < max_raw && input[i] == input[i + 1]) {
          break;  // stop - run found
        }
        raw_len++;
        i++;
      }

      // Place packet header before payload in packetBuffer (now we can send as a single contiguous block)
      if (borderLoadingEffect) {
        uint8_t* pTransfer = &BufferManager::packetBuffer[GLOBAL_MAX_PACKET_LEN + ((int16_t)raw_start - E(TransferPacket::PACKET_LEN))];
        Z80Bus::sendBytes(pTransfer, PacketBuilder::buildTransferCommand(pTransfer, addr, raw_len) + raw_len);
      } else {
        uint8_t* pTransfer = &BufferManager::packetBuffer[GLOBAL_MAX_PACKET_LEN + ((int16_t)raw_start - E(CopyPacket::PACKET_LEN))];
        Z80Bus::sendBytes(pTransfer, PacketBuilder::buildCopyCommand(pTransfer, addr, raw_len) + raw_len);
      }
      addr += raw_len;
    }
  }
}

void Z80Bus::transferSnaData(FatFile* pFile, bool borderLoadingEffect) {
  uint16_t currentAddress = ZX_SCREEN_ADDRESS_START;
  // Transfer data to Spectrum RAM
  while (pFile->available()) {
    uint16_t bytesRead = pFile->read(&BufferManager::packetBuffer[GLOBAL_MAX_PACKET_LEN], COMMAND_PAYLOAD_SECTION_SIZE);
    encodeTransferPacket(bytesRead, currentAddress, borderLoadingEffect);
    currentAddress += bytesRead;
  }
}

void Z80Bus::synchronizeForExecution() {
  // Enable Spectrum's 50Hz maskable interrupt and sync timing
  // Creates gap before next interrupt to avoid stack corruption during restore
  sendWaitVBLCommand();
//  Z80Bus::waitHalt();
  syncWithZ80();
  waitForZ80Resume();
}


/* executeSnapshot:
 * See Z80 code around L16D4. It will idle-loop to allow bankswitching into the 
 * stock ROM to continue execution into RAM to jump back into game code. 
 * (Previously this needed an extra messy HALT and a precise wait for the NMI to complete)
 */
void Z80Bus::executeSnapshot() {
  PacketBuilder::buildExecuteCommand(BufferManager::head27_Execute);
  Z80Bus::sendSnaHeader(BufferManager::head27_Execute);
  delay(1);  // allow 'L16D4' time to reach idle loop
  digitalWriteFast(Pin::ROM_HALF, HIGH);
}

boolean Z80Bus::bootFromSnapshot(FatFile* pFile) {
  Z80Bus::sendStackCommand(ZX_SCREEN_ADDRESS_START + 3);  // Initialize stack pointer
  //Z80Bus::waitRelease_NMI();
   syncWithZ80();
   triggerZ80NMI();
   waitForZ80Resume();

  Z80Bus::fillScreenAttributes(0);
  Z80Bus::clearScreen();
  Z80Bus::transferSnaData(pFile, true);
  synchronizeForExecution();
  executeSnapshot();
  return true;
}

void Z80Bus::clearScreen(uint8_t col) {
  Z80Bus::fillScreenAttributes(col);
  Z80Bus::sendFillCommand(ZX_SCREEN_ADDRESS_START, ZX_SCREEN_BITMAP_SIZE, 0);
}

void Z80Bus::syncWithZ80() {
  // Wait until the Z80 enters the HALT state (active low).
  while (digitalReadFast(Pin::Z80_HALT) != 0) {};  
}

void Z80Bus::triggerZ80NMI() {
  // Pulse the Z80’s /NMI line: LOW -> HIGH to un-halt the CPU.
  digitalWriteFast(Pin::Z80_NMI, LOW);
 // delayMicroseconds(1);     // other platforms may need a pause
  digitalWriteFast(Pin::Z80_NMI, HIGH);
}


void Z80Bus::waitForZ80Resume(){
  // wait until HALT goes HIGH (Z80 resumes)
   while (digitalReadFast(Pin::Z80_HALT) == 0) {};  
}

// __attribute__((optimize("-Os")))
// uint8_t Z80Bus::GetKeyPulses_NO_LONGER_USED() {
//   //constexpr uint8_t DELAY_ITERATIONS_PARAM = 20;  // 20 loops of 25 t-states
//   BufferManager::packetBuffer[static_cast<uint8_t>(ReceiveKeyboardPacket::CMD_HIGH)] = (uint8_t)((CommandRegistry::command_TransmitKey) >> 8);
//   BufferManager::packetBuffer[static_cast<uint8_t>(ReceiveKeyboardPacket::CMD_LOW)] = (uint8_t)((CommandRegistry::command_TransmitKey)&0xFF);
//  // BufferManager::packetBuffer[static_cast<uint8_t>(TransmitKeyPacket::CMD_DELAY)] = DELAY_ITERATIONS_PARAM;  // delay use as end marker
//   Z80Bus::sendBytes(BufferManager::packetBuffer, static_cast<uint8_t>(ReceiveKeyboardPacket::PACKET_LEN));
//   return Utils::get8bitPulseValue();
// }
