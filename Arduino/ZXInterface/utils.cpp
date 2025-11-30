#include "utils.h"
#include "Z80Bus.h"

#include "draw.h"
#include "SdCardSupport.h"
#include "BufferManager.h"
#include "CommandRegistry.h"
#include "z80bus.h"

#include "PacketBuilder.h"


static uint8_t REG_D;
static uint8_t REG_E;
static uint8_t REG_B;
static uint8_t REG_C;
static uint8_t REG_H;
static uint8_t REG_L;
static uint8_t REG_IFF2;

__attribute__((optimize("-Os")))
void Utils::highlightSelection(uint16_t currentFileIndex, uint16_t startFileIndex, uint16_t& oldHighlightAddress) {
  const uint16_t fillAddr = ZX_SCREEN_ATTR_ADDRESS_START + ((currentFileIndex - startFileIndex) * ZX_SCREEN_WIDTH_BYTES);
  if (oldHighlightAddress != fillAddr) {  // Clear old highlight if it's different
    Z80Bus::sendFillCommand(oldHighlightAddress, ZX_SCREEN_WIDTH_BYTES, COL::BLACK_WHITE);
    oldHighlightAddress = fillAddr;
  }
  Z80Bus::sendFillCommand(fillAddr, ZX_SCREEN_WIDTH_BYTES, COL::CYAN_BLACK);
}

 __attribute__((optimize("-Os")))
void Utils::clearScreen(uint8_t col) {
   Z80Bus::sendFillCommand( ZX_SCREEN_ATTR_ADDRESS_START, ZX_SCREEN_ATTR_SIZE, col);
   Z80Bus::sendFillCommand(ZX_SCREEN_ADDRESS_START, ZX_SCREEN_BITMAP_SIZE, 0);
}

 __attribute__((optimize("-Os")))
void Utils::frameDelay(unsigned long start) {
  const unsigned long timeSpent = millis() - start;
  if (timeSpent < MAX_BUTTON_READ_MILLISECONDS) {
    delay(MAX_BUTTON_READ_MILLISECONDS - timeSpent);  // aiming for 50 FPS
  }
}

// Kempston Joystick Support:
// 1. The Arduino polls the joystick at 50 FPS via a 74HC165 shift register.
// 2. When the Spectrum performs an IN (C) instruction (e.g. LD C,$1F; IN D,(C)), the Arduino drives the Z80 data bus.
//    A 74HC32 combines the /IORQ, /RD, and /A7 signals to detect when the joystick port is being accessed.
//    A 74HC245D transceiver connects the Arduino to the Z80 to send joystick data, and it is tri-stated when not in use.
__attribute__((optimize("-Ofast")))
uint8_t Utils::readJoystick() {
  PORTB &= ~(1 << PB2);     // Latch LOW: Take a snapshot of all joystick inputs
  delayMicroseconds(1);     // Small delay to let the latch complete
  PORTB |= (1 << PB2);      // Latch HIGH: Enable shifting and make the first bit available on the data pin

  uint8_t data = 0;
  for (uint8_t i = 0; i < 8; i++) {
    data <<= 1;      
    PORTC &= ~(1 << PC2);     // Clock LOW
    if (PINB & (1 << PB1)) {  // serial data pin
      data |= 1;
    }
    PORTC |= (1 << PC2);      // Clock HIGH (next bit)
  }

  // The remaining 3 bits are FireB, Button Select (PCB button) & unused.
  return data;  // read bits are "uSfFUDLR"  
}

__attribute__((optimize("-Os"))) 
void Utils::setupJoystick() {
  // Setup pins for "74HC165" shift register
  pinModeFast(Pin::ShiftRegDataPin, INPUT);
  pinModeFast(Pin::ShiftRegLatchPin, OUTPUT);
  pinModeFast(Pin::ShiftRegClockPin, OUTPUT);
}

void Utils::storeZ80StateExitToMenu() {
  // Wait for the game's own 50 Hz interrupt to fire - this avoids pushing onto a stack
  // the game may already be using heavily.
  uint32_t start = millis();
  while (digitalReadFast(Pin::Z80_HALT) != 0) {
    if ((millis() - start) >= 80) {    // Allow 80ms timeout
      break;  // give up - looks like interrupts are disabled
    }
  }
  Z80Bus::waitForZ80Resume();  // Currently still in the stock (modified) ROM.
  
  // Triggering NMI gives us a path back to the SNA ROM
  Z80Bus::triggerZ80NMI();              // NMI - jmp to modified ROM method 'L0066:'
  delay(1);                             // Allow time to reach the idle loop - NEEDED
  // Above to swap ROM back to stock giving us a new run path back to menus
  digitalWriteFast(Pin::ROM_HALF, LOW); // swapping from stock to SNA ROM 
  REG_IFF2 = Z80Bus::get_IO_Byte();     // get_IO_Byte: Z80 will HALT and OUT
  REG_D = Z80Bus::get_IO_Byte();
  REG_E = Z80Bus::get_IO_Byte();
  REG_B = Z80Bus::get_IO_Byte();
  REG_C = Z80Bus::get_IO_Byte();
  REG_H = Z80Bus::get_IO_Byte();
  REG_L = Z80Bus::get_IO_Byte();
}

void Utils::restoreZ80StateBackToGame() {
  uint8_t addr0x04AA[] = { 0x04, 0xAA };  // 0x04AA:  Z80 .restoreInGameState -> idle loop
  Z80Bus::sendBytes(addr0x04AA, sizeof(addr0x04AA));
  Z80Bus::sendBytes(&REG_D, 1); // sendBytes: Z80 will IN and HALT
  Z80Bus::sendBytes(&REG_E, 1);
  Z80Bus::sendBytes(&REG_B, 1);
  Z80Bus::sendBytes(&REG_C, 1);
  Z80Bus::sendBytes(&REG_H, 1);
  Z80Bus::sendBytes(&REG_L, 1);
  Z80Bus::sendBytes(&REG_IFF2, 1);  // send last - important for restore order
  delay(1); // allow Z80 time to reach idle loop - NEEDED
  digitalWriteFast(Pin::ROM_HALF, HIGH);  // STOCK 48K ROM - escapes out of idle loop
}

__attribute__((optimize("-Os")))
void Utils::waitForUserExit() {

  // ------- fudge  ----------------
  // TO-DO - restore the real values in screen memory from game launch using 3 bytes of screen memory
  // + 2nd time into the pause menu (same game) would not need this again.
  storeZ80StateExitToMenu();
  Z80Bus::sendFillCommand(0x4000, 3, 0); //for now, just clear borrowed screen memory!
  restoreZ80StateBackToGame();
  // -------------------------------



  // storeZ80StateExitToMenu();

  //  TEST CODE TO SEND SCREEN DATA TO SD CARD

  //     // Ask Speccy to send its screen data
  //     uint8_t len = PacketBuilder::build_Request_CommandSendData(BufferManager::packetBuffer,
  //                                                                ZX_SCREEN_BITMAP_SIZE + ZX_SCREEN_ATTR_SIZE, 0x4000);
  //     Z80Bus::sendBytes(BufferManager::packetBuffer, len);

  //     FatFile &file = SdCardSupport::getFile();
  //     FatFile &root = SdCardSupport::getRoot();
  //     if (root.isOpen()) { root.close(); }
  //     if (file.isOpen()) { file.close(); }
  //     if (root.open("/")) {

  //       file.open("scratch16384.SCR", O_CREAT | O_WRONLY);

  //       // -------------------------------------------------------------
  //       // Get ready to read from Latch IC (74HC574) - set Nano data pins to read
  //       DDRD = 0x00;   // make them inputs
  //       PORTD = 0x00;  // no pull-ups
  //       // Enable Latch for reading - we can keep it on for this section
  //       digitalWriteFast(PIN_A5, LOW);
  //       // Timing delay: 74HC574 needs around 38ns (5V) for output enable time.
  //       // The Nano can execute the next instruction before latch outputs are ready!
  //       __asm__ __volatile__("nop\n\t"    // single nop is about 62ns @16MHz
  //                            "nop\n\t");  // playing it safe with 124 ns (!!TIMINGS FOR NANO
  //       // -------------------------------------------------------------

  //       for (uint16_t i = 0; i < ZX_SCREEN_BITMAP_SIZE + ZX_SCREEN_ATTR_SIZE; i++) {
  //         Z80Bus::waitHalt_syncWithZ80();
  //         uint8_t byte = PIND;  // Capture latched data
  //         Z80Bus::triggerZ80NMI();
  //         file.write(byte);
  //       }
  //       file.flush();
  //       file.close();

  //       // -------------------------------------------------------------
  //       // Done with latch back disable and put Arduino back to sending mode
  //       digitalWriteFast(PIN_A5, HIGH);  // Disable #OE
  //       DDRD = 0xFF;                     // Set all PORTD pins as outputs
  //      // PORTD = 0x00;                    // outputs start LOW
  //       // -------------------------------------------------------------
  //     }


 // restoreZ80StateBackToGame();



  // Dummy flush read - SNA ROM also uses port 0x1F (KEMPSTONE PORT) for send/receiving
  readJoystick();

  uint8_t b;
  while(true) {
    b = Utils::readJoystick();

    // Ready PORTD — this (and the cart hardware) behaves like a Kempston joystick interface.
    // Note: We will let this joystick loop run as fast as possible, since a real 
    // Kempston interface returns results instantly.
    PORTD = b & JOYSTICK_MASK;

    if (b & JOYSTICK_SELECT) {
      storeZ80StateExitToMenu();
      Utils::clearScreen(0);
      Draw::text_P(80, 90, F("IN-GAME PAUSE MENU"));
      Draw::text_P(80, 120, F("TO-DO"));
      delay(2000);
      restoreZ80StateBackToGame();
      break;
    }
  }
  while ((Utils::readJoystick() & JOYSTICK_SELECT)) { }  // wait for button let go

}

// ---------------------
// .TXT FILE - Support
// ---------------------
__attribute__((optimize("-Ofast")))
uint16_t Utils::readLineTxt(FatFile* f, char* buf, uint16_t maxChars) {
  uint16_t i = 0;
  uint8_t c;
  while (i < maxChars) {
    if (f->read(&c, 1) <= 0) break;      // EOF
    if (c >= 32 && c <= 126) {
      if (buf) {
        buf[i] = c;
      }
      i++;
      continue;
    }
    if (c == 0) continue;
    if (c == '\n') break;
    if (c == '\r') {
      if (f->peek() == '\n') f->read();
      break;
    }
  }
  return i;
}







// =============================


// ======================
// ARCHIVED CODE SECTION:
// ======================

// The 1-bit HALT-pulse protocol has been replaced as I've added additional supporting hardware
// to the curcuit design allowing the OUT instruction to be used instead.
//
// The HALT-pulse method is actually quite impressive - it's surprising how well it works.
// It’s probably a bulletproof poor mans out method, since the pulse timing
// makes it far less likely to be affected by interference - as HALT stays active
// long enough to ride through most real-world noise.

// // get16bitPulseValue: Used to get the command functions (addresses) from the speccy.
// // The speccy broadcasts all the functions at power up.
// // uint16_t Utils::get16bitPulseValue() {
// //   constexpr uint16_t BIT_TIMEOUT_US  = 70; //120+20;
// //   uint16_t value = 0;
// //   for (uint8_t i = 0; i < 16; i++) { // 16 bits, 2 bytes
// //     uint8_t pulseCount = 0;
// //     uint32_t lastPulseTime = 0;
// //     while (1) {
// //       // Service current HALT if active
// //       if (digitalReadFast(Pin::Z80_HALT) == 0) {  // Waits for Z80 HALT line to go HIGH   
// //         // Pulse the Z80's /NMI line: LOW -> HIGH to un-halt the CPU.
// //         // digitalWriteFast(Pin::Z80_NMI, LOW);
// //         // digitalWriteFast(Pin::Z80_NMI, HIGH);
// //         Z80Bus::triggerZ80NMI();
// //         pulseCount++;
// //         lastPulseTime = micros();  // reset timer, allow another pulse to be sampled
// //       }
// //       // Detect end marker delay at end of each single bit 
// //       if ((pulseCount > 0) && ((micros() - lastPulseTime) > BIT_TIMEOUT_US )) {
// //         break;
// //       }
// //     }
// //     if (pulseCount == 2) {  // collect set bit
// //       value += 1 << (15 - i);
// //     }
// //   }
// //   return value;
// // }

// // uint8_t Utils::get8bitPulseValue_NO_LONGER_USED() {
// //   constexpr uint16_t BIT_TIMEOUT_US  = 25; // 70; //120+20;
// //   uint8_t value = 0;
// //   for (uint8_t i = 0; i < 8; i++) { // 8 bits
// //     uint8_t pulseCount = 0;
// //     uint32_t lastPulseTime = 0;
// //     while (1) {
// //       // Service current HALT if active
// //       if (digitalReadFast(Pin::Z80_HALT) == 0) {  // Waits for Z80 HALT line to go HIGH   
// //         // Pulse the Z80's /NMI line: LOW -> HIGH to un-halt the CPU.
// // //        digitalWriteFast(Pin::Z80_NMI, LOW);
// // //        digitalWriteFast(Pin::Z80_NMI, HIGH);
// //         Z80Bus::triggerZ80NMI();
// //         pulseCount++;
// //         lastPulseTime = micros();  // reset timer, allow another pulse to be sampled
// //       }
// //       // Detect end marker delay at end of each single bit 
// //       if ((pulseCount > 0) && ((micros() - lastPulseTime) > BIT_TIMEOUT_US )) {
// //         break;
// //       }
// //     }
// //     if (pulseCount == 2) {  // collect set bit
// //       value += 1 << (7 - i);
// //     }
// //   }
// //   return value;
// // }

// ========================

// Not used ...
// 
// __attribute__((optimize("-Ofast"))) 
// void joinBits(byte* output, uint8_t input, uint16_t bitWidth, uint16_t bitPosition) {
//   uint16_t byteIndex = bitPosition >> 3;   // /8
//   uint8_t  bitIndex  = bitPosition & 7;    // %8
//   // Using a WORD to allow for a boundary crossing
//   uint16_t aligned = (uint16_t)input << (8 - bitWidth);
//   output[byteIndex] |= aligned >> bitIndex;
//   if (bitIndex + bitWidth > 8) {  // spans across two bytes
//     output[byteIndex + 1] |= aligned << (8 - bitIndex);
//   }
// }
// 
// __attribute__((optimize("-Ofast"))) 
// byte reverseBits(byte data) {
//   data = (data & 0xF0) >> 4 | (data & 0x0F) << 4;
//   data = (data & 0xCC) >> 2 | (data & 0x33) << 2;
//   data = (data & 0xAA) >> 1 | (data & 0x55) << 1;
//   return data;
// }
// 
// __attribute__((optimize("-Ofast"))) 
// void swap(byte &a, byte &b) {
//   byte temp = a;
//   a = b;
//   b = temp;
// }

//=============================

//
// NOTES:
// 1,000,000 microseconds to a second
// T-States: 4 + 4 + 12 = 20 ish (NOP,dec,jr)
// 1 T-state on a ZX Spectrum 48K is approximately 0.2857 microseconds.
// 20 T-States / 0.285714 = 70 t-states

