#include "utils.h"
#include "Z80Bus.h"

#include "draw.h"
#include "SdCardSupport.h"
#include "BufferManager.h"
#include "z80bus.h"
#include "menu.h"
#include "PacketTypes.h"
#include "pin.h"
#include "constants.h"

void Utils::clearScreen(uint8_t col) {
  Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START, ZX_SCREEN_ATTR_SIZE, col);
  //Z80Bus::sendFillCommand(ZX_SCREEN_ADDRESS_START, ZX_SCREEN_BITMAP_SIZE, 0);
  ClearScreenBitmap pktBitmap;
  Z80Bus::sendBytes((uint8_t*)&pktBitmap, sizeof(ClearScreenBitmap));
//  ClearScreenAttributes pktAttributes;
//  Z80Bus::sendBytes((uint8_t*)&pktAttributes, sizeof(ClearScreenBitmap)); // only black/black
}

// Kempston Joystick Support:
// 1. The Arduino polls the joystick at 50 FPS via a 74HC165 shift register.
// 2. When the Spectrum performs an IN (C) instruction (e.g. LD C,$1F; IN D,(C)), the Arduino drives the Z80 data bus.
//    A 74HC32 combines the /IORQ, /RD, and /A7 signals to detect when the joystick port is being accessed.
//    A 74HC245D transceiver connects the Arduino to the Z80 to send joystick data, and it is tri-stated when not in use.

//__attribute__((optimize("-Ofast")))
uint8_t Utils::readJoystick() {
  digitalWriteFast(Pin::ShiftRegLatchPin, LOW);   // snapshot joystick
  delayMicroseconds(1);                           // let the latch IC complete
  digitalWriteFast(Pin::ShiftRegLatchPin, HIGH);  // Enable shifting + 1st bit now available

  uint8_t data = 0;
  for (uint8_t i = 0; i < 8; i++) {
    data <<= 1;
    digitalWriteFast(Pin::ShiftRegClockPin, LOW);
    if (digitalReadFast(Pin::ShiftRegDataPin)) {  // get serial data
      data |= 1;
    }
    digitalWriteFast(Pin::ShiftRegClockPin, HIGH);  // next bit
  }
  return data;  // bits "XsfFUDLR"
  // bit 0 to 7: Right,Left,Down,Up,Fire1,Fire2, Select (PCB button) and last bit is not used.
}

void Utils::setupJoystick() {
  // Setup pins for "74HC165" shift register
  pinModeFast(Pin::ShiftRegDataPin, INPUT);
  pinModeFast(Pin::ShiftRegLatchPin, OUTPUT);
  pinModeFast(Pin::ShiftRegClockPin, OUTPUT);
}

Z80Registers* Utils::storeZ80States() {
  uint16_t mark = BufferManager::getMark();
  Z80Registers* regs = (Z80Registers*)BufferManager::allocate(sizeof(Z80Registers));

  // !!! not the same order as restore !!!
  regs->a = Z80Bus::get_IO_Byte();
  regs->b = Z80Bus::get_IO_Byte();
  regs->c = Z80Bus::get_IO_Byte();
  regs->sp_hi = Z80Bus::get_IO_Byte();
  regs->sp_lo = Z80Bus::get_IO_Byte();
  regs->f = Z80Bus::get_IO_Byte();
  regs->i = Z80Bus::get_IO_Byte();
  regs->iff2 = Z80Bus::get_IO_Byte();
  regs->d = Z80Bus::get_IO_Byte();
  regs->e = Z80Bus::get_IO_Byte();
  regs->h = Z80Bus::get_IO_Byte();
  regs->l = Z80Bus::get_IO_Byte();
  regs->ixh = Z80Bus::get_IO_Byte();
  regs->ixl = Z80Bus::get_IO_Byte();
  regs->iyh = Z80Bus::get_IO_Byte();
  regs->iyl = Z80Bus::get_IO_Byte();
  regs->a_prime = Z80Bus::get_IO_Byte(); 
  regs->b_prime = Z80Bus::get_IO_Byte();
  regs->c_prime = Z80Bus::get_IO_Byte();
  regs->f_prime = Z80Bus::get_IO_Byte();
  regs->d_prime = Z80Bus::get_IO_Byte();
  regs->e_prime = Z80Bus::get_IO_Byte();
  regs->h_prime = Z80Bus::get_IO_Byte();
  regs->l_prime = Z80Bus::get_IO_Byte();
  regs->r = 255; // memory refresh register - anything will do
  regs->borderCol = COL::RED;  // red to stand out if this failes as will be overwritten later
  // 0x3f is the Speccys default ROM's startup
  // Anything other then 99% chance the games uses IM2 (i.e. it's something like i==0xfe)
  regs->im = (regs->i == 0x3f) ? 1 : 2;

  regs->AllocMark = mark;
  return regs;
}


void Utils::restoreZ80States(Z80Registers* regs) {
  
  // 0x04AA: Z80 code jumps to '.restoreInGameState', then enters an idle loop. 
  // This allows time for the ROM to swap back to the stock ROM and begin the restore process.
  uint8_t addr0x04AA[] = { 0x04, 0xAA };  // jump to address
  Z80Bus::sendBytes(addr0x04AA, sizeof(addr0x04AA));

  // !!! not the same order as store !!!
  Z80Bus::sendBytes(&regs->sp_hi, 1);
  Z80Bus::sendBytes(&regs->sp_lo, 1);
  Z80Bus::sendBytes(&regs->d, 1);
  Z80Bus::sendBytes(&regs->e, 1);
  Z80Bus::sendBytes(&regs->b, 1);
  Z80Bus::sendBytes(&regs->c, 1);
  Z80Bus::sendBytes(&regs->h, 1);
  Z80Bus::sendBytes(&regs->l, 1);
  Z80Bus::sendBytes(&regs->i, 1);
  Z80Bus::sendBytes(&regs->ixh, 1);
  Z80Bus::sendBytes(&regs->ixl, 1);
  Z80Bus::sendBytes(&regs->iyh, 1);
  Z80Bus::sendBytes(&regs->iyl, 1);
  Z80Bus::sendBytes(&regs->b_prime, 1);
  Z80Bus::sendBytes(&regs->c_prime, 1);
  Z80Bus::sendBytes(&regs->f_prime, 1);
  Z80Bus::sendBytes(&regs->d_prime, 1);
  Z80Bus::sendBytes(&regs->e_prime, 1);
  Z80Bus::sendBytes(&regs->h_prime, 1);
  Z80Bus::sendBytes(&regs->l_prime, 1);
  Z80Bus::sendBytes(&regs->a_prime, 1);
  Z80Bus::sendBytes(&regs->iff2, 1);
  // no point doing R (z80's .restoreInGameState: skips reg-R)
  Z80Bus::sendBytes(&regs->f, 1);  
  Z80Bus::sendBytes(&regs->a, 1);

  BufferManager::freeToMark(regs->AllocMark);
}

void Utils::dumpMemoryAsSnapshot(Z80Registers* regs, char* fileName, FatFile& dir) {
  FatFile& file = SdCardSupport::closeFile();
  
  if (!file.open(&dir, fileName, O_WRONLY | O_CREAT | O_TRUNC)) {
    return;
  }
  
  file.write((const uint8_t*)regs, sizeof(Z80Registers) - 2);  // -2 don't include AllocMark attribute

  constexpr uint16_t size = 1024U * 48; // 48K Spectrum RAM size
  
  // Alert Z80 to send memory data packets
  RequestSendDataPacket pkt(size, ZX_SCREEN_ADDRESS_START);
  Z80Bus::sendBytes((uint8_t*)&pkt, sizeof(RequestSendDataPacket));

  // Small delay buffer allowing hardware states to equalize
  Utils::delay16(1);  

  DDRD = 0x00;                    // Set PORTD data pins to inputs
  digitalWriteFast(PIN_A5, LOW);  // Enable output latch for reading bus data
  __asm__ __volatile__("nop; nop");

  // Stream data out of the Z80 bus directly onto the SD file stream
  for (uint16_t i = 0; i < size; i++) {
    Z80Bus::waitHalt_syncWithZ80();
    uint8_t byte = PIND;          // Capture latched data from lines
    Z80Bus::triggerZ80NMI();
    file.write(byte);
  }
  
  file.close();

  digitalWriteFast(PIN_A5, HIGH);  // Disable latch #OE line
  DDRD = 0xFF;                     // Return PORTD pins safely back to output modes
}

void Utils::saveMemory(const char* filename, uint16_t address, uint16_t size) {
  FatFile& file = SdCardSupport::closeFile();
  FatFile& root = SdCardSupport::reopenRoot();

  while (!file.open(filename, O_CREAT | O_WRONLY)) {
    // Can't warn user here - need are in the middle of storing memory!!!
    while (!SdCardSupport::init()) {
      Utils::delay16(20);
    }  // worst case - sd card removed, full reset needed.
  }

  // send header details first (request Z80 to send the memory data)
  RequestSendDataPacket pkt(size, address);
  Z80Bus::sendBytes((uint8_t*)&pkt, sizeof(RequestSendDataPacket));

  // Above will be doing a final NMI to unhalt Z80
  // We need to give it time to catch up before we slam the lines to input!
  Utils::delay16(1);  // todo - will do for now, but this is way too much time!

  DDRD = 0x00;                    // make them inputs
  digitalWriteFast(PIN_A5, LOW);  // Enable Latch for reading
  __asm__ __volatile__("nop; nop");

  for (uint16_t i = 0; i < size; i++) {
    Z80Bus::waitHalt_syncWithZ80();
    uint8_t byte = PIND;  // Capture latched data
    Z80Bus::triggerZ80NMI();
    file.write(byte);
  }
  file.close();

  digitalWriteFast(PIN_A5, HIGH);  // Disable latch #OE
  DDRD = 0xFF;                     // Set all PORTD pins as outputs
}

void Utils::loadMemory(const char* filename, uint16_t address, uint16_t size) {
  FatFile& file = SdCardSupport::closeFile();
  FatFile& root = SdCardSupport::reopenRoot();

  if (file.open(filename, O_READ)) {
    const uint16_t mark = BufferManager::getMark();
    uint8_t* buf = BufferManager::allocate(FILE_READ_BUFFER_SIZE);
    uint16_t currentAddress = address;
    uint16_t totalRemaining = size;

    while (totalRemaining > 0) {
      uint16_t bytesToRead = (totalRemaining < FILE_READ_BUFFER_SIZE) ? totalRemaining : FILE_READ_BUFFER_SIZE;
      uint16_t bytesRead = file.read(buf, bytesToRead);
      if (bytesRead == 0) break;
      Z80Bus::rleOptimisedTransfer(bytesRead, currentAddress, buf, CMD_Copy);
      currentAddress += bytesRead;
      totalRemaining -= bytesRead;
    }
    file.close();
    BufferManager::freeToMark(mark);
  }
}

// ---------------------
// .TXT FILE - Support
// ---------------------
__attribute__((optimize("-Ofast")))
uint16_t Utils::readLineTxt(FatFile* f, char* buf, uint16_t maxChars) {
  uint16_t i = 0;
  uint8_t c;
  while (i < maxChars) {
    if (f->read(&c, 1) <= 0) break;  // EOF
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

void Utils::viewSpeccyMemory() {
  constexpr uint8_t MAX_CHARS_PER_LINE = 38; // max 42 chars;  // (256pixels/6)
  constexpr uint8_t HEX_TO_SHOW_PER_LINE = 8;
  int32_t currentBaseAddr = 0x5B00;  // start of program mem
  uint16_t mark = BufferManager::getMark();
  char* lineBuffer = (char*)BufferManager::allocate(MAX_CHARS_PER_LINE + 1); // 0 to 38

  // layout: "XXXX: XX XX XX XX XX XX XX XX ........"
  memset(lineBuffer, ' ', MAX_CHARS_PER_LINE );
  lineBuffer[4] = ':';
  lineBuffer[MAX_CHARS_PER_LINE] = '\0';

  const char hexDigits[] = "0123456789ABCDEF";

  while (true) {
    uint16_t tempBaseAddr = currentBaseAddr;
    for (uint16_t i = 0; i < ZX_SCREEN_HEIGHT_BYTES; i++) {

      uint8_t pos = 0;
      // Use 6 chars for formatting address i.e. "AEFF: "
      lineBuffer[0] = hexDigits[(tempBaseAddr >> 12) & 0x0F ];
      lineBuffer[1] = hexDigits[(tempBaseAddr >> 8)  & 0x0F];
      lineBuffer[2] = hexDigits[(tempBaseAddr >> 4)  & 0x0F];
      lineBuffer[3] = hexDigits[(tempBaseAddr & 0x0F) ];
      pos+=2;  // skip pre filled

      // ASK FOR 8 BYTES
      RequestSendDataPacket pkt (HEX_TO_SHOW_PER_LINE, tempBaseAddr);
      // START receiving process from z80 (we will render what we get and send it back as graphics)
      Z80Bus::sendBytes((uint8_t*) &pkt, sizeof(RequestSendDataPacket));

      uint8_t hexPos = 6;   // start of hex field (after "XXXX: ")
      for (uint8_t k = 0; k < HEX_TO_SHOW_PER_LINE; ++k) {
        uint8_t b = Z80Bus::get_IO_Byte();
        // hex pair + space e.g. ("1B ")
        lineBuffer[hexPos]     = hexDigits[(b >> 4) & 0x0f];
        lineBuffer[hexPos + 1] = hexDigits[(b & 0x0F) ];
        hexPos += 3;

        // ASCII on the far right, "." for unknown
        lineBuffer[6 + (3 * HEX_TO_SHOW_PER_LINE) + k] = (b >= 32 && b <= 126) ? b : '.';
      }
      Draw::textLine(i * 8, lineBuffer);  // i*8 : ypos pixels
      tempBaseAddr += HEX_TO_SHOW_PER_LINE;
    }

    while (true) {
      Menu::Button_t btn = Menu::getButton();
      if (btn == Menu::BUTTON_ADVANCE) {
        currentBaseAddr += ZX_SCREEN_HEIGHT_PIXELS;  // 8 * 24;
        if (currentBaseAddr > 0xFFFF - ZX_SCREEN_HEIGHT_PIXELS) {
          currentBaseAddr = 0xFFFF - (ZX_SCREEN_HEIGHT_PIXELS - 1);  // = 0xFF40
        }
        break;  // Redraw
      } else if (btn == Menu::BUTTON_BACK) {
        currentBaseAddr -= ZX_SCREEN_HEIGHT_PIXELS;  // 8 * 24;
        if (currentBaseAddr < 0) {
          currentBaseAddr = 0;
        }
        break;  // Redraw
      } else if (btn == Menu::BUTTON_MENU) {
        Menu::waitForRelease();
        BufferManager::freeToMark(mark);  // frees both allocs
        return;
      }
    }
  }
}

void Utils::resetSystem() {
  pinModeFast(Pin::Z80_REST, OUTPUT);
  digitalWriteFast(Pin::Z80_REST, LOW);  // begin reset

  Z80Bus::setupPins();
  Utils::setupJoystick();

  Utils::delay16(Z80_RESET_TIME);

  digitalWriteFast(Pin::Z80_REST, HIGH);  // release RESET (Z80 restarts)
  Utils::delay16(1);
}

void Utils::stockRomBoot_Blocking() {
  Z80Bus::setStockRom();
  Z80Bus::resetZ80(); // Resets Z80 for a clean boot from internal ROM.
  while ((Utils::readJoystick() & INPUT_SELECT) != 0){} // Debounces button release.
  while (true) {
    // Allow the user to return back to the game loader screen.
    if (Utils::readJoystick() & INPUT_SELECT) {
      Z80Bus::setSnaRom();
      while ((Utils::readJoystick() & INPUT_SELECT) != 0) { }
      break; // continue with Sna loader ROM
    }
  }
}

void Utils::waitForSDCard_Blocking(bool clearScreen) {
  if (!SdCardSupport::init()) { 
    if (clearScreen) {
      Utils::clearScreen(COL::BLACK_WHITE); 
    }
    Draw::text_P(80, 90, F("INSERT SD CARD"));
    do {
      Utils::delay16(20);
    } while (!SdCardSupport::init());  // keep looking
    Utils::clearScreen(COL::BLACK_WHITE);
  }
}

void Utils::show5VoltRailStatus() {
  //constexpr uint16_t VOLTAGE_OK_MIN = 4850;    // 4.85V
  //constexpr uint16_t VOLTAGE_OK_MAX = 5150;    // 5.15V
  //constexpr uint16_t VOLTAGE_WARN_MIN = 4750;  // 4.75V
  //constexpr uint16_t VOLTAGE_WARN_MAX = 5250;  // 5.25V

  static uint32_t filtered_vcc = 5000;  // 5.00V

  // ADC Read / Measure voltage - Nano's internal reference is around 1.1V
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  ADCSRA |= _BV(ADSC);
  while (bit_is_set(ADCSRA, ADSC));

  uint32_t raw_adc = ADCW;
  uint32_t raw_vcc = 1105920 / raw_adc;
  filtered_vcc = filtered_vcc - (filtered_vcc >> 6) + raw_vcc;
  uint16_t display_vcc = filtered_vcc >> 6;
  char voltageStr[] = { //'V','o','l','t','a','g','e',':',
                        static_cast<char>('0' + (display_vcc / 1000)),
                        '.',
                        static_cast<char>('0' + ((display_vcc % 1000) / 100)),
                        static_cast<char>('0' + ((display_vcc % 100) / 10)),
                        ' ','V','o','l','t','s',
                        '\0'};

  Z80Bus::sendFillCommand( ZX_SCREEN_ATTR_ADDRESS_START + 32 - sizeof(voltageStr) + (((192-8)/8)*32), sizeof(voltageStr), COL::BLUE_WHITE);
  Draw::text(256-(6*sizeof(voltageStr)), 192-8, voltageStr);  // top right corner
}

void Utils::delay16(uint16_t ms) {
    const uint32_t start = millis();
    while ((uint16_t)(millis() - start) < ms) {  }

}

// Screen Address (16-bit): starting at 16384 (0x4000, %0100000000000000)
// +---+---+---+---+---+---+---+---+ +---+---+---+---+---+---+---+---+
// |15 |14 |13 |12 |11 |10 | 9 | 8 | | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
// +---+---+---+---+---+---+---+---+ +---+---+---+---+---+---+---+---+
// | 0 | 1 | 0 |   TT  |    SSS    | |    RRR    |       CCCCC       |
// +---+---+---+---+---+---+---+---+ +---+---+---+---+---+---+---+---+
// \___________ High Byte _________/ \___________ Low Byte _________/
//
// TT = Screen third (0–2)
// SSS = Pixel row within character line (0–7)
// RRR = Character row within third (0–7)
// CCCCC = Character column (0–31)

uint16_t Utils::zx_spectrum_screen_address(uint8_t x, uint8_t y) {
    uint8_t addr_high = 0x40 | ((y >> 3) & 0x18) | (y & 0x07);
    uint8_t addr_low = ((y & 0x38) << 2) | (x >> 3);
    return (static_cast<uint16_t>(addr_high) << 8) | addr_low;

}

void Utils::join6Bits(byte* output, uint8_t input, uint16_t bitPosition) {
  uint16_t byteIndex = bitPosition >> 3;
  uint8_t bitIndex = bitPosition & 7;
  uint8_t maskedInput = input & 0x3F;

  switch (bitIndex) {
    case 0: 
      output[byteIndex] |= (maskedInput << 2); 
      break;
    case 1: 
      output[byteIndex] |= (maskedInput << 1); 
      break;
    case 2: 
      output[byteIndex] |= maskedInput;        
      break;
    case 3: 
      output[byteIndex]     |= (maskedInput >> 1); 
      output[byteIndex + 1] |= (maskedInput << 7); 
      break;
    case 4: 
      output[byteIndex]     |= (maskedInput >> 2); 
      output[byteIndex + 1] |= (maskedInput << 6); 
      break;
    case 5: 
      output[byteIndex]     |= (maskedInput >> 3); 
      output[byteIndex + 1] |= (maskedInput << 5); 
      break;
    case 6: 
      output[byteIndex]     |= (maskedInput >> 4); 
      output[byteIndex + 1] |= (maskedInput << 4); 
      break;
    case 7: 
      output[byteIndex]     |= (maskedInput >> 5); 
      output[byteIndex + 1] |= (maskedInput << 3); 
      break;
  }
}
void Utils::memsetZero(byte* b, uint16_t len) {
  for (; len != 0; len--) { *b++ = 0; }
}



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
