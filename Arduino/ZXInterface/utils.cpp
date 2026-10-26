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


void Utils::playClick() {
  PlayClick playClick;
  Z80Bus::sendBytes8((uint8_t*)&playClick, sizeof(PlayClick));
}

void Utils::clearScreen(uint8_t col) {
  Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START, ZX_SCREEN_ATTR_SIZE, col);
  //Z80Bus::sendFillCommand(ZX_SCREEN_ADDRESS_START, ZX_SCREEN_BITMAP_SIZE, 0);
  ClearScreenBitmap pktBitmap;
  Z80Bus::sendBytes8((uint8_t*)&pktBitmap, sizeof(ClearScreenBitmap));
}

// Kempston Joystick Support:
// 1. The Arduino polls the joystick via a 74HC165 shift register.
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

// Internal storage for storing Z80 states (not part of the SNA header format).
Z80Registers* Utils::storeZ80States() {
  uint16_t mark = BufferManager::getMark();
  Z80Registers* regs = (Z80Registers*)BufferManager::allocate(sizeof(Z80Registers));

  SNAHeader* sna = &regs->header;

  sna->a = Z80Bus::get_IO_Byte();
  sna->b = Z80Bus::get_IO_Byte();
  sna->c = Z80Bus::get_IO_Byte();
  sna->d = Z80Bus::get_IO_Byte();
  sna->e = Z80Bus::get_IO_Byte();
  sna->h = Z80Bus::get_IO_Byte();
  sna->l = Z80Bus::get_IO_Byte();
  sna->sp_hi = Z80Bus::get_IO_Byte();
  sna->sp_lo = Z80Bus::get_IO_Byte();
  sna->i = Z80Bus::get_IO_Byte();
  sna->iff2 = Z80Bus::get_IO_Byte();
  sna->f = Z80Bus::get_IO_Byte();
  sna->ixh = Z80Bus::get_IO_Byte();
  sna->ixl = Z80Bus::get_IO_Byte();
  sna->iyh = Z80Bus::get_IO_Byte();
  sna->iyl = Z80Bus::get_IO_Byte();
  sna->a_prime = Z80Bus::get_IO_Byte(); 
  sna->b_prime = Z80Bus::get_IO_Byte();
  sna->c_prime = Z80Bus::get_IO_Byte();
  sna->f_prime = Z80Bus::get_IO_Byte();
  sna->d_prime = Z80Bus::get_IO_Byte();
  sna->e_prime = Z80Bus::get_IO_Byte();
  sna->h_prime = Z80Bus::get_IO_Byte();
  sna->l_prime = Z80Bus::get_IO_Byte();
  sna->r = 127; // memory refresh register - from mid point
  sna->borderCol = COL::RED;  // red to stand out if this failes as will be overwritten later
  // 0x3f is the Speccys default ROM's startup
  // Anything other then 99% chance the games uses IM2 (i.e. it's something like i==0xfe)
  sna->im = (sna->i == 0x3f) ? 1 : 2;

  regs->AllocMark = mark;

  return regs;
}

// Internal storage for restoring Z80 states (not part of the SNA header format).
void Utils::restoreZ80States(Z80Registers* regs) {
  
  SNAHeader* sna = &regs->header;

  // 0x04AA: Z80 code jumps to '.restoreInGameState', then enters an idle loop. 
  // This allows time for the ROM to swap back to the stock ROM and begin the restore process.
  uint8_t addr0x04AA[] = { 0x04, 0xAA };  // jump to address
  Z80Bus::sendBytes8(addr0x04AA, sizeof(addr0x04AA));

  Z80Bus::sendByte(&sna->sp_hi);
  Z80Bus::sendByte(&sna->sp_lo);
  Z80Bus::sendByte(&sna->b);
  Z80Bus::sendByte(&sna->c);
  Z80Bus::sendByte(&sna->h);
  Z80Bus::sendByte(&sna->l);
  Z80Bus::sendByte(&sna->i);
  Z80Bus::sendByte(&sna->ixh);
  Z80Bus::sendByte(&sna->ixl);
  Z80Bus::sendByte(&sna->iyh);
  Z80Bus::sendByte(&sna->iyl);
  Z80Bus::sendByte(&sna->b_prime);
  Z80Bus::sendByte(&sna->c_prime);
  Z80Bus::sendByte(&sna->f_prime);
  Z80Bus::sendByte(&sna->d_prime);
  Z80Bus::sendByte(&sna->e_prime);
  Z80Bus::sendByte(&sna->h_prime);
  Z80Bus::sendByte(&sna->l_prime);
  Z80Bus::sendByte(&sna->a_prime);
  Z80Bus::sendByte(&sna->iff2);
  //z80's .restoreInGameState: skips reg-R (hard coded as 127 in store above)
  Z80Bus::sendByte(&sna->f);  
  Z80Bus::sendByte(&sna->d);
  Z80Bus::sendByte(&sna->e);
  Z80Bus::sendByte(&sna->a);

  BufferManager::freeToMark(regs->AllocMark);
}

void Utils::saveMemory(const char* filename, uint16_t address, uint16_t size) {

  FatFile& file = SdCardSupport::closeFile();
  FatFile& root = SdCardSupport::reopenRoot();  

  while (!file.open(filename, O_CREAT | O_WRONLY | O_TRUNC)) {
    // Can't warn user here - we are in the middle of storing memory!!!
    while (!SdCardSupport::init()) {
      Utils::delay16(20);
    }  // worst case - sd card removed, full reset needed.
  }

  streamZ80ToFile(file, address, size );
  file.close();
}

bool Utils::loadMemory(const char* filename, uint16_t address, uint16_t size) {
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
    return true;
  }

  return false;
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

// Sketch uses 28636 bytes (93%) 
// static const char hexDigits[] = "0123456789ABCDEF";  

// Sketch uses 28560 bytes (92%) 
static char getHex(uint8_t n) { n &= 15; return n + (n < 10 ? '0' : 'A' - 10); }

void Utils::viewSpeccyMemory() {
  constexpr uint8_t MAX_CHARS_PER_LINE = 38;  // max 42 chars;  // (256pixels/6)
  constexpr uint8_t HEX_TO_SHOW_PER_LINE = 8;
  uint16_t currentBaseAddr = 0x5B00;  // start of program mem
  uint16_t mark = BufferManager::getMark();
  char* lineBuffer = (char*)BufferManager::allocate(MAX_CHARS_PER_LINE + 1);  // 0 to 38

  // layout: "XXXX: XX XX XX XX XX XX XX XX ........"
  memset(lineBuffer, ' ', MAX_CHARS_PER_LINE);
  lineBuffer[4] = ':';
  lineBuffer[MAX_CHARS_PER_LINE] = '\0';

  // Packet buffer (4-byte header + font payload)
  uint8_t* workingArea = BufferManager::allocate(4 + 224);
  // Send packet header with data
  const uint16_t commandAddr = cmd_addr(CMD_Copy32x7WithFixedStride);
  workingArea[0] = (uint8_t)(commandAddr >> 8);
  workingArea[1] = (uint8_t)(commandAddr);

  // ------------------------------------------------------
  Menu::Button_t lastBtn = Menu::BUTTON_NONE;
  bool buttonHeld = false;
  uint32_t lastPress = 0;
  uint32_t lastHold = 0;
  uint32_t repeatDelay = MAX_REPEAT_KEY_DELAY;
  // ------------------------------------------------------

  while (true) {
    uint8_t hi = 0x40;
    uint8_t lo = 0x00;

    workingArea[2] = hi;
    workingArea[3] = lo;
    uint16_t tempBaseAddr = currentBaseAddr;
    for (uint16_t i = 0; i < ZX_SCREEN_HEIGHT_BYTES; i++) {
      uint8_t pos = 0;
      // Use 6 chars for formatting address i.e. "AEFF: "
      lineBuffer[0] = getHex((tempBaseAddr >> 12) & 0x0F);
      lineBuffer[1] = getHex((tempBaseAddr >> 8) & 0x0F);
      lineBuffer[2] = getHex((tempBaseAddr >> 4) & 0x0F);
      lineBuffer[3] = getHex((tempBaseAddr & 0x0F));
      pos += 2;  // skip pre filled

      // ASK FOR 8 BYTES
      RequestSendDataPacket pkt(HEX_TO_SHOW_PER_LINE, tempBaseAddr);
      Z80Bus::sendBytes8((uint8_t*)&pkt, sizeof(RequestSendDataPacket));

      uint8_t hexPos = 6;  // start of hex field (after "XXXX: ")
      for (uint8_t k = 0; k < HEX_TO_SHOW_PER_LINE; ++k) {
        uint8_t b = Z80Bus::get_IO_Byte();
        // hex pair + space e.g. ("1B ")
        lineBuffer[hexPos] = getHex((b >> 4) & 0x0f);
        lineBuffer[hexPos + 1] = getHex((b & 0x0F));
        hexPos += 3;

        // ASCII on the far right, "." for unknown
        lineBuffer[6 + (3 * HEX_TO_SHOW_PER_LINE) + k] = (b >= 32 && b <= 126) ? b : '.';
      }
      Draw::textLine(lineBuffer, workingArea);
      lo += 0x20;  // overflows back to 0x00
      if (lo == 0)  hi += 8;  // wrapped past 0xE0 -> next 2 KB block (0x4000->0x4800->0x5000)
      workingArea[2] = hi;
      workingArea[3] = lo;
      tempBaseAddr += HEX_TO_SHOW_PER_LINE;
    }

    while (true) {
      Menu::Button_t btn = Menu::getButton();

      if (btn == Menu::BUTTON_NONE) {
        lastBtn = Menu::BUTTON_NONE;
        buttonHeld = false;
        repeatDelay = MAX_REPEAT_KEY_DELAY;
        continue;
      }

      bool clicked = false;
      if (btn != lastBtn) {
        Utils::playClick();
        clicked = true;
      }
      lastBtn = btn;

      if (btn == Menu::BUTTON_MENU) {
        Menu::waitForRelease();
        BufferManager::freeToMark(mark);  // frees both allocs
        return;
      }

      const uint32_t now = millis();
      if (buttonHeld && (now - lastHold >= repeatDelay)) {
        repeatDelay = (repeatDelay > MIN_REPEAT_KEY_DELAY)
                          ? repeatDelay - ADJUST_REPEAT_KEY_DELAY
                          : MIN_REPEAT_KEY_DELAY;
        lastHold = now;
      }
      if (!buttonHeld || (now - lastPress >= repeatDelay)) {
        bool acted = false;

        if (btn == Menu::BUTTON_ADVANCE) {
          currentBaseAddr += ZX_SCREEN_HEIGHT_PIXELS;  // 8 * 24;
          if (currentBaseAddr > 0xFFFF - ZX_SCREEN_HEIGHT_PIXELS) {
            // wrap - but land nicely so we end on 0xFFF8 (start here on 0xFF40)
            currentBaseAddr = 0xFFFF - (ZX_SCREEN_HEIGHT_PIXELS - 1);
          }
          acted = true;
        } else if (btn == Menu::BUTTON_BACK) {
          // wraps
          currentBaseAddr -= ZX_SCREEN_HEIGHT_PIXELS;  // 8 * 24;
          acted = true;
        }

        if (acted) {
          if (!clicked) { 
            Utils::playClick();  
          }
          buttonHeld = true;
          lastPress  = now;
          lastHold   = now;
          break;  // Redraw
        }
      }
    }
  }
}

void Utils::stockRomBoot_Blocking() {

  uint8_t ResetJmpAddr[2] = {0,0};
  Z80Bus::sendBytes8(ResetJmpAddr, sizeof(ResetJmpAddr));  
  delayMicroseconds(12);  // fudge wait this for now - give PC time to end up at 0x0000
  Z80Bus::setStockRom();

  // Above, would be better to run something in Z80's ram to wait then do the 0x0000 jmp.

  while (true) {
    // // Allow the user to return back to the game loader screen.
    // if (Utils::readJoystick() & INPUT_SELECT) {
    //   Z80Bus::setSnaRom();
    //   while ((Utils::readJoystick() & INPUT_SELECT) != 0) { }
    //   break; // continue with Sna loader ROM
    // }
  }
}

void Utils::waitForSDCard_Blocking(bool clearScreen) {
  if (clearScreen) {
      Utils::clearScreen(COL::BLACK_WHITE); 
  }
  boolean doText = true;
  if (!SdCardSupport::init()) { 
    if (doText) {
      doText = false;
      Draw::text_P(80, 90, F("INSERT SD CARD"));
    }
    do { 
      Utils::delay16(20);
    } while (!SdCardSupport::init()); 
    Utils::clearScreen(COL::BLACK_WHITE);
    Utils::delay16(650);   // Grace period for card to be seated solid in the reader
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

  //Sketch uses 28340 bytes (92%)
  //uint32_t raw_adc = ADCW;
  //uint32_t raw_vcc = 1105920 / raw_adc;
  
  // Sketch uses 28312 bytes (92%) of
  const uint16_t raw = ADCW;
  const uint16_t raw_vcc = ((34560u / raw) << 5) + (((34560u % raw) << 5) / raw);  // = 1105920/raw
  
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

// uint16_t Utils::zx_spectrum_screen_address(uint8_t y) {
//     uint8_t addr_high = 0x40 | ((y >> 3) & 0x18) | (y & 0x07);
//     uint8_t addr_low = ((y & 0x38) << 2) ;
//     return (static_cast<uint16_t>(addr_high) << 8) | addr_low;
// }
uint16_t Utils::zx_spectrum_screen_address(uint8_t y) {
    uint8_t hi = 0x40 | ((y & 0xC0) >> 3) | (y & 0x07);
    uint8_t lo = (y & 0x38) << 2;
    return (uint16_t)lo | ((uint16_t)hi << 8);
}

void Utils::clearTopBar(uint8_t col) {
  Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START, ZX_SCREEN_WIDTH_BYTES,col);
  for (uint8_t i = 0; i < 8; i++) {
    Z80Bus::sendFillCommand(Utils::zx_spectrum_screen_address(0, i), ZX_SCREEN_WIDTH_BYTES, 0);
  }
}

void Utils::streamZ80ToFile(FatFile& f, uint16_t addr, uint16_t size) {
  Z80Bus::beginBulkRead(addr, size);
  while (size--) f.write(Z80Bus::bulkReadByte());
  Z80Bus::endBulkRead();
}


// bool Utils::loadBmpToScreen(const char* filename) {
//   FatFile& file = SdCardSupport::closeFile();
//   SdCardSupport::reopenRoot();

//   if (!file.open(filename, O_READ)) return false;

//   // ---- BITMAPFILEHEADER (14 bytes) ----
//   uint8_t fh[14];
//   if (file.read(fh, 14) != 14) { file.close(); return false; }
//   if (fh[0] != 'B' || fh[1] != 'M') { file.close(); return false; }

//   uint32_t dataOffset = (uint32_t)fh[10] | ((uint32_t)fh[11] << 8) |
//                         ((uint32_t)fh[12] << 16) | ((uint32_t)fh[13] << 24);

//   // ---- BITMAPINFOHEADER (>= 40 bytes) ----
//   uint8_t ih[40];
//   if (file.read(ih, 40) != 40) { file.close(); return false; }

//   uint32_t hdrSize = (uint32_t)ih[0] | ((uint32_t)ih[1] << 8) |
//                      ((uint32_t)ih[2] << 16) | ((uint32_t)ih[3] << 24);
//   int32_t  width   = (int32_t)ih[4] | ((int32_t)ih[5] << 8) |
//                      ((int32_t)ih[6] << 16) | ((int32_t)ih[7] << 24);
//   int32_t  height  = (int32_t)ih[8] | ((int32_t)ih[9] << 8) |
//                      ((int32_t)ih[10] << 16) | ((int32_t)ih[11] << 24);
//   uint16_t bitCnt  = (uint16_t)ih[14] | ((uint16_t)ih[15] << 8);

//   if (hdrSize < 40 || bitCnt != 1 || width != 256 ||
//       (height != 192 && height != -192)) {
//     file.close();
//     return false;
//   }
//   bool bottomUp = (height > 0);

//   // Skip any extended-info-header bytes (usually zero for BMP)
//   for (uint32_t i = 40; i < hdrSize; i++) {
//     uint8_t d; if (file.read(&d, 1) != 1) { file.close(); return false; }
//   }

//   // ---- Palette: 2 entries x 4 bytes ----
//   uint8_t pal[8];
//   if (file.read(pal, 8) != 8) { file.close(); return false; }
//   uint16_t lum0 = (uint16_t)pal[0] + pal[1] + pal[2];   // B+G+R
//   uint16_t lum1 = (uint16_t)pal[4] + pal[5] + pal[6];
//   bool invert = (lum0 > lum1);   // ZX wants 0=ink(black), 1=paper(white)

//   // Skip anything between header end and pixel data
//   uint32_t consumed = 14 + hdrSize + 8;
//   if (dataOffset < consumed) { file.close(); return false; }
//   for (uint32_t i = consumed; i < dataOffset; i++) {
//     uint8_t d; if (file.read(&d, 1) != 1) { file.close(); return false; }
//   }

//   // ---- Set attributes (black ink, white paper) ----
//   Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START,
//                           ZX_SCREEN_ATTR_SIZE, COL::BLACK_WHITE);

//   // ---- Stream rows out ----
//   constexpr uint16_t ROW_BYTES = 32;
//   uint8_t rowBuf[ROW_BYTES];

//   for (uint16_t bmpRow = 0; bmpRow < 192; bmpRow++) {
//     if (file.read(rowBuf, ROW_BYTES) != ROW_BYTES) break;
//     if (invert) {
//       for (uint8_t k = 0; k < ROW_BYTES; k++) rowBuf[k] = ~rowBuf[k];
//     }
//     uint8_t imgY = bottomUp ? (uint8_t)(191 - bmpRow) : (uint8_t)bmpRow;
//     uint16_t zxAddr = zx_spectrum_screen_address(0, imgY);
//     Z80Bus::rleOptimisedTransfer(ROW_BYTES, zxAddr, rowBuf, CMD_Copy);
//   }

//   file.close();
//   return true;
// }


// // Convert one source row to a packed 1-bit brightness bitmap.
// // brightBuf: (srcW + 7) / 8 bytes  |  chunkBuf: 128 bytes scratch
// static bool readRowAsBrightBits(FatFile& file, uint16_t srcW,
//                                  uint16_t srcRowBytes, uint16_t rowBytes,
//                                  uint8_t bitCnt, const uint8_t* brightMap,
//                                  uint8_t* brightBuf, uint8_t* chunkBuf,
//                                  uint16_t chunkBufSize) {
//   memset(brightBuf, 0, (srcW + 7) / 8);

//   uint8_t bytesPerPix = 0, pixPerByte = 0;
//   switch (bitCnt) {
//     case 1:  pixPerByte = 8; break;
//     case 4:  pixPerByte = 2; break;
//     case 8:  pixPerByte = 1; break;
//     case 24: bytesPerPix = 3; break;
//     case 32: bytesPerPix = 4; break;
//     default: return false;
//   }

//   uint16_t px = 0, bytesRead = 0;
//   while (bytesRead < srcRowBytes) {
//     uint16_t remaining = srcRowBytes - bytesRead;
//     uint16_t chunkSize;
//     if (remaining <= chunkBufSize) {
//       chunkSize = remaining;
//     } else {
//       chunkSize = chunkBufSize;
//       // Align so a pixel never straddles a chunk boundary
//       if (bitCnt == 24) chunkSize = (chunkSize / 3) * 3;
//       if (bitCnt == 32) chunkSize = (chunkSize / 4) * 4;
//     }
//     if (file.read(chunkBuf, chunkSize) != chunkSize) return false;

//     if (bytesPerPix > 0) {
//       // 24 / 32-bit: BGRA / BGRX, alpha ignored
//       uint16_t n = chunkSize / bytesPerPix;
//       for (uint16_t j = 0; j < n && px < srcW; j++, px++) {
//         uint16_t idx = j * bytesPerPix;
//         uint16_t b = chunkBuf[idx + 0];
//         uint16_t g = chunkBuf[idx + 1];
//         uint16_t r = chunkBuf[idx + 2];
//         if (r + g + b > 384) brightBuf[px >> 3] |= (0x80 >> (px & 7));
//       }
//     } else {
//       // 1 / 4 / 8-bit indexed via brightMap
//       uint16_t n = (uint16_t)chunkSize * pixPerByte;
//       for (uint16_t j = 0; j < n && px < srcW; j++, px++) {
//         uint8_t palIdx;
//         if (bitCnt == 1)      palIdx = (chunkBuf[j >> 3] >> (7 - (j & 7))) & 1;
//         else if (bitCnt == 4) { uint8_t bb = chunkBuf[j >> 1];
//                                 palIdx = (j & 1) ? (bb & 0x0F) : (bb >> 4); }
//         else                  palIdx = chunkBuf[j];
//         if (brightMap[palIdx >> 3] & (0x80 >> (palIdx & 7)))
//           brightBuf[px >> 3] |= (0x80 >> (px & 7));
//       }
//     }
//     bytesRead += chunkSize;
//   }
//   // Skip BMP row padding to next 4-byte boundary
//   while (bytesRead < rowBytes) {
//     uint8_t d; if (file.read(&d, 1) != 1) return false; bytesRead++;
//   }
//   return true;
// }

// // Compact helper to parse 32-bit little-endian values without repeating shift code
// static inline uint32_t parse32(const uint8_t* p) {
//   return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | 
//          ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
// }

// bool Utils::loadBmpToScreen(const char* filename) {
//   FatFile& file = SdCardSupport::closeFile();
//   SdCardSupport::reopenRoot();
//   if (!file.open(filename, O_READ)) return false;

//   uint16_t mark = BufferManager::getMark();
//   uint8_t* brightBuf = nullptr;

//   uint8_t fh[14];
//   if (file.read(fh, 14) != 14 || fh[0] != 'B' || fh[1] != 'M') goto fail;
//   uint32_t dataOffset = parse32(&fh[10]);

//   uint8_t ih[40];
//   if (file.read(ih, 40) != 40) goto fail;

//   uint32_t hdrSize  = parse32(&ih[0]);
//   int32_t width     = (int32_t)parse32(&ih[4]);
//   int32_t height    = (int32_t)parse32(&ih[8]);
//   uint16_t bitCnt   = (uint16_t)ih[14] | ((uint16_t)ih[15] << 8);
//   uint32_t compress = parse32(&ih[16]);
//   uint32_t clrUsed  = parse32(&ih[32]);

//   if (hdrSize < 40 || compress != 0 ||
//       (bitCnt != 1 && bitCnt != 4 && bitCnt != 8 &&
//        bitCnt != 24 && bitCnt != 32) ||
//       width <= 0 || width > 4096 ||
//       height == 0 || height > 4096 || height < -4096) {
//     goto fail;
//   }

//   bool bottomUp = (height > 0);
//   uint16_t srcW = (uint16_t)width;
//   uint16_t srcH = (uint16_t)(height < 0 ? -height : height);

//   // REPLACEMENT 1: Direct seek past extra header bytes instead of a byte loop
//   if (!file.seekSet(14 + hdrSize)) goto fail;

//   // Palette (only for indexed depths)
//   uint8_t brightMap[32] = {0};
//   uint16_t numPal = 0;
//   if (bitCnt <= 8) {
//     numPal = (clrUsed != 0) ? (uint16_t)clrUsed : (1u << bitCnt);
//     if (numPal > 256) numPal = 256;
//     for (uint16_t i = 0; i < numPal; i++) {
//       uint8_t pal[4];
//       if (file.read(pal, 4) != 4) goto fail;
//       if ((uint16_t)pal[0] + pal[1] + pal[2] > 384)
//         brightMap[i >> 3] |= (0x80 >> (i & 7));
//     }
//   }

//   // REPLACEMENT 2: Direct absolute seek to pixel data instead of a byte loop
//   if (!file.seekSet(dataOffset)) goto fail;

//   // Row layout
//   uint16_t srcRowBytes = 0;
//   switch (bitCnt) {
//     case 1:  srcRowBytes = (srcW + 7) / 8; break;
//     case 4:  srcRowBytes = (srcW + 1) / 2; break;
//     case 8:  srcRowBytes = srcW;           break;
//     case 24: srcRowBytes = srcW * 3;       break;
//     case 32: srcRowBytes = srcW * 4;       break;
//   }
//   uint16_t rowBytes    = (srcRowBytes + 3) & ~3;
//   uint16_t brightBytes = (srcW + 7) / 8;

//   brightBuf = (uint8_t*)BufferManager::allocate(brightBytes);
//   if (!brightBuf) goto fail;

//   uint8_t chunkBuf[128];
//   uint8_t rowBuf[32];

//   Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START,
//                           ZX_SCREEN_ATTR_SIZE, COL::BLACK_WHITE);

//   int16_t lastBuiltFileRow = -1;

//   for (uint8_t i = 0; i < 192; i++) {
//     uint8_t dstY     = bottomUp ? (uint8_t)(191 - i) : i;
//     int16_t srcY_img = (int16_t)(((uint32_t)dstY * srcH) / 192);
//     int16_t fileRow  = bottomUp ? (int16_t)(srcH - 1 - srcY_img) : srcY_img;

//     if (fileRow != lastBuiltFileRow) {
//       uint32_t rowOffset = dataOffset + (uint32_t)fileRow * rowBytes;
//       if (!file.seekSet(rowOffset)) goto fail;
//       if (!readRowAsBrightBits(file, srcW, srcRowBytes, rowBytes, bitCnt,
//                                brightMap, brightBuf, chunkBuf,
//                                sizeof(chunkBuf))) {
//         goto fail;
//       }
//       lastBuiltFileRow = fileRow;

//       // Bresenham-map brightness bitmap -> 32-byte ZX row
//       for (uint8_t dB = 0; dB < 32; dB++) {
//         uint8_t outByte = 0;
//         for (uint8_t bit = 0; bit < 8; bit++) {
//           uint8_t  dstX = (uint8_t)((dB << 3) | bit);
//           uint16_t srcX = (uint16_t)(((uint32_t)dstX * srcW) / 256);
//           if (brightBuf[srcX >> 3] & (0x80 >> (srcX & 7)))
//             outByte |= (0x80 >> bit);
//         }
//         rowBuf[dB] = outByte;
//       }
//     }

//     Z80Bus::rleOptimisedTransfer(32, zx_spectrum_screen_address(0, dstY),
//                                  rowBuf, CMD_Copy);
//   }

//   file.close();
//   BufferManager::freeToMark(mark);
//   return true;

// fail:
//   file.close();
//   BufferManager::freeToMark(mark);
//   return false;
// }



void Utils::searchAndDisplayGraphics() {

  // WORKING ON IT!


  // // =========================================================================
  // // CONFIGURATION
  // // =========================================================================
  // constexpr uint8_t  SPRITE_WIDTH_BYTES  = 3;    // Sprite width in bytes (e.g., 2 = 16 pixels)
  // constexpr uint8_t  SPRITE_HEIGHT_LINES = 8;   // Sprite height in pixel lines
  // constexpr uint16_t BYTE_OFFSET        = 0;    // Start byte alignment shift
  // constexpr uint32_t PAUSE_MS           = 2000; // Pause per full screen page

  // constexpr uint16_t FETCH_CHUNK_SIZE   = 128;
  // uint8_t chunkBuffer[FETCH_CHUNK_SIZE];

  // uint32_t currentAddr = 0x5B00 + BYTE_OFFSET;

  // // Grid coordinates in pixels: X (0..255 in 8px / 1-byte steps), Y (0..191 pixel lines)
  // uint16_t gridX = 0;
  // uint16_t gridY = 0;

  // constexpr uint16_t spriteTotalBytes = SPRITE_WIDTH_BYTES * SPRITE_HEIGHT_LINES;
  // constexpr uint16_t spriteWidthPixels = SPRITE_WIDTH_BYTES * 8; // 8 pixels per byte
  // uint16_t spriteByteIdx = 0;

  // while (currentAddr <= (0xFFFF - FETCH_CHUNK_SIZE)) {
  //   // -----------------------------------------------------------------
  //   // PHASE 1: Fetch chunk into local RAM buffer
  //   // -----------------------------------------------------------------
  //   RequestSendDataPacket pkt(FETCH_CHUNK_SIZE, currentAddr);
  //   Z80Bus::sendBytes8((uint8_t*)&pkt, sizeof(RequestSendDataPacket));

  //   for (uint16_t i = 0; i < FETCH_CHUNK_SIZE; ++i) {
  //     chunkBuffer[i] = Z80Bus::get_IO_Byte();
  //   }

  //   // -----------------------------------------------------------------
  //   // PHASE 2: Render buffer to screen
  //   // -----------------------------------------------------------------
  //   for (uint16_t i = 0; i < FETCH_CHUNK_SIZE; ++i) {
  //     uint8_t b = chunkBuffer[i];

  //     // Relative byte offset inside the sprite (+8 pixels per X byte)
  //     uint16_t relX = (spriteByteIdx % SPRITE_WIDTH_BYTES) * 8;
  //     uint16_t relY = (spriteByteIdx / SPRITE_WIDTH_BYTES);

  //     uint16_t drawX = gridX + relX; // 0..255 pixels
  //     uint16_t drawY = gridY + relY; // 0..191 lines

  //     if (drawX < 256 && drawY < 192) {
  //       // Pass byte column (drawX / 8) or pixel X depending on your function signature
  //       uint16_t screenAddress = zx_spectrum_screen_address(drawX, drawY);
  //       CopyPacket drawPkt(screenAddress, 1);
  //       Z80Bus::sendBytes8((uint8_t*)&drawPkt, sizeof(CopyPacket));
  //       Z80Bus::sendBytes8(&b, 1);
  //     }

  //     spriteByteIdx++;

  //     // Full sprite completed? Advance grid position
  //     if (spriteByteIdx >= spriteTotalBytes) {
  //       spriteByteIdx = 0;
  //       gridX += spriteWidthPixels; // Advance X by sprite width (in 8px byte steps)

  //       // Hit right edge of screen? (256 pixels / 32 byte columns)
  //       if (gridX + spriteWidthPixels > 256) {
  //         gridX = 0;
  //         gridY += SPRITE_HEIGHT_LINES;

  //         // Hit bottom edge of screen? (192 pixel lines)
  //         if (gridY + SPRITE_HEIGHT_LINES > 192) {
  //           delay(PAUSE_MS); // PAUSE FOR 2 SECONDS PER FULL PAGE
  //           gridX = 0;
  //           gridY = 0;
  //         }
  //       }
  //     }
  //   }

  //   currentAddr += FETCH_CHUNK_SIZE;
  // }

  // delay(5000);
}




  // ======================
  // ARCHIVED CODE SECTION:
  // ======================

  // The 1-bit HALT-pulse protocol has been replaced as I've added additional
  // supporting hardware to the curcuit design allowing the OUT instruction to
  // be used instead.
  //
  // The HALT-pulse method is actually quite impressive - it's surprising how
  // well it works. It’s probably a bulletproof poor mans out method, since the
  // pulse timing makes it far less likely to be affected by interference - as
  // HALT stays active long enough to ride through most real-world noise.

  // // get16bitPulseValue: Used to get the command functions (addresses) from
  // the speccy.
  // // The speccy broadcasts all the functions at power up.
  // // uint16_t Utils::get16bitPulseValue() {
  // //   constexpr uint16_t BIT_TIMEOUT_US  = 70; //120+20;
  // //   uint16_t value = 0;
  // //   for (uint8_t i = 0; i < 16; i++) { // 16 bits, 2 bytes
  // //     uint8_t pulseCount = 0;
  // //     uint32_t lastPulseTime = 0;
  // //     while (1) {
  // //       // Service current HALT if active
  // //       if (digitalReadFast(Pin::Z80_HALT) == 0) {  // Waits for Z80 HALT
  // line to go HIGH
  // //         // Pulse the Z80's /NMI line: LOW -> HIGH to un-halt the CPU.
  // //         // digitalWriteFast(Pin::Z80_NMI, LOW);
  // //         // digitalWriteFast(Pin::Z80_NMI, HIGH);
  // //         Z80Bus::triggerZ80NMI();
  // //         pulseCount++;
  // //         lastPulseTime = micros();  // reset timer, allow another pulse
  // to be sampled
  // //       }
  // //       // Detect end marker delay at end of each single bit
  // //       if ((pulseCount > 0) && ((micros() - lastPulseTime) >
  // BIT_TIMEOUT_US )) {
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
  // //       if (digitalReadFast(Pin::Z80_HALT) == 0) {  // Waits for Z80 HALT
  // line to go HIGH
  // //         // Pulse the Z80's /NMI line: LOW -> HIGH to un-halt the CPU.
  // // //        digitalWriteFast(Pin::Z80_NMI, LOW);
  // // //        digitalWriteFast(Pin::Z80_NMI, HIGH);
  // //         Z80Bus::triggerZ80NMI();
  // //         pulseCount++;
  // //         lastPulseTime = micros();  // reset timer, allow another pulse
  // to be sampled
  // //       }
  // //       // Detect end marker delay at end of each single bit
  // //       if ((pulseCount > 0) && ((micros() - lastPulseTime) >
  // BIT_TIMEOUT_US )) {
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
  // void joinBits(byte* output, uint8_t input, uint16_t bitWidth, uint16_t
  // bitPosition) {
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

  
// void Utils::join6Bits(byte* output, uint8_t input, uint16_t bitPosition) {
//   uint16_t byteIndex = bitPosition >> 3;
//   uint8_t bitIndex = bitPosition & 7;
//   uint8_t maskedInput = input & 0x3F;

//   switch (bitIndex) {
//     case 0: 
//       output[byteIndex] |= (maskedInput << 2); 
//       break;
//     case 1: 
//       output[byteIndex] |= (maskedInput << 1); 
//       break;
//     case 2: 
//       output[byteIndex] |= maskedInput;        
//       break;
//     case 3: 
//       output[byteIndex]     |= (maskedInput >> 1); 
//       output[byteIndex + 1] |= (maskedInput << 7); 
//       break;
//     case 4: 
//       output[byteIndex]     |= (maskedInput >> 2); 
//       output[byteIndex + 1] |= (maskedInput << 6); 
//       break;
//     case 5: 
//       output[byteIndex]     |= (maskedInput >> 3); 
//       output[byteIndex + 1] |= (maskedInput << 5); 
//       break;
//     case 6: 
//       output[byteIndex]     |= (maskedInput >> 4); 
//       output[byteIndex + 1] |= (maskedInput << 4); 
//       break;
//     case 7: 
//       output[byteIndex]     |= (maskedInput >> 5); 
//       output[byteIndex + 1] |= (maskedInput << 3); 
//       break;
//   }
// }

  //=============================

  //
  // NOTES:
  // 1,000,000 microseconds to a second
  // T-States: 4 + 4 + 12 = 20 ish (NOP,dec,jr)
  // 1 T-state on a ZX Spectrum 48K is approximately 0.2857 microseconds.
  // 20 T-States / 0.285714 = 70 t-states
