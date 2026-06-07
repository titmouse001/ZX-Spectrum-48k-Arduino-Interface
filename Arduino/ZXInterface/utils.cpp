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

  uint16_t mark = BufferManager::getMark();
  uint8_t* buf = BufferManager::allocate(sizeof(RequestSendDataPacket));

  // Speccy to send its screen data
  uint8_t len = PacketBuilder::build_Request_CommandSendData(buf, ZX_SCREEN_BITMAP_SIZE + ZX_SCREEN_ATTR_SIZE, 0x4000);
  Z80Bus::sendBytes(buf, len); // Sending z80 request to send bytes

// TODO - ******* replace me with optimised code please ********
// ...... This version of the code is for testing the get_IO_Byte by hammering it with this task.
// .......By saving a screen shot we can easly verify things work OK, is free testing!
  // FatFile &file = SdCardSupport::getFile();
  // if (file.isOpen()) { file.close(); }
  // FatFile &root = SdCardSupport::getRoot();
  // if (root.isOpen()) { root.close(); }

  FatFile &file = SdCardSupport::closeFileIfOpen(); 
  FatFile &root = SdCardSupport::closeRootIfOpen();

  if (root.open("/")) {
    file.open(filename, O_CREAT | O_WRONLY);
    for (uint16_t i = 0; i < ZX_SCREEN_BITMAP_SIZE + ZX_SCREEN_ATTR_SIZE; i++) {
      file.write(Z80Bus::get_IO_Byte());
    }
    file.close();
  }

  BufferManager::freeToMark(mark);

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
  
  // FatFile &file = SdCardSupport::getFile();
  // if (file.isOpen()) file.close();
  // FatFile &root = SdCardSupport::getRoot();
  // if (root.isOpen()) { root.close(); }

  FatFile &file = SdCardSupport::closeFileIfOpen(); 
  FatFile &root = SdCardSupport::closeRootIfOpen();

  if (root.open("/")) {
    if (file.open(filename, O_READ)) {

      const uint16_t mark = BufferManager::getMark();
      uint8_t *buf = BufferManager::allocate(FILE_READ_BUFFER_SIZE);
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


// ----------------------------------------------------------------------------------------------
// HARDWARE SYNC: /NMI TRIGGER LOGIC TO AVOID CORRUPTION
// ----------------------------------------------------------------------------------------------
// PCB UPDATE: Nano A2 monitors /RD and /IORQ via a passive resistor logic gate (4.7K per line). 
// This acts as a hardware AND gate for active-low signals: A2 only hits a Logic LOW when both
// Z80 lines are active, pinpointing the I/O Read cycle.
// WHY: Most games poll input (IN) when the stack is stable, so using NMI directly after seeing 
// a I/O read means the stack is unlikely to be hijacked.
//
// Using Inline ASM for cycle-accurate timing.
// Equivalent to the following logic:
//   while (digitalRead(A2) == HIGH); // Wait for I/O Read cycle
//   NMI_LOW(); NMI_HIGH();           // Pulse NMI immediately
//
// ----------------------------------------------------------------------------------------------

__attribute__((optimize("-Os"))) 
void Utils::waitForUserExit() {

  readJoystick(); // flush junk (Z80 rd/wr port 0x1f shared)

  uint8_t buttonData;
  while (true) {
    buttonData = Utils::readJoystick();
    PORTD = buttonData & JOYSTICK_MASK;  // Kempston joystick interface

    if (buttonData & JOYSTICK_SELECT) {
      pinModeFast(Pin::ShiftRegClockPin, INPUT);  // A2

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
     
      if (result == 5) {
        Utils::clearScreen(COL::BLACK_WHITE); 
       viewSpeccyMemory();
      }


      if (result == 4) {  // Screenshot
    
        Utils::clearScreen(COL::BLACK_WHITE); 
        Draw::text_P((ZX_SCREEN_WIDTH_PIXELS/2)-((6*17)/2), (ZX_SCREEN_HEIGHT_PIXELS/2)-4, F("Saving Screenshot"));
      
        exportScreenshot();
        while ((Menu::getButton() != Menu::BUTTON_NONE)) {}
      }
      Z80Bus::sendFillCommand( ZX_SCREEN_ATTR_ADDRESS_START, ZX_SCREEN_ATTR_SIZE, COL::BLACK_BLACK);  

      if (result == 6 ) { break; }  // back to main menu
      while ((Menu::getButton() != Menu::BUTTON_NONE)) {}  // wait for button let go
      
      restoreScreen(SCRATCH_FILE);

      restoreZ80States();
      readJoystick(); // flush junk

    }
  }
}


constexpr uint16_t PM_INITIAL_DELAY = 380; 
constexpr uint16_t PM_REPEAT_RATE = 120;
constexpr uint16_t PAUSE_XPOS = 80; 
constexpr uint8_t  PAUSE_YPOS_START = 48;
constexpr uint8_t  HIGHLIGHT_OFFSET = PAUSE_YPOS_START / 8;  // align highlighting with the "Resume" line
inline uint8_t getY(int8_t line) { return PAUSE_YPOS_START + (line * 8); }

__attribute__((optimize("-Os")))
uint8_t Utils::pauseMenu() {

  Utils::clearScreen(COL::BLACK_WHITE);
  Draw::text_P(PAUSE_XPOS, getY(-2), F("PAUSE MENU"));
  Draw::text_P(PAUSE_XPOS, getY(0), F("Resume"));                 // 0
  Draw::text_P(PAUSE_XPOS, getY(1), F("<todo>Save SNA"));         // 1
  Draw::text_P(PAUSE_XPOS, getY(2), F("<todo>SlowMo [normal]"));  // 2
  Draw::text_P(PAUSE_XPOS, getY(3), F("<todo>Cheats"));           // 3
  Draw::text_P(PAUSE_XPOS, getY(4), F("Screenshot"));             // 4
  Draw::text_P(PAUSE_XPOS, getY(5), F("Mem View"));               // 5
  Draw::text_P(PAUSE_XPOS, getY(6), F("Exit"));                   // 6

  uint8_t index = 0;
  uint16_t oldHighlightAddress = 0;
  unsigned long lastActionTime = 0;
  bool isRepeating = false;
  Menu::Button_t lastButton = Menu::BUTTON_NONE;
  while (true) {
    highlightSelection(index + HIGHLIGHT_OFFSET, 0, oldHighlightAddress); 
    Menu::Button_t currentButton = Menu::getButton();
    if (currentButton == Menu::BUTTON_NONE) {
      lastButton = Menu::BUTTON_NONE;
      isRepeating = false;
    } else {
      const unsigned long now = millis();
      const uint16_t delayThreshold = isRepeating ? PM_REPEAT_RATE : PM_INITIAL_DELAY;
      if (currentButton != lastButton || (now - lastActionTime) >= delayThreshold) {
        isRepeating = (currentButton == lastButton);  // Becomes true only on subsequent held triggers
        lastActionTime = now;
        lastButton = currentButton;
        if (currentButton == Menu::BUTTON_ADVANCE && index < 6) {
          index++;
        } else if (currentButton == Menu::BUTTON_BACK && index > 0) {
          index--;
        } else if (currentButton == Menu::BUTTON_MENU) {
          return index;
        }
      }
    }
    delay(1);
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


static inline char hexChar(uint8_t nibble) {
    nibble &= 0x0F;
    return nibble < 10 ? '0' + nibble : 'A' + (nibble - 10);
}

__attribute__((optimize("-Os"))) 
void Utils::viewSpeccyMemory() {
  constexpr uint8_t MAX_CHARS_PER_LINE = 42;  // (256pixels/6)
  constexpr uint8_t BYTES_TO_SHOW_PER_LINE = 8;
  int32_t currentBaseAddr = 0x5B00; // start of program mem
  uint16_t mark = BufferManager::getMark();
  uint8_t* buf = BufferManager::allocate(sizeof(RequestSendDataPacket));
  char* lineBuffer = (char*)BufferManager::allocate(MAX_CHARS_PER_LINE+1);

  memset(lineBuffer, '\0', MAX_CHARS_PER_LINE+1);

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

      uint8_t cmdLen = PacketBuilder::build_Request_CommandSendData(buf, BYTES_TO_SHOW_PER_LINE, tempBaseAddr);
      Z80Bus::sendBytes(buf, cmdLen);

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
        currentBaseAddr += ZX_SCREEN_HEIGHT_PIXELS; // 8 * 24;
        if (currentBaseAddr > 0xFFFF - ZX_SCREEN_HEIGHT_PIXELS ) {  
          currentBaseAddr =  0xFFFF - (ZX_SCREEN_HEIGHT_PIXELS-1);    // = 0xFF40
        }
        break;  // Redraw
      } else if (btn == Menu::BUTTON_BACK) {
        currentBaseAddr -= ZX_SCREEN_HEIGHT_PIXELS; // 8 * 24;
        if (currentBaseAddr<0) {
          currentBaseAddr=0;
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


constexpr char SHOTS[]   = "SHOTS";   // Folder name (5 chars) & filename prefix (4 chars)
constexpr char SCR_EXT[] = ".SCR";    // Extension

__attribute__((optimize("-Os"))) 
void Utils::exportScreenshot() {
  FatFile& root = SdCardSupport::getRoot();
  if (!root.isOpen() && !root.open("/")) {
    return;
  }

  // make sure "SHOTS" folder exists
  FatFile dir;
  if (!dir.open(&root, SHOTS, O_READ)) {
    if (!dir.mkdir(&root, SHOTS) || !dir.open(&root, SHOTS, O_READ)) {
      return;
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

