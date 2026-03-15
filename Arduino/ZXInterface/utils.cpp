#include "utils.h"
#include "Z80Bus.h"

#include "draw.h"
#include "SdCardSupport.h"
#include "BufferManager.h"
#include "CommandRegistry.h"
#include "z80bus.h"

#include "PacketBuilder.h"
#include "menu.h"

static uint8_t REG_A;
static uint8_t REG_F;
static uint8_t REG_D;
static uint8_t REG_E;
static uint8_t REG_B;
static uint8_t REG_C;
static uint8_t REG_H;
static uint8_t REG_L;
static uint8_t REG_IFF2;
static uint8_t REG_IXL;
static uint8_t REG_IXH;

static const char* SCRATCH_FILE = "scratch16384.SCR";

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
  digitalWriteFast(Pin::ShiftRegLatchPin, LOW);     // snapshot joystick
  delayMicroseconds(1);                             // let the latch IC complete
  digitalWriteFast(Pin::ShiftRegLatchPin, HIGH);    // Enable shifting + 1st bit now available

  uint8_t data = 0;
  for (uint8_t i = 0; i < 8; i++) {
    data <<= 1;      
    digitalWriteFast(Pin::ShiftRegClockPin, LOW);  
    if (digitalReadFast(Pin::ShiftRegDataPin)) {    // get serial data
      data |= 1;
    }
    digitalWriteFast(Pin::ShiftRegClockPin, HIGH);   // next bit
  }
  return data;  // bits "XsfFUDLR" 
  // bit 0 to 7: Right,Left,Down,Up,Fire1,Fire2, Select (PCB button) and last bit is not used.
}

__attribute__((optimize("-Os"))) 
void Utils::setupJoystick() {
  // Setup pins for "74HC165" shift register
  pinModeFast(Pin::ShiftRegDataPin, INPUT);
  pinModeFast(Pin::ShiftRegLatchPin, OUTPUT);
  pinModeFast(Pin::ShiftRegClockPin, OUTPUT);
}

__attribute__((optimize("-Os"))) 
void Utils::storeZ80States() {
  
  delay(1);             // Allow NMI time to do it's idle loop (will be way more than needed)
  Z80Bus::setSnaRom();  // swap out stock ROM 
  delay(1);             // wait for Z80 to hit SNA ROM's '.IngameHook'

  // note: for each get_IO_Byte the Z80 will 'OUT' then 'HALT'
  REG_A = Z80Bus::get_IO_Byte();

  REG_B = Z80Bus::get_IO_Byte();
  REG_C = Z80Bus::get_IO_Byte();

  REG_F = Z80Bus::get_IO_Byte();

  REG_IFF2 = Z80Bus::get_IO_Byte();     

  REG_D = Z80Bus::get_IO_Byte();
  REG_E = Z80Bus::get_IO_Byte();

  REG_H = Z80Bus::get_IO_Byte();
  REG_L = Z80Bus::get_IO_Byte();

  REG_IXH = Z80Bus::get_IO_Byte();
  REG_IXL = Z80Bus::get_IO_Byte();
}

void Utils::restoreZ80States() {  
  
  // 0x04AA:  Z80 code jumps to '.restoreInGameState' then idle loops. This allow time 
  // for the ROM to swap back to the stock ROM and start the restore process at 0x04AA.
  uint8_t addr0x04AA[] = { 0x04, 0xAA }; 
  Z80Bus::sendBytes(addr0x04AA, sizeof(addr0x04AA));

  Z80Bus::sendBytes(&REG_D, 1); 
  Z80Bus::sendBytes(&REG_E, 1);

  Z80Bus::sendBytes(&REG_B, 1);
  Z80Bus::sendBytes(&REG_C, 1);

  Z80Bus::sendBytes(&REG_H, 1);
  Z80Bus::sendBytes(&REG_L, 1);

  Z80Bus::sendBytes(&REG_IXH, 1);
  Z80Bus::sendBytes(&REG_IXL, 1);

  Z80Bus::sendBytes(&REG_IFF2, 1); 

  Z80Bus::sendBytes(&REG_F, 1); 
  Z80Bus::sendBytes(&REG_A, 1); 

  delay(1); // allow Z80 time to reach the next idle loop for the stock rom to take control.
  Z80Bus::setStockRom();  // Stock rom escapes out of idle loop via it's NOP's
  delay(1); // let the stock rom catch up
}

__attribute__((optimize("-Os"))) 
void Utils::saveScreen(const char* filename) {

  // Speccy to send its screen data
  uint8_t len = PacketBuilder::build_Request_CommandSendData(BufferManager::packetBuffer,
                                                             ZX_SCREEN_BITMAP_SIZE + ZX_SCREEN_ATTR_SIZE, 0x4000);
  Z80Bus::sendBytes(BufferManager::packetBuffer, len);

// TODO - ******* replace me with optimised code please ********
// ...... This version of the code is for testing the get_IO_Byte by hammering it with this task.
// .......By saving a screen shot we can easly verify things work OK, is free testing!
  FatFile &file = SdCardSupport::getFile();
  if (file.isOpen()) { file.close(); }
  FatFile &root = SdCardSupport::getRoot();
  if (root.isOpen()) { root.close(); }

  if (root.open("/")) {
    file.open(filename, O_CREAT | O_WRONLY);
    for (uint16_t i = 0; i < ZX_SCREEN_BITMAP_SIZE + ZX_SCREEN_ATTR_SIZE; i++) {
      file.write(Z80Bus::get_IO_Byte());
    }
    file.close();
  }

  // if (root.open("/")) {
  //   file.open(filename, O_CREAT | O_WRONLY);
  //   DDRD = 0x00;   // make them inputs
  //   digitalWriteFast(PIN_A5, LOW);  //Enable Latch for reading 
  //   __asm__ __volatile__("nop; nop");

  //   for (uint16_t i = 0; i < ZX_SCREEN_BITMAP_SIZE + ZX_SCREEN_ATTR_SIZE; i++) {
  //     Z80Bus::waitHalt_syncWithZ80();
  //     uint8_t byte = PIND;  // Capture latched data
  //     Z80Bus::triggerZ80NMI();
  //     file.write(byte);
  //   }
  //   file.close();

  //   digitalWriteFast(PIN_A5, HIGH);  // Disable latch #OE
  //   DDRD = 0xFF;                     // Set all PORTD pins as outputs
  // }
}


__attribute__((optimize("-Os"))) 
void Utils::restoreScreen(const char *filename) {
  FatFile &file = SdCardSupport::getFile();
  if (file.isOpen()) file.close();
  FatFile &root = SdCardSupport::getRoot();
  if (root.isOpen()) { root.close(); }
  if (root.open("/")) {
    if (file.open(filename, O_READ)) {
      uint16_t currentAddress = 0x4000;
      uint16_t totalRemaining = ZX_SCREEN_BITMAP_SIZE + ZX_SCREEN_ATTR_SIZE;  // 6912 bytes
      while (totalRemaining > 0) {
        uint16_t bytesRead = file.read(&BufferManager::packetBuffer[GLOBAL_MAX_PACKET_LEN], COMMAND_PAYLOAD_SECTION_SIZE);
        Z80Bus::rleOptimisedTransfer(bytesRead, currentAddress, false);
        currentAddress += bytesRead;
        totalRemaining -= bytesRead;
      }
      file.close();
    }
  }
}


// TODO - this section of code is mostly prototype-grade. Fix me soon.
__attribute__((optimize("-Ofast"))) 
void Utils::waitForUserExit() {

  readJoystick(); // flush junk (Z80 rd/wr port 0x1f shared)

  uint8_t buttonData;
  while (true) {
    buttonData = Utils::readJoystick();
    PORTD = buttonData & JOYSTICK_MASK;  // Kempston joystick interface

    if (buttonData & JOYSTICK_SELECT) {
      pinModeFast(Pin::ShiftRegClockPin, INPUT);
      // delay(1); not needed - Zynaps shows yellow with or without

      // PCB UPDATE: Added 10k resistor between NANO A2 and Z80 /RD.  UPDATE: Tried /IORQ and it is actually better!
      // Why? While Z80 specs suggest they are similar, one timing graph shows /IORQ has a sharper edge.
      // Anyway, using /IORQ gives better results when timing the /NMI fire.
      // I still use the stack (one deep), hopefully most games aren't doing anything crazy with the stack at that point
      // -----------------------------------------------
      // Find a safer spot in the game code where we can maybe break in wihout hitting a manipulated stack.
  //    while (digitalReadFast(Pin::ShiftRegClockPin)); // wait for safer code 
    //  digitalWriteFast(Pin::Z80_NMI, LOW);  // jumps to ROM's idle loop at 0x0066
   //   digitalWriteFast(Pin::Z80_NMI, HIGH);
      // -----------------------------------------------

      asm volatile (
          "1: sbic %[pin],2   \n\t"   // skip if LOW
          "rjmp 1b            \n\t"   // loop while HIGH
          "cbi  %[port],0     \n\t"   // NMI low
          "sbi  %[port],0     \n\t"   // NMI high
          :
          : [pin]  "I" (_SFR_IO_ADDR(PINC)),
            [port] "I" (_SFR_IO_ADDR(PORTC))
      );



      storeZ80States();
      pinModeFast(Pin::ShiftRegClockPin, OUTPUT);
      saveScreen(SCRATCH_FILE); 
      while ((Utils::readJoystick() & JOYSTICK_SELECT)) {}
      
      uint8_t result = pauseMenu();
   
      // For a clean transition we clear colour attributes first before restoring screen
      Z80Bus::sendFillCommand( ZX_SCREEN_ATTR_ADDRESS_START, ZX_SCREEN_ATTR_SIZE, COL::BLACK_BLACK);  

      if (result == 6 ) { break; }  // back to main menu
      while ((Menu::getButton() != Menu::BUTTON_NONE)) {}  // wait for button let go
      restoreScreen(SCRATCH_FILE);
      restoreZ80States();
      readJoystick(); // flush junk
    }
  }
}

uint8_t Utils::pauseMenu() {
  uint8_t index = 0;
  uint16_t oldHighlightAddress = 0;
  
  const uint16_t INITIAL_DELAY = 380; 
  const uint16_t REPEAT_RATE = 120;
  
  unsigned long lastActionTime = 0;
  bool isRepeating = false;
  Menu::Button_t lastButton = Menu::BUTTON_NONE;

  Utils::clearScreen(COL::BLACK_WHITE);
  Draw::text_P(80, 8 * 4, F("PAUSE MENU"));
  Draw::text_P(80, 8 * 6, F("Resume"));           // 1
  Draw::text_P(80, 8 * 7, F("Save SNA"));
  Draw::text_P(80, 8 * 8, F("SlowMo [normal]"));
  Draw::text_P(80, 8 * 9, F("Cheats"));
  Draw::text_P(80, 8 * 10, F("Screenshot"));
  Draw::text_P(80, 8 * 11, F("Mem View"));
  Draw::text_P(80, 8 * 12, F("Exit"));           // 6

  while (true) {
    highlightSelection(index + 6, 0, oldHighlightAddress);
    Menu::Button_t currentButton = Menu::getButton();

    if (currentButton == Menu::BUTTON_NONE) {
      lastActionTime = 0;
      isRepeating = false;
      lastButton = Menu::BUTTON_NONE;
    } 
    else {
      unsigned long now = millis();     
      bool shouldTrigger = false;

      if (currentButton != lastButton) {
        shouldTrigger = true;
        lastActionTime = now;
        isRepeating = false;
      } 
      else {
        uint16_t threshold = isRepeating ? REPEAT_RATE : INITIAL_DELAY;
        if (now - lastActionTime >= threshold) {
          shouldTrigger = true;
          lastActionTime = now;
          isRepeating = true;
        }
      }

      if (shouldTrigger) {
        if (currentButton == Menu::BUTTON_ADVANCE && index < 6) index++;
        else if (currentButton == Menu::BUTTON_BACK && index > 0) index--;
        else if (currentButton == Menu::BUTTON_MENU) {
          if (index == 0 || index == 6) break;
        }
      }
      
      lastButton = currentButton;
    }
    
    delay(1); 
  }
  return index;
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

