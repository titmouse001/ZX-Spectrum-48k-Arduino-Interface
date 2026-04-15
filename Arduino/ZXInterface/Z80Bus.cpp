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
 
  pinModeFast(Pin::OE_LATCH, OUTPUT);
  digitalWriteFast(Pin::OE_LATCH,HIGH);   //74HC574PW , #OE_LATCH

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

// TODO - reset timing for 128k machines
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

void Z80Bus::setSnaRom() {
  digitalWriteFast(Pin::ROM_HALF, LOW);  // LOW = Snaploader Custom ROM
}

void Z80Bus::setStockRom() {
  digitalWriteFast(Pin::ROM_HALF, HIGH);  // HIGH = Stock ROM
}

__attribute__((optimize("-Ofast")))
void Z80Bus::sendBytes(uint8_t* data, uint16_t size) {
  // cli();  // maybe saves a tiny bit, guess it depends on number on interrupts during this send.
  for (uint16_t i = 0; i < size; i++) {
    waitHalt_syncWithZ80();
    PORTD = data[i];  // data on lines ready for Z80s 'IN'
    triggerZ80NMI();
  }
  //  sei();
}

//TODO - PUT THIS IN Snapshot.cpp  (rename snapZ80 - allow for all sna,z80 ...)

__attribute__((optimize("-Os")))
void Z80Bus::sendSnaHeader(uint8_t* header) {
  uint8_t buf[2];
  uint8_t cmdLength = PacketBuilder::buildExecuteCommand(buf);
  sendBytes(buf, cmdLength);

  sendBytes(&header[SNA_I],  1 + 2 + 2 + 2 + 2);    // I,HL',DE',BC',AF'
  sendBytes(&header[SNA_IY_LOW], 2 + 2 + 1 + 1);    // IY,IX,IFF2,R
  sendBytes(&header[SNA_SP_LOW], 2);                // The rest aren't in sequence...
  sendBytes(&header[SNA_HL_LOW], 2);
  sendBytes(&header[SNA_IM_MODE], 1);
  sendBytes(&header[SNA_BORDER_COLOUR], 1);
  sendBytes(&header[SNA_DE_LOW], 2);
  sendBytes(&header[SNA_BC_LOW], 2);
  sendBytes(&header[SNA_AF_LOW], 2);
}

void Z80Bus::sendFillCommand(uint16_t address, uint16_t amount, uint8_t color) {
//  uint8_t packetLen = PacketBuilder::buildFillCommand(BufferManager::packetBuffer, amount, address, color);
//  Z80Bus::sendBytes(BufferManager::packetBuffer, packetLen);


	uint16_t mark = BufferManager::getMark();
	uint8_t* buf = BufferManager::allocate(  E(FillPacket::PACKET_LEN) );

  uint8_t packetLen = PacketBuilder::buildFillCommand(buf, amount, address, color);
  Z80Bus::sendBytes(buf, packetLen);

  BufferManager::freeToMark(mark);

}

__attribute__((optimize("-Os"))) 
void Z80Bus::sendWaitVBLCommand() {
//  uint8_t packetLen = PacketBuilder::buildWaitVBLCommand(BufferManager::packetBuffer);
//  Z80Bus::sendBytes(BufferManager::packetBuffer, packetLen);

	uint16_t mark = BufferManager::getMark();
	uint8_t* buf = BufferManager::allocate(E(WaitVBLPacket::PACKET_LEN));

  uint8_t packetLen = PacketBuilder::buildWaitVBLCommand(buf);
  Z80Bus::sendBytes(buf, packetLen);

  BufferManager::freeToMark(mark);

}


//TODO - check this HALT is really needed 

// sendStackCommand:
//   action = 0 -> Read SP
//   action = 1 -> Write SP
// The Z80-side routine finishes with a HALT. We wait for that HALT
// to confirm things have completed and release with NMI.
__attribute__((optimize("-Os"))) 
void Z80Bus::sendStackCommand(uint16_t addr, uint8_t action) {
  //uint8_t packetLen = PacketBuilder::buildStackCommand(BufferManager::packetBuffer, addr, action);
  //Z80Bus::sendBytes(BufferManager::packetBuffer, packetLen);

	uint16_t mark = BufferManager::getMark();
	uint8_t* buf = BufferManager::allocate(  E(StackPacket::PACKET_LEN) );

  uint8_t packetLen = PacketBuilder::buildStackCommand( buf, addr, action);
  Z80Bus::sendBytes( buf, packetLen);

  BufferManager::freeToMark(mark);

  Z80Bus::waitHalt_syncWithZ80();   // Wait for the Z80 to HALT
  Z80Bus::triggerZ80NMI();          // Clear the HALT by firing an NMI
}

//__attribute__((optimize("-Os"))) 
uint8_t Z80Bus::get_IO_Byte() {
  DDRD = 0x00;                    // All Nano data pins to input

  waitHalt_syncWithZ80();         // Wait for OUT cycle
  digitalWriteFast(Pin::OE_LATCH, LOW);  // Drive bus with latch data

  // Capture time for latch - spec says around 38ns.
  __asm__ __volatile__("nop\n\t"    // single nop is about 62ns @16MHz
                       "nop\n\t");  // playing it safe with 124 ns (NANO TIMINGS!)

  uint8_t byte = PIND;  

  digitalWriteFast(Pin::OE_LATCH, HIGH);   // IC to tri-state
  triggerZ80NMI();                  // Z80 handshake
  DDRD = 0xFF;                      // default - rest of code expects active bus driving
  return byte;
}

// Reminder ... optimised runs can do this with #OE pin set once at start and end ....
  //       for (uint16_t i = 0; i < ZX_SCREEN_BITMAP_SIZE + ZX_SCREEN_ATTR_SIZE; i++) {
  //         Z80Bus::waitHalt_syncWithZ80();
  //         uint8_t byte = PIND;  // Capture latched data
  //         Z80Bus::triggerZ80NMI();
  //         file.write(byte);
  //       }



__attribute__((optimize("-Os"))) 
uint8_t Z80Bus::getKeyboard() {

  uint8_t buf[E(ReceiveKeyboardPacket::PACKET_LEN)];
  // Send command packet to the Z80 asking it to send back a byte with the OUT instruction (TransmitKey) 
//  BufferManager::packetBuffer[(uint8_t)ReceiveKeyboardPacket::CMD_HIGH] = (uint8_t)(CommandRegistry::command_TransmitKey >> 8);
//  BufferManager::packetBuffer[(uint8_t)ReceiveKeyboardPacket::CMD_LOW] =  (uint8_t)(CommandRegistry::command_TransmitKey & 0xFF);
  //Z80Bus::sendBytes(BufferManager::packetBuffer, (uint8_t)ReceiveKeyboardPacket::PACKET_LEN);
  
  buf[E(ReceiveKeyboardPacket::CMD_HIGH)] = (uint8_t)(CommandRegistry::command_TransmitKey >> 8);
  buf[E(ReceiveKeyboardPacket::CMD_LOW)] =  (uint8_t)(CommandRegistry::command_TransmitKey & 0xFF);
  Z80Bus::sendBytes(buf, (uint8_t)ReceiveKeyboardPacket::PACKET_LEN);

  return get_IO_Byte();
}

//------------------------------------------------------------------------------------------
// This transfer has a simple look ahead to see if data can be RLE-compressed.  When a performance payoff 
// for using RLE is found, the data is encoded and the RLE-compressed block is sent to the Z80 using fast
// fill commands. We do things this way as the Z80 can use PUSH instructions to fill 2 bytes per
// instruction. When no compression is found raw bytes are sent instead.
// NOTE: The transfer assumes memory to fill is zeroed
//
__attribute__((optimize("-Ofast")))
void Z80Bus::rleOptimisedTransfer(uint16_t input_len, uint16_t addr, uint8_t* input, bool borderLoadingEffect) {
//   {
//  //  DEBUG FOR TESTING WITHOUT RLE (RLE JUST HELPS SPEEDUP TRANSFER)
//     uint8_t* pTransfer = &BufferManager::packetBuffer[0];
//     uint8_t packetLen = PacketBuilder::buildTransferCommand(pTransfer, addr, input_len);
//     Z80Bus::sendBytes(pTransfer, packetLen + input_len);
//     return ;
//   }
  constexpr uint16_t MAX_RUN_LENGTH = 255;
  constexpr uint16_t MAX_RAW_LENGTH = 255;
  constexpr uint8_t MIN_RUN_LENGTH = 3;  // about where RLE pays off over raw

  // Starts after the reserved packet header!
  //uint8_t* input = &BufferManager::packetBuffer[GLOBAL_MAX_PACKET_LEN];

  uint8_t mark = BufferManager::getMark();
  uint8_t* pHeader = BufferManager::allocate(E(FillPacket::PACKET_LEN));

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
    // Filling for zero's now to make this more general purpuse so we can do things like restore screens after leaving menus.
   ///////   if (value != 0) {               // ignore zero as we cleared memory at start
        // --- Send as SmallFillPacket ---
        // Using offset after input header and data so we don't touch the lower part of
        // packetBuffer as it still holds input to be processed
     //   uint8_t* pFill = spare; // &BufferManager::packetBuffer[COMMAND_PAYLOAD_SECTION_SIZE + GLOBAL_MAX_PACKET_LEN];

    //   + E(FillPacket::PACKET_LEN) 
        uint8_t packetLen = PacketBuilder::build_command_fill_mem_bytecount(pHeader, addr + run_len, run_len, value);
	      Z80Bus::sendBytes(pHeader, packetLen);
  ////////    }
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

    
      uint8_t* dataSrc = &input[raw_start];
      uint8_t headerLen;
      if (borderLoadingEffect) {
          headerLen = PacketBuilder::buildTransferCommand(pHeader, addr, raw_len);
      } else {
          headerLen = PacketBuilder::buildCopyCommand(pHeader, addr, raw_len);
      }

      Z80Bus::sendBytes(pHeader, headerLen);
      Z80Bus::sendBytes(dataSrc, raw_len);
      addr += raw_len;

      // // Packet header command is calculated to be placed just before the payload.
      // // The caller reading from file has added GLOBAL_MAX_PACKET_LEN to allow for any header type to be copied in.
      // // (this location is outside the range of the RLE buffer area)
      // if (borderLoadingEffect) {
      //   // Transfer gives a flashing border loader effect
      //   uint8_t* pTransfer = spare +raw_start - E(TransferPacket::PACKET_LEN) ;// &BufferManager::packetBuffer[GLOBAL_MAX_PACKET_LEN + ((int16_t)raw_start - E(TransferPacket::PACKET_LEN))];
      //   Z80Bus::sendBytes(pTransfer, PacketBuilder::buildTransferCommand(pTransfer, addr, raw_len) + raw_len);
      // } else {
      //   // Copy is plain transfer without flashing borders
      //   uint8_t* pTransfer = spare +raw_start - E(CopyPacket::PACKET_LEN);// = &BufferManager::packetBuffer[GLOBAL_MAX_PACKET_LEN + ((int16_t)raw_start - E(CopyPacket::PACKET_LEN))];
      //   Z80Bus::sendBytes(pTransfer, PacketBuilder::buildCopyCommand(pTransfer, addr, raw_len) + raw_len);
      // }
      // addr += raw_len;
    }
  }

  BufferManager::freeToMark(mark);

}


// Note: Since snapshots (*.SNA files) include the screen we can reuse this for things like loading *.SCR files
__attribute__((optimize("-Os"))) 
void Z80Bus::transferSnaData(FatFile* pFile, bool borderLoadingEffect) {

    uint16_t mark = BufferManager::getMark();
    uint8_t* Buf = BufferManager::allocate(128);  

    

  uint16_t currentAddress = ZX_SCREEN_ADDRESS_START;
  // Transfer data to Spectrum RAM
  while (pFile->available()) {
   // uint16_t bytesRead = pFile->read(&BufferManager::packetBuffer[GLOBAL_MAX_PACKET_LEN], COMMAND_PAYLOAD_SECTION_SIZE);

    uint16_t bytesRead = pFile->read(Buf, 128);

    rleOptimisedTransfer(bytesRead, currentAddress,Buf, borderLoadingEffect);
    currentAddress += bytesRead;
  }

  BufferManager::freeToMark(mark);
}


/* executeSnapshot:
 * See Z80 code around L16D4. It will idle-loop to allow bankswitching into the 
 * stock ROM to continue execution into RAM to jump back into game code. 
 * (Previously this needed an extra messy HALT and a precise wait for the NMI to complete)
 */
 __attribute__((optimize("-Os"))) 
void Z80Bus::executeSnapshot(uint8_t* snaHeaderPacket) {

  // For a short while we will re-enable the Spectrum's 50 Hz maskable interrupt (IM 1).
  // This provides a safety gap before the next interrupt so the game's ISR won't corrupt the stack
  // when the launch code is in the last stages of resuming the game. The Arduino needs to synchronize via NMI 
  // to send data and that also uses the Z80's stack.
  // NOTE: On the Z80 side - the SNA ROM does not execute 'EI' when exiting the maskable interrupt
  // (IM 1) at vector 0x0038. This means IM 1 interrupts remain disabled when the ISR returns.

  sendWaitVBLCommand();    // Ask Speccy to start 50Hz interrupt and halt itself.
  waitHalt_syncWithZ80();  // Halt line has gone it's active low (Arduino sync point)
  hasZ80Resumed();         // Wait for HALT to clear (50Hz interrupt occurred, Z80 running again)

  //PacketBuilder::buildExecuteCommand(snaHeaderPacket) ; //BufferManager::head27_Execute);
  sendSnaHeader(snaHeaderPacket); // BufferManager::head27_Execute);

  delay(1);  // allow 'L16D4' routine in SNA rom time to reach idle loop
  Z80Bus::setStockRom();
}

__attribute__((optimize("-Os"))) 
 void Z80Bus::waitHalt_syncWithZ80() {
   // Wait until the Z80 enters the HALT state (active low).
   while (digitalReadFast(Pin::Z80_HALT) != 0) {};  
 }

__attribute__((optimize("-Os"))) 
void Z80Bus::triggerZ80NMI() {
  digitalWriteFast(Pin::Z80_NMI, LOW);
  // delayMicroseconds(1);     // other Arduino platforms may need a pause
  digitalWriteFast(Pin::Z80_NMI, HIGH);
  hasZ80Resumed();
}

__attribute__((optimize("-Os"))) 
void Z80Bus::hasZ80Resumed(){
  // wait until HALT goes HIGH (Z80 resumes)
   while (digitalReadFast(Pin::Z80_HALT) == 0) {};  
}

