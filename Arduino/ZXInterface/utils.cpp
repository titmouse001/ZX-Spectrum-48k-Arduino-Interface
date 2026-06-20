#include "utils.h"
#include "Z80Bus.h"

#include "draw.h"
#include "SdCardSupport.h"
#include "BufferManager.h"
//#include "CommandRegistry.h"
#include "z80bus.h"
#include "PacketBuilder.h"
#include "menu.h"
//#include "constants.h"
#include "PacketTypes.h"
#include "pin.h"

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


__attribute__((optimize("-Os")))
void Utils::clearScreen(uint8_t col) {
  Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START, ZX_SCREEN_ATTR_SIZE, col);
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

__attribute__((optimize("-Os"))) 
void Utils::setupJoystick() {
  // Setup pins for "74HC165" shift register
  pinModeFast(Pin::ShiftRegDataPin, INPUT);
  pinModeFast(Pin::ShiftRegLatchPin, OUTPUT);
  pinModeFast(Pin::ShiftRegClockPin, OUTPUT);
}

__attribute__((optimize("-Os"))) 
void Utils::storeZ80States() {
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

  // 0x04AA: Z80 code jumps to '.restoreInGameState', then enters an idle loop. 
  // This allows time for the ROM to swap back to the stock ROM and begin the restore process at 0x04AA.
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

  // // About to restore the stock ROM and continue game
  // // We need to put something useful on the output pins for joystick port 0x1F.
  // const uint8_t buttonData = Utils::readJoystick();
  // PORTD = buttonData & INPUT_MASK; 

  // // Give Z80 time to reach the next idle loop so the stock ROM can take control.
  // delay(1);               
  // // Stock rom escapes the idle loop via its NOPs
  // Z80Bus::setStockRom();  
  // // let the stock rom catch up
  // delay(1);               
}

__attribute__((optimize("-Os"))) 
void Utils::saveScreen(const char* filename) {

  FatFile& file = SdCardSupport::closeFileIfOpen();
  FatFile& root = SdCardSupport::closeRootIfOpen();

  // UnOptimised version
  // RequestSendDataPacket pkt (ZX_SCREEN_BITMAP_SIZE + ZX_SCREEN_ATTR_SIZE, 0x4000);
  // Z80Bus::sendBytes((uint8_t*)&pkt, sizeof(RequestSendDataPacket)); 
  // if (root.open("/")) {
  //   file.open(filename, O_CREAT | O_WRONLY);
  //   for (uint16_t i = 0; i < ZX_SCREEN_BITMAP_SIZE + ZX_SCREEN_ATTR_SIZE; i++) {
  //     file.write(Z80Bus::get_IO_Byte());
  //   }
  //   file.close();
  // }

  if (root.open("/")) {
    while (!file.open(filename, O_CREAT | O_WRONLY)) {
      // Can't warn user here - need are in the middle of storing the screen!!!
      while (!SdCardSupport::init()) { delay(20); }  // worst case - sd card removed, full reset needed.
    }

    // send header detials first (request Spectrum to send all screen data)
    RequestSendDataPacket pkt (ZX_SCREEN_BITMAP_SIZE + ZX_SCREEN_ATTR_SIZE, 0x4000);
    Z80Bus::sendBytes((uint8_t*)&pkt, sizeof(RequestSendDataPacket)); 

    // Above will be doing a final NMI to unhalt Z80
    // We need to give it time to catch up before we slam the lines to input!
    delay(1);  // todo - will do for now, but this is way to mutch time!

    DDRD = 0x00;   // make them inputs
    digitalWriteFast(PIN_A5, LOW);  //Enable Latch for reading
    __asm__ __volatile__("nop; nop");

    for (uint16_t i = 0; i < ZX_SCREEN_BITMAP_SIZE + ZX_SCREEN_ATTR_SIZE; i++) {
      Z80Bus::waitHalt_syncWithZ80();
      uint8_t byte = PIND;  // Capture latched data
      Z80Bus::triggerZ80NMI();
      file.write(byte);
    }
    file.close();

    digitalWriteFast(PIN_A5, HIGH);  // Disable latch #OE
    DDRD = 0xFF;                     // Set all PORTD pins as outputs
  }
}


__attribute__((optimize("-Os"))) 
void Utils::restoreScreen(const char* filename) {

  FatFile& file = SdCardSupport::closeFileIfOpen();
  FatFile& root = SdCardSupport::closeRootIfOpen();

  if (root.open("/")) {
    if (file.open(filename, O_READ)) {

      const uint16_t mark = BufferManager::getMark();
      uint8_t* buf = BufferManager::allocate(FILE_READ_BUFFER_SIZE);
      uint16_t currentAddress = 0x4000;
      uint16_t totalRemaining = ZX_SCREEN_BITMAP_SIZE + ZX_SCREEN_ATTR_SIZE;  // 6912 bytes
      while (totalRemaining > 0) {
        uint16_t bytesRead = file.read(buf, FILE_READ_BUFFER_SIZE);
        Z80Bus::rleOptimisedTransfer(bytesRead, currentAddress, buf, false);
        currentAddress += bytesRead;
        totalRemaining -= bytesRead;
      }
      file.close();

      BufferManager::freeToMark(mark);
    }
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


static inline char hexChar(uint8_t nibble) {
  nibble &= 0x0F;
  return nibble < 10 ? '0' + nibble : 'A' + (nibble - 10);
}

__attribute__((optimize("-Os"))) 
void Utils::viewSpeccyMemory() {
  constexpr uint8_t MAX_CHARS_PER_LINE = 42;  // (256pixels/6)
  constexpr uint8_t BYTES_TO_SHOW_PER_LINE = 8;
  int32_t currentBaseAddr = 0x5B00;  // start of program mem
  uint16_t mark = BufferManager::getMark();
 // uint8_t* buf = BufferManager::allocate(sizeof(RequestSendDataPacket));
  char* lineBuffer = (char*)BufferManager::allocate(MAX_CHARS_PER_LINE + 1);

  memset(lineBuffer, '\0', MAX_CHARS_PER_LINE + 1);

  while (true) {
    uint16_t tempBaseAddr = currentBaseAddr;
    for (uint16_t i = 0; i < ZX_SCREEN_HEIGHT_BYTES; i++) {

      uint8_t pos = 0;
      // Use 6 chars for formatting address i.e. "AEFF: "
      lineBuffer[pos++] = hexChar(tempBaseAddr >> 12);
      lineBuffer[pos++] = hexChar(tempBaseAddr >> 8);
      lineBuffer[pos++] = hexChar(tempBaseAddr >> 4);
      lineBuffer[pos++] = hexChar(tempBaseAddr & 0x0F);
      lineBuffer[pos++] = ':';
      lineBuffer[pos++] = ' ';

      RequestSendDataPacket pkt (BYTES_TO_SHOW_PER_LINE, tempBaseAddr);
      Z80Bus::sendBytes((uint8_t*) &pkt, sizeof(RequestSendDataPacket));

      for (uint8_t k = 0; k < 8; ++k) {
        uint8_t b = Z80Bus::get_IO_Byte();
        // Use 3 chars for formatting byte data i.e ("1B ")
        lineBuffer[pos++] = hexChar(b >> 4);
        lineBuffer[pos++] = hexChar(b & 0x0F);
        lineBuffer[pos++] = ' ';
        // ascii text far right - "." for unknown (+30 draws past previous chars)
        lineBuffer[6 + (3 * 8) + k] = (b >= 32 && b <= 126) ? b : '.';
      }
      Draw::textLine(i * 8, lineBuffer);  // i*8 : ypos pixels
      tempBaseAddr += BYTES_TO_SHOW_PER_LINE;
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
        //while (Menu::getButton() != Menu::BUTTON_NONE);
        Menu::waitForRelease();
        BufferManager::freeToMark(mark);  // frees both allocs
        return;
      }
    }
  }
}

constexpr char SHOTS[] = "SHOTS";   // Folder name (5 chars) & filename prefix (4 chars)
constexpr char SCR_EXT[] = ".SCR";  // Extension

__attribute__((optimize("-Os"))) 
bool Utils::exportScreenshot() {
  FatFile& root = SdCardSupport::getRoot();
  if (!root.isOpen() && !root.open("/")) {
    return false;
  }

  // make sure "SHOTS" folder exists
  FatFile dir;
  if (!dir.open(&root, SHOTS, O_READ)) {
    if (!dir.mkdir(&root, SHOTS) || !dir.open(&root, SHOTS, O_READ)) {
      return false;
    }
  }

  // Search for the first free number slot
  FatFile file;
  char filename[] = "SHOT0000.SCR";  // sizeof(filename) == 13, null included
  for (uint16_t i = 0; i < 9999; i++) {
    if (++filename[7] > '9') {
      filename[7] = '0';
      if (++filename[6] > '9') {
        filename[6] = '0';
        if (++filename[5] > '9') {
          filename[5] = '0';
          ++filename[4];  // thousands digit
        }
      }
    }
    if (!file.open(&dir, filename, O_READ)) {
      Draw::text((ZX_SCREEN_WIDTH_PIXELS / 2) - ((6 * 12) / 2), (ZX_SCREEN_HEIGHT_PIXELS / 2) + 8, filename);
      Draw::text_P((ZX_SCREEN_WIDTH_PIXELS / 2) - ((6 * 6) / 2), (ZX_SCREEN_HEIGHT_PIXELS / 2) + 16, F("Saving"));
      break;  // name found
    }
    file.close();  // exists - try next
  }

  // Copy the scratch file to the new filename
  uint16_t mark = BufferManager::getMark();
  uint8_t* buf = BufferManager::allocate(FILE_READ_BUFFER_SIZE);
  if (file.open(&root, SCRATCH_FILE, O_READ)) {
    FatFile destFile;
    if (destFile.open(&dir, filename, O_CREAT | O_WRONLY)) {
      int bytesRead;
      while ((bytesRead = file.read(buf, FILE_READ_BUFFER_SIZE)) > 0) {
        destFile.write(buf, bytesRead);
      }
      destFile.close();
    }
    file.close();
  }
  dir.close();
  BufferManager::freeToMark(mark);

  Draw::text_P((ZX_SCREEN_WIDTH_PIXELS / 2) - ((6 * 15) / 2), (ZX_SCREEN_HEIGHT_PIXELS / 2) + 16, F("SAVED - ANY KEY"));

  return true;
}

__attribute__((optimize("-Os"))) 
void Utils::stockRomBoot_Blocking()
{
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
      delay(20);
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


// // // ****** DEBUG ONLY *************
// // char _c[8];
// // //sprintf(_c, "%d", BufferManager::poolOffsetLastMax );
// // itoa(BufferManager::poolOffsetLastMax, _c, 10);
// // Draw::text(256 - 64, 32, _c);
// // // *******************************


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
