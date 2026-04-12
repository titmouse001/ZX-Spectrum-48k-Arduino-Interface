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



__attribute__((optimize("-Os"))) 
void Utils::waitForUserExit() {

  readJoystick(); // flush junk (Z80 rd/wr port 0x1f shared)

  uint8_t buttonData;
  while (true) {
    buttonData = Utils::readJoystick();
    PORTD = buttonData & JOYSTICK_MASK;  // Kempston joystick interface

    if (buttonData & JOYSTICK_SELECT) {
      pinModeFast(Pin::ShiftRegClockPin, INPUT);  // A2

      // ----------------------------------------------------------------------------------------------
      // HARDWARE SYNC: /NMI TRIGGER LOGIC TO AVOID CORRUPTION
      // ----------------------------------------------------------------------------------------------
      // PCB UPDATE: Nano A2 monitors /RD and /IORQ via a passive resistor logic gate (4.7K per line). 
      // This acts as a hardware AND gate for active-low signals: A2 only hits a Logic LOW when both
      // Z80 lines are active, pinpointing the I/O Read cycle.
      //
      // WHY: Most games poll for input (IN) once the game logic is stable. Firing the NMI here ensures the 
      // stack is unlikely to be hijacked, preventing the corruption from a interrupted stack-based memory move.
      // ----------------------------------------------------------------------------------------------

      // Using Inline ASM for cycle-accurate timing.
      // Equivalent to the following logic:
      //   while (digitalRead(A2) == HIGH); // Wait for I/O Read cycle
      //   NMI_LOW(); NMI_HIGH();           // Pulse NMI immediately
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

__attribute__((optimize("-Os"))) 
uint8_t Utils:: pauseMenu() {
  uint8_t index = 0;
  uint16_t oldHighlightAddress = 0;
  
  const uint16_t INITIAL_DELAY = 380; 
  const uint16_t REPEAT_RATE = 120;
  
  unsigned long lastActionTime = 0;
  bool isRepeating = false;
  Menu::Button_t lastButton = Menu::BUTTON_NONE;

  Utils::clearScreen(COL::BLACK_WHITE);
  Draw::text_P(80, 8 * 4, F("PAUSE MENU"));
  Draw::text_P(80, 8 * 6, F("Resume"));           // 0
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
          if (index == 0 || index == 4|| index == 5 || index == 6) break;  
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


static inline char hexChar(uint8_t nibble) {
    nibble &= 0x0F;
    return nibble < 10 ? '0' + nibble : 'A' + (nibble - 10);
}

__attribute__((optimize("-Os"))) 
void Utils::viewSpeccyMemory() {
  constexpr uint16_t START_ADDR = 0xB000;
  constexpr uint16_t LENGTH = (8*(192/8))/2;  // total bytes to read
  constexpr uint8_t BYTES_PER_LINE = 8;
  constexpr uint8_t LINE_WIDTH = 42;
  constexpr uint8_t LINES_PER_PAGE = LENGTH/8;  

  //uint8_t memDumpBuffer[LENGTH];
  uint16_t currentBaseAddr = START_ADDR;

 //static char lineBuffer[LINE_WIDTH + 1];

  while (true) {
      uint8_t cmdLen = PacketBuilder::build_Request_CommandSendData(
                     BufferManager::packetBuffer, LENGTH, currentBaseAddr);

    Z80Bus::sendBytes(BufferManager::packetBuffer, cmdLen);

   uint8_t* ptr =  &BufferManager::packetBuffer[ GLOBAL_MAX_PACKET_LEN+(7*32) ]; // hacked for now ... need to add light weight memory allocator

   //// TO FAR !!! char* lineBuffer =  &BufferManager::packetBuffer[GLOBAL_MAX_PACKET_LEN+(7*32) + LENGTH ]; //[cmdLen + LENGTH];

    for (uint16_t i = 0; i < LENGTH; ++i) {
    //  memDumpBuffer[i] = Z80Bus::get_IO_Byte();
        ptr[i] = Z80Bus::get_IO_Byte();
    }

    Utils::clearScreen(COL::BLACK_WHITE);

    uint8_t y = 0;
    for (uint16_t offset = 0; offset < LENGTH; offset += BYTES_PER_LINE) {

      // for now look into the whole shared buffer thing!!! need to find a better way !!!
      char lineBuffer[LINE_WIDTH + 1];

      memset(lineBuffer, ' ', LINE_WIDTH);
      uint8_t pos = 0;

      uint16_t addr = currentBaseAddr + offset;
      lineBuffer[pos++] = hexChar(addr >> 12);
      lineBuffer[pos++] = hexChar(addr >> 8);
      lineBuffer[pos++] = hexChar(addr >> 4);
      lineBuffer[pos++] = hexChar(addr);
      lineBuffer[pos++] = ':';
      lineBuffer[pos++] = ' ';

      for (uint8_t i = 0; i < BYTES_PER_LINE; ++i) {
        uint8_t b = ptr[offset + i];
        lineBuffer[pos++] = hexChar(b >> 4);
        lineBuffer[pos++] = hexChar(b & 0x0F);
        lineBuffer[pos++] = ' ';
      }

      for (uint8_t i = 0; i < BYTES_PER_LINE; ++i) {
        uint8_t c = ptr[offset + i];
        lineBuffer[pos++] = (c >= 32 && c <= 126) ? c : '.';
      }

      lineBuffer[LINE_WIDTH] = '\0';
      Draw::textLine(y, lineBuffer);
      y += 8; 
    }

    while (true) {
      Menu::Button_t btn = Menu::getButton();

      if (btn == Menu::BUTTON_ADVANCE) {
        currentBaseAddr += (LINES_PER_PAGE * BYTES_PER_LINE);
        while (Menu::getButton() != Menu::BUTTON_NONE) { delay(10); }  
        break;   // Redraw                                                       
      } else if (btn == Menu::BUTTON_BACK) {
        if (currentBaseAddr >= (LINES_PER_PAGE * BYTES_PER_LINE)) {
          currentBaseAddr -= (LINES_PER_PAGE * BYTES_PER_LINE);
        }
        while (Menu::getButton() != Menu::BUTTON_NONE) { delay(10); }
        break;  // Redraw
      } else if (btn == Menu::BUTTON_MENU) {
        while (Menu::getButton() != Menu::BUTTON_NONE) { delay(10); }
        return; // Exit
      }
    }
  }
}


constexpr char SHOTS[]   = "SHOTS";  // Does both folder [5] & filename [4]
constexpr char SCR_EXT[] = ".SCR";  // note: sizeof includes null

__attribute__((optimize("-Os"))) 
void Utils::exportScreenshot() {
  FatFile &root = SdCardSupport::getRoot();
  if (!root.isOpen()) {
    if (!root.open("/")) return;
  }

  FatFile dir;
  if (!dir.open(&root, SHOTS, O_READ)) {
    if (!dir.mkdir(&root, SHOTS)) return;
    if (!dir.open(&root, SHOTS, O_READ)) return;
  }

  char filename[13];
  filename[12] = '\0';
  memcpy(filename, SHOTS, sizeof(SHOTS)-2);          // "SHOT" 
  memcpy(filename + 8, SCR_EXT, sizeof(SCR_EXT)-1);    // ".SCR"

  FatFile file;
  for (uint16_t i = 1; i <= 9999; i++) {
    // avoiding sprintf as it eats Flash mem!!! 
    filename[4] = '0' + (i / 1000) % 10;
    filename[5] = '0' + (i / 100) % 10;
    filename[6] = '0' + (i / 10) % 10;
    filename[7] = '0' + i % 10;

    if (!file.open(&dir, filename, O_READ)) {
      break;  // no file so we have found next
    }
    file.close();
  }

  // place working scratch file -> screenshot file
  if (file.open(&root, SCRATCH_FILE, O_READ)) {
    FatFile destFile;
    if (destFile.open(&dir, filename, O_CREAT | O_WRONLY)) {
      uint8_t* buffer = BufferManager::packetBuffer;
      int bytesRead;
      while ((bytesRead = file.read(buffer, 64)) > 0) { 
        destFile.write(buffer, bytesRead);
      }
      destFile.close();
    }
    file.close();
  }
  dir.close();
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

