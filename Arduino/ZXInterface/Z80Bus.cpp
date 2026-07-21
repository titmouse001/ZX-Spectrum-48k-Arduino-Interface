#include <stdint.h>
#include "digitalWriteFast.h"
#include "pin.h"
#include "Z80Bus.h"
#include "PacketTypes.h"
#include "BufferManager.h"
#include "Utils.h"
#include "Constants.h"

//-------------------------------------------------
// Z80 Data Transfer and Control Routines
//-------------------------------------------------

void Z80Bus::setupPins() {
 
  pinModeFast(Pin::ROM_HALF, OUTPUT);  // set ROM_HALF first
  digitalWriteFast(Pin::ROM_HALF, LOW);  //  Switch over to Sna ROM.
 
  pinModeFast(Pin::OE_LATCH, OUTPUT);
  digitalWriteFast(Pin::OE_LATCH,HIGH);   //74HC574PW , #OE_LATCH

  pinModeFast(Pin::Z80_HALT, INPUT);
  pinModeFast(Pin::Z80_NMI, OUTPUT);
  pinModeFast(Pin::Z80_REST, OUTPUT);

  // pinModeFast(Pin::Z80_D0Pin, OUTPUT);
  // pinModeFast(Pin::Z80_D1Pin, OUTPUT);
  // pinModeFast(Pin::Z80_D2Pin, OUTPUT);
  // pinModeFast(Pin::Z80_D3Pin, OUTPUT);
  // pinModeFast(Pin::Z80_D4Pin, OUTPUT);
  // pinModeFast(Pin::Z80_D5Pin, OUTPUT);
  // pinModeFast(Pin::Z80_D6Pin, OUTPUT);
  // pinModeFast(Pin::Z80_D7Pin, OUTPUT);
  DDRD = 0xFF;                     // Set all PORTD pins as outputs

  // setup /NMI - Z80 /NMI, used to trigger a 'HALT' so the Z80 can be released and resume
  digitalWriteFast(Pin::Z80_NMI, HIGH);  // put into a default state
}

void Z80Bus::resetZ80() {
  digitalWriteFast(Pin::Z80_REST, LOW);  // begin reset
  Utils::delay16(Z80_RESET_TIME); 
  digitalWriteFast(Pin::Z80_REST, HIGH);  // release RESET (Z80 restarts)
  Utils::delay16(1);
}

void Z80Bus::setSnaRom() {
  digitalWriteFast(Pin::ROM_HALF, LOW);  // LOW = Snaploader Custom ROM
}

void Z80Bus::setStockRom() {
  digitalWriteFast(Pin::ROM_HALF, HIGH);  // HIGH = Stock ROM
}

__attribute__((optimize("-Ofast")))
void Z80Bus::sendBytes(uint8_t* data, uint16_t size) {
  // cli();  // maybe save a tiny bit, guess it depends on number on interrupts during this send.
  for (uint16_t i = 0; i < size; i++) {
    waitHalt_syncWithZ80();
    PORTD = data[i];  // data on lines ready for Z80s 'IN'
    triggerZ80NMI();
  }
  //  sei();
}

__attribute__((optimize("-Ofast")))
void Z80Bus::sendBytes8(uint8_t* data, uint8_t size) {
  for (uint8_t i=0; i < size; i++) {
    waitHalt_syncWithZ80();
    PORTD = data[i]; 
    triggerZ80NMI();
  }
}

void Z80Bus::sendByte(uint8_t* byte) {
  waitHalt_syncWithZ80();
  PORTD = *byte; 
  triggerZ80NMI();
}

void Z80Bus::sendSnaHeader(uint8_t* header) {
  RestoreGameAndExecute pkt;
  sendBytes((uint8_t*)&pkt, sizeof(RestoreGameAndExecute));
  // Execute Command expects registers to follow in this order
  sendBytes(&header[SNA_I],  1 + 2 + 2 + 2 + 2);  // I,HL',DE',BC',AF'
  sendBytes(&header[SNA_IY_LOW], 2 + 2 + 1 + 1);  // IY,IX,IFF2,R
  sendBytes(&header[SNA_SP_LOW], 2);              // The rest aren't in SNA header sequence...
  sendBytes(&header[SNA_HL_LOW], 2);
  sendBytes(&header[SNA_IM_MODE], 1);
  sendBytes(&header[SNA_BORDER_COLOUR], 1);
  sendBytes(&header[SNA_DE_LOW], 2);
  sendBytes(&header[SNA_BC_LOW], 2);
  sendBytes(&header[SNA_AF_LOW], 2);
}

void Z80Bus::sendFillCommand(uint16_t address, uint16_t amount, uint8_t color) {
  FillPacket pkt(address, amount, color);
  Z80Bus::sendBytes((uint8_t*)&pkt, sizeof(FillPacket));
}

void Z80Bus::sendWaitVBLCommand() {
  WaitVBLPacket pkt;
  Z80Bus::sendBytes((uint8_t*)&pkt, sizeof(WaitVBLPacket));
}
 
void Z80Bus::setStackCommand(uint16_t addr) {
  StackPacket pkt(addr);
  Z80Bus::sendBytes( (uint8_t*) &pkt, sizeof(StackPacket) );
  // The Z80-side routine finishes with a HALT. We wait for that HALT
  // to confirm things have completed and release with NMI.
  Z80Bus::waitHalt_syncWithZ80();   // Wait for the Z80 to HALT
  Z80Bus::triggerZ80NMI();          // Clear the HALT by firing an NMI
}

// ASM> OUT ($1F), A    		  ; Game cart latches value (latch ic: 74HC574PW)
// ASM> halt	

uint8_t Z80Bus::get_IO_Byte() {
  DDRD = 0x00;                    // Nano data pins to input

  waitHalt_syncWithZ80();         // Wait for OUT cycle
  digitalWriteFast(Pin::OE_LATCH, LOW);  // Drive bus with latch data

  // Capture time for latch - spec says around 38ns (nanoseconds)
  __asm__ __volatile__("nop\n\t"    // single nop is about 62ns @16MHz
                       "nop\n\t");  // playing it safe with 124 ns (NANO TIMINGS!)

  uint8_t byte = PIND;  

  digitalWriteFast(Pin::OE_LATCH, HIGH);   // IC to tri-state
  triggerZ80NMI();                  // Z80 handshake
  DDRD = 0xFF;                      // default - rest of code expects active bus driving
  return byte;
}
  
uint8_t Z80Bus::getKeyboard() {
    ReceiveKeyboardPacket pkt;
    Z80Bus::sendBytes(reinterpret_cast<uint8_t*>(&pkt), sizeof(pkt));
    return get_IO_Byte();
}
  
void Z80Bus::Z80_NOP() {
    NOP_Packet pkt;
    Z80Bus::sendBytes(reinterpret_cast<uint8_t*>(&pkt), sizeof(NOP_Packet));
}

//#include "Draw.h"
void Z80Bus::transferSnaData(FatFile* pFile, bool borderLoadingEffect) {
   //uint32_t startTime = millis();

  const uint8_t cmd = borderLoadingEffect ? CMD_Transfer : CMD_Copy;
  const uint16_t mark = BufferManager::getMark();
  // transfering SNAPS gets it own buffer value, it can't use project constants
  // due to RLE and max limit of 255
  constexpr uint8_t BUFFER_SIZE_U8 = 255;
  uint8_t* Buf = BufferManager::allocate(BUFFER_SIZE_U8);
  uint16_t currentAddress = ZX_SCREEN_ADDRESS_START;
  // Transfer data to Spectrum RAM
  while (pFile->available()) {
    uint16_t bytesRead = pFile->read(Buf, BUFFER_SIZE_U8);
    rleOptimisedTransfer(bytesRead, currentAddress, Buf, cmd);
    currentAddress += bytesRead;
  }
  BufferManager::freeToMark(mark);

  // Utils::clearScreen(COL::CYAN_BLACK);
  // char _c[10];
  // uint32_t endTime = millis();
  // uint16_t duration = endTime - startTime;
  // itoa(duration, _c, 10);
  // Draw::text(8, 8, _c);
  // itoa(duration / 1000, _c, 10);
  // Draw::text(8, 8 + 16, _c);
  // delay(2000);
}

//------------------------------------------------------------------------------------------
/* rleOptimisedTransfer:
 * Transfers data using look-ahead RLE. If beneficial, sends RLE blocks via fast Z80 fill
 * commands (PUSH fills 2 bytes/instruction); otherwise sends raw bytes.
 */

__attribute__((optimize("-Ofast"))) 
void Z80Bus::rleOptimisedTransfer(uint8_t input_len, uint16_t addr, uint8_t* input, const uint8_t cmd) {

#if 0
      {
        TransferPacket pkt(addr, input_len);
        Z80Bus::sendBytes8((uint8_t*)&pkt, sizeof(TransferPacket) );
        Z80Bus::sendBytes8(input, input_len);
        return;
        // Looks like the RLE is ONLY JUST A BIT quicker than the plain copy
        //   Exolon: noRLE=1115, withRLE=1036 1027
        //  Turtles: noRLE=1115, withRLE=1113 
        //   Zynaps: noRLE=1115, withRLE=1119
        //    Dizzy: noRLE=1115, withRLE=1170
        //  FireFly: noRLE=1115, withRLE=1085
        //      Zub: noRLE=1115, withRLE=1039
        //    Cobra: noRLE=1115, withRLE=1087
        // Commando: noRLE=1115, withRLE=1078
      }
#endif

  uint8_t remaining = input_len;
  uint8_t* p = input;

  while (remaining > 0) {
    // RLE Check - kind of worth doing!
    if (remaining >= 6) {
      // 5 is an ok payoff Threshold  - setup cost of Z80 fill call
      if (*p == *(p + 5) && *p == *(p + 1) && *p == *(p + 2) &&
          *p == *(p + 3) && *p == *(p + 4)) {
        uint8_t run = 6;
        uint8_t* scan = p + 6;
        remaining -= 6;  // already checked above

        // Count the run (max 255)
        while (remaining > 0 && *scan == *p) {
          ++scan;
          ++run;
          --remaining;
        }

        addr += run;  // (fill is in reverse)
        Fill8Packet pkt(addr, run, *p);
        Z80Bus::sendBytes8((uint8_t*)&pkt, sizeof(pkt));
        p = scan;
        continue;
      }
    }

    // RAW Block Collection
    uint8_t* raw_start = p;
    uint8_t rawLen = 0;
    // Collect RAW bytes until we see an RLE run or run out of data
    while (remaining > 0) {
      if (remaining >= 6 && *p == *(p + 5) && *p == *(p + 1) &&
          *p == *(p + 2) && *p == *(p + 3) && *p == *(p + 4)) {
        break;  // Found an RLE run, stop collecting raw
      }
      ++p;
      ++rawLen;
      --remaining;
    }

    if (rawLen > 0) {
      MemoryPacket pkt(cmd , addr, rawLen);
      Z80Bus::sendBytes8((uint8_t*)&pkt, sizeof(pkt));
      Z80Bus::sendBytes8(raw_start, rawLen);
      addr += rawLen;
    }
  }
}

/* executeSnapshot:
 * See Z80 code around L16D4. It will idle-loop to allow bankswitching into the 
 * stock ROM to continue execution into RAM to jump back into game code. 
 * (Previously this needed an extra messy HALT and a precise wait for the NMI to complete)
 */
void Z80Bus::executeSnapshot(uint8_t* snaHeaderPacket) {

  // --------------------------------------------------------------------------------------------
  // Create a safety gap around the Spectrum's 50Hz maskable interrupt (IM 1) at vector 0x0038.
  // This prevents the game's ISR from corrupting the stack while the launch code is in the 
  // final stages of resuming. The Arduino synchronizes with the Z80 via NMI.
  //
  // NOTE: The SNA ROM does not execute 'EI' upon exiting the IM 1 interrupt at 0x0038, 
  // leaving maskable interrupts disabled after the ISR returns.
  // --------------------------------------------------------------------------------------------
  sendWaitVBLCommand();    // Trigger 50Hz interrupt and force Speccy into HALT
  waitHalt_syncWithZ80();  // Synchronize execution until the HALT state is confirmed
  hasZ80Resumed();         // Wait for the interrupt to clear and Z80 to resume
  // ---------------------------------------------------------------------------------------------

  sendSnaHeader(snaHeaderPacket); // BufferManager::head27_Execute);

  Utils::delay16(1);  // allow 'L16D4' routine in SNA rom time to reach idle loop
  Z80Bus::setStockRom();
}
 
void Z80Bus::waitHalt_syncWithZ80() {
  // Wait until the Z80 enters the HALT state (active low).
  while (digitalReadFast(Pin::Z80_HALT) != 0) {};  
}

void Z80Bus::triggerZ80NMI() {
  digitalWriteFast(Pin::Z80_NMI, LOW);
  // delayMicroseconds(1);     // other Arduino platforms may need a pause
  digitalWriteFast(Pin::Z80_NMI, HIGH);
  hasZ80Resumed();
}
 
void Z80Bus::hasZ80Resumed() {
  // wait until HALT goes HIGH (Z80 resumes)
  while (digitalReadFast(Pin::Z80_HALT) == 0) {};
}
