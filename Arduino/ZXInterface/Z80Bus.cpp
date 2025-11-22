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

__attribute__((optimize("-Ofast")))
void Z80Bus::sendBytes(uint8_t* data, uint16_t size) {
  // cli();  // maybe saves a tiny bit, guess it depends on number on interrupts during this send.
  for (uint16_t i = 0; i < size; i++) {
    waitHalt_syncWithZ80();
    PORTD = data[i];  // Send to Z80 data lines
    unHalt_triggerZ80NMI();
 ////////////   waitForZ80Resume();
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

void Z80Bus::sendFillCommand(uint16_t address, uint16_t amount, uint8_t color) {
  uint8_t packetLen = PacketBuilder::buildFillCommand(BufferManager::packetBuffer, amount, address, color);
  Z80Bus::sendBytes(BufferManager::packetBuffer, packetLen);
}

void Z80Bus::sendWaitVBLCommand() {
  uint8_t packetLen = PacketBuilder::buildWaitVBLCommand(BufferManager::packetBuffer);
  Z80Bus::sendBytes(BufferManager::packetBuffer, packetLen);
}

void Z80Bus::sendStackCommand(uint16_t addr, uint8_t action) {
  uint8_t packetLen = PacketBuilder::buildStackCommand(BufferManager::packetBuffer, addr, action);
  Z80Bus::sendBytes(BufferManager::packetBuffer, packetLen);
}

uint8_t Z80Bus::get_IO_Byte() {
  // get_IO_Byte will be used inside a command like 'sendFunctionList' (se we don't use BufferManager here)
  // -------------------------------------------------------------
  // Prepare to read from Latch IC (74HC574)
  // Set Nano data pins to input mode before Z80 writes to latch.
  waitHalt_syncWithZ80();      // wait for Z80 to HALT
  DDRD = 0x00;        // all inputs
  unHalt_triggerZ80NMI();    // Unhalt Z80 to allow it to do a I/O out instruction
  ///////////////waitForZ80Resume(); // Wait for Z80 to complete the out instruction
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

uint8_t Z80Bus::getKeyboard() {
  // Send command packet to the Z80 asking it to send back a byte with the OUT instruction (TransmitKey) 
  BufferManager::packetBuffer[(uint8_t)ReceiveKeyboardPacket::CMD_HIGH] = (uint8_t)(CommandRegistry::command_TransmitKey >> 8);
  BufferManager::packetBuffer[(uint8_t)ReceiveKeyboardPacket::CMD_LOW] =  (uint8_t)(CommandRegistry::command_TransmitKey & 0xFF);
  Z80Bus::sendBytes(BufferManager::packetBuffer, (uint8_t)ReceiveKeyboardPacket::PACKET_LEN);

  return get_IO_Byte();
}

//------------------------------------------------------------------------------------------
// This transfer has a simple look ahead to see if data can be RLE-compressed. 
// When a performance payoff for using RLE is found, the data is encoded and the RLE-compressed 
// block is sent to the Z80 using fast fill commands.
// We do things this way as the Z80 can use PUSH instructions to fill 2 bytes per instruction, 
// so runs of repeated bytes are handled very efficiently..
//
// Both buildFillVariableEvenCommand() and buildFillVariableOddCommand() are used to
// maximise performance depending on whether the run length is even or odd.
//
// Currently, fill packets use a maximum of 255 payload bytes (as we /2 we could use 511).
//
// When no compression is found (short run/random data), raw bytes are sent instead,
// limited by the working buffer size.
//
__attribute__((optimize("-Ofast")))
void Z80Bus::rleOptimisedTransfer(uint16_t input_len, uint16_t addr, bool borderLoadingEffect) {
  // {
  //  DEBUG FOR TESTING WITHOUT RLE (RLE JUST HELPS SPEEDUP TRANSFER)
  //  (!!all the code bellow is really doing this - but it's using the Z80 in a more efficient way!!!)
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
        // Using offset after input header and data so we don't touch the lower part of
        // packetBuffer as it still holds input to be processed
        uint8_t* pFill = &BufferManager::packetBuffer[COMMAND_PAYLOAD_SECTION_SIZE + GLOBAL_MAX_PACKET_LEN];

        uint8_t packetLen = PacketBuilder::build_command_fill_mem_bytecount(pFill, addr + run_len, run_len, value);
	      Z80Bus::sendBytes(pFill, packetLen);

        // if ((run_len & 0x01) == 0) {  
        //   // even number of PUSH operations ((count-1)/2)
        //   uint8_t packetLen = PacketBuilder::build_command_fill_mem_bytecount(pFill, addr + run_len, run_len / 2, value);
        //   Z80Bus::sendBytes(pFill, packetLen);
        // } else {
        //   // number of PUSH operations (count/2)
        //   uint8_t packetLen = PacketBuilder::build_command_fill_mem_bytecount(pFill, addr + run_len, (run_len - 1) / 2, value);
        //   Z80Bus::sendBytes(pFill, packetLen);
        // }
      }
      addr += run_len;
      i += run_len;
    } else {
       // Fallback to sending plain byte data (as data was a bad fit for RLE) 
      uint16_t raw_start = i;
      uint16_t max_raw = (remaining > MAX_RAW_LENGTH) ? MAX_RAW_LENGTH : remaining;
      // Start with current byte, then gather more until a run is detected or max length hit
      uint16_t raw_len = 1;
      i++;
      // Find where next RLE starts
      while (raw_len < max_raw && i < input_len) {
        if (raw_len + 1 < max_raw && input[i] == input[i + 1]) {
          break;  // stop - run found
        }
        raw_len++;
        i++;
      }

      // Packet header command is calculated to be placed just before the payload.
      // The caller reading from file has added GLOBAL_MAX_PACKET_LEN to allow for any header type to be copied in.
      // (this location is outside the range of the RLE buffer area)
      if (borderLoadingEffect) {
        // Transfer gives a flashing border loader effect
        uint8_t* pTransfer = &BufferManager::packetBuffer[GLOBAL_MAX_PACKET_LEN + ((int16_t)raw_start - E(TransferPacket::PACKET_LEN))];
        Z80Bus::sendBytes(pTransfer, PacketBuilder::buildTransferCommand(pTransfer, addr, raw_len) + raw_len);
      } else {
        // Copy is plain transfer without flashing borders
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
    rleOptimisedTransfer(bytesRead, currentAddress, borderLoadingEffect);
    currentAddress += bytesRead;
  }
}

// void Z80Bus::synchronizeForExecution() {
//   // Re-enable the Spectrum's 50 Hz interrupt (IM 1).
//   //
//   // This gives a safty gap before the next interrupt so the game's ISR won't
//   // corrupt the stack when execution resumes after restoring the snapshot.
//   //
//   // synchronizeForExecution() would normally be called just before executeSnapshot()
//   // // // //
//   // // // sendWaitVBLCommand();
//   // // // waitHalt_syncWithZ80(); // Wait for HALT to assert (interrupt sync point)
//   // // // waitForZ80Resume();     // Wait for HALT to clear (interrupt occurred, Z80 running again)
// }


/* executeSnapshot:
 * See Z80 code around L16D4. It will idle-loop to allow bankswitching into the 
 * stock ROM to continue execution into RAM to jump back into game code. 
 * (Previously this needed an extra messy HALT and a precise wait for the NMI to complete)
 */
void Z80Bus::executeSnapshot() {

  // -----------------------------------
  // Re-enable the Spectrum's 50 Hz maskable interrupt (IM 1).
  // This provides a safety gap before the next interrupt so the game's ISR won't corrupt the stack
  // when the launch code is in the last stages of resuming the game. The Arduino needs to synchronize via NMI 
  // to send data and that also uses the Z80's stack.
  // NOTE: On the Z80 side - the SNA ROM does not execute 'EI' when exiting the maskable interrupt
  // (IM 1) at vector 0x0038. This means IM 1 interrupts remain disabled when the ISR returns.

  sendWaitVBLCommand();
  waitHalt_syncWithZ80(); // Wait for HALT to assert (interrupt sync point)
  waitForZ80Resume();     // Wait for HALT to clear (interrupt occurred, Z80 running again)

//TODO ... maybe do a 2nd stage to EI (at game time !??!?!) and avoid this IM 1 sync gap

  // -----------------------------------

  PacketBuilder::buildExecuteCommand(BufferManager::head27_Execute);
  sendSnaHeader(BufferManager::head27_Execute);
  delay(1);  // allow 'L16D4' time to reach idle loop
  digitalWriteFast(Pin::ROM_HALF, HIGH);
}

boolean Z80Bus::bootFromSnapshot(FatFile* pFile) {

  // Navigation away from menu. Restoring snap - so can't leave menu's stack (0xFFFF) in place.
  // We need to put the Z80 stack somewhere it will do less damage.
  // Temporary working stack in screen memory is 99.9% ok.
  // 0x4000 will infact also have the z80 jp <patched-start-addr> launch code.
  // +3 as we can borrow that addr for push/pops until it's set near end of SNA restore.
  sendStackCommand(ZX_SCREEN_ADDRESS_START + 3, 1 ); 

  waitHalt_syncWithZ80();
  unHalt_triggerZ80NMI();
 ///////////// waitForZ80Resume();
  Utils::clearScreen(0);
  transferSnaData(pFile, true);
  //////////synchronizeForExecution();
  executeSnapshot();

  // At this point the Z80's game has been fuly restored including it's stack and registers.
  return true;
}

void Z80Bus::waitHalt_syncWithZ80() {
  // Wait until the Z80 enters the HALT state (active low).
  while (digitalReadFast(Pin::Z80_HALT) != 0) {};  
}

void Z80Bus::unHalt_triggerZ80NMI() {
  // Pulse the Z80’s /NMI line: LOW -> HIGH to un-halt the CPU.
  digitalWriteFast(Pin::Z80_NMI, LOW);
 // delayMicroseconds(1);     // other Arduino platforms may need a pause
  digitalWriteFast(Pin::Z80_NMI, HIGH);

  waitForZ80Resume();
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
