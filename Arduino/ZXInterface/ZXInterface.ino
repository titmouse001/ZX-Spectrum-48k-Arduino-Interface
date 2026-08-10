// This is an Arduino-Based ZX Spectrum Game Loader (by Paul Overy)
//
// This software uses the Arduino's ATmega328P Nano (2K SRAM, 32K flash & 1K EEPROM)
// IMPORTANT: Do not modify PORTB directly without preserving the clock/crystal bits
//
// -------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------
// To upload via an external programmer (USBasp) instead of the onboard USB chip. 
// Useful if the USB/Serial chip fails, and saves flash memory (frees ~512B by removing bootloader).
// Quick start guide: https://www.youtube.com/watch?v=ToKerwRR-70
// Drivers (Windows): https://zadig.akeo.ie/ (Select USBasp device -> install 'WinUSB' driver)
//
// For clone boards with ATmega328PB (which fail under standard "Arduino AVR Boards" -> "Arduino Nano"):
// 1. Board Selection: Tools > Board > MiniCore -> ATmega328
// 2. MiniCore Options: 
//    - Variant: "328PB"
//    - Clock: "External 16MHz"
//    - BOD: "BOD 2.7V"
//    - EEPROM: "EEPROM retained"
//    - LTO: "Enabled"
// 3. Programmer: Tools > Programmer -> "USBasp"
// 4. Upload Code: Sketch > Upload Using Programmer (Do NOT use "Burn Bootloader" unless changing fuses)
// -------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------
// NOTE: USING CLONES OF CLONES (LGT8F328P LQFP32 MiniEVB)
// Issue with the  LGT8F328 + SdFat combination (others also seen this)
// The LGT's SPI hardware isn't a 100% register-compatible ATmega328P clone, and
// the standard SD.h works while SdFat fails on the exact same hardware.
// THIS CHANGE FIXED IT FOR ME:
//    #define SPI_DRIVER_SELECT 1
// This forces SdFat to use the standard Arduino SPI driver rather than its optimized native one.
// BUT HAD TO UPLOAD USING "Arduino IDE 2.3.10"  (VSC extension wth same config same build/upload but just failed to run!!!)
// FOR NOW AVOID USING PURPLE NANO LGT8F328 - looking like support on latest arduino nano setup is breaking something!!!
// -------------------------------------------------------------------------------------

// see README.MD for more info

//Sketch uses 27500 bytes (89%) of program storage space. Maximum is 30720 bytes.
//Global variables use 1392 bytes (67%) of dynamic memory, leaving 656 bytes for local variables. Maximum is 2048 bytes.
//Sketch uses 27804 bytes (90%) of program storage space. Maximum is 30720 bytes.
//Global variables use 1388 bytes (67%) of dynamic memory, leaving 660 bytes for local variables. Maximum is 2048 bytes.
//Sketch uses 28042 bytes (91%) of program storage space. Maximum is 30720 bytes.
//Global variables use 1388 bytes (67%) of dynamic memory, leaving 660 bytes for local variables. Maximum is 2048 bytes.

#include <Arduino.h>
//#include <avr/wdt.h>
//#include <EEPROM.h>

#include "Utils.h"
#include "Menu.h"
#include "InGamePauseMenu.h"
#include "Draw.h"
#include "SdCardSupport.h"
#include "BufferManager.h"
#include "Z80Bus.h"
#include "SnapZ80.h"
#include "PacketTypes.h"

void setup() {
  // *************************************************************************************
  // NOTE: To free up a pin, I removed the Arduino line used to reset the Z80 (/RESET). 
  // Because of this, things can now go out of sync between the Z80 and Arduino at start-up.
  // To get around this, the A3 (ROM Half) pin has now been tied to GND via a 10K pull-down resistor, 
  // so at power-up it defaults to LOW (SNA-ROM).
  // *************************************************************************************

  Z80Bus::setupPins();
  Utils::setupJoystick();

  // ----------------------------------------------------------------------
  // Attempt to sync with Speccy - Spectrum side has extra HALT at startup
  Z80Bus::waitHalt_syncWithZ80();
  // At this point HALT has been spotted by the Arduino and we carry on!
  // ----------------------------------------------------------------------
  //
  // WARNING: Pin defaults to LOW on power-up. Changing the pin to OUTPUT
  // generates a falling edge that triggers the Z80's edge-sensitive /NMI.
  pinModeFast(Pin::Z80_NMI, OUTPUT);
  //
  // Now we can go LOW/HIGH in a controlled pulse at start-up.
  digitalWriteFast(Pin::Z80_NMI, LOW);
  __asm__ __volatile__("nop\n\t nop\n\t");
  digitalWriteFast(Pin::Z80_NMI, HIGH);
  // ----------------------------------------------------------------------------------------

  // Use stock ROM when select button or fire held at power up
  if (Utils::readJoystick() & (INPUT_FIRE2 | INPUT_SELECT)) {
    Utils::stockRomBoot_Blocking();  // user pressing select again will exit
  }

  //  Utils::delay16(1);

  // Display the version (remove sd card to view version)
  Utils::clearScreen(COL::CYAN_BLACK);
  Draw::text_P(256 - 24, 192 - 8, F(VERSION));
  Utils::waitForSDCard_Blocking();  // When blocking shows - "INSERT SD CARD"
}

void loop() {
  
  FatFile* pFile = Menu::handleMenu();
  char extension[4];
  uint8_t amount =pFile->getExtension(extension, sizeof(extension));
  if (amount==3) {
    if (strcasecmp(extension, "scr") == 0) {
      handleScrFile(pFile);
    } else if (strcasecmp(extension, "sna") == 0) {
      handleSnaFile(pFile);
    } else if (strcasecmp(extension, "z80") == 0) {
      handleZ80File(pFile);
    } else if (strcasecmp(extension, "txt") == 0) {
      handleTxtFile(pFile);
    }
  }
  pFile->close();
}

// ---------------------
// .SCR FILE 
// ---------------------
void handleScrFile(FatFile* pFile) {
  Utils::clearScreen(0);
  if (pFile->fileSize() == ZX_SCREEN_TOTAL_SIZE) {
    Z80Bus::transferSnaData(pFile); 
    Menu::waitForRelease();
    Menu::waitForAnyKey();
    Utils::clearScreen(0);
  }
}

// -------------------------------------------------------------------------------------
// SHARED HELPER: Hijack the Z80 and restore the 3 bytes corrupted by the temporary stack
// 
// Note: We don't need the extra stage to monitor the Z80's combined /RD and /IORQ lines here 
// (see InGamePauseMenu::process), since the game has just been launched from executeSnapshot and 
// waits for the next VBL. We should be safe, hopefully still inside the 50Hz maskable interrupt routine, 
// as game code doesn't tend to abuse the stack there.
// -------------------------------------------------------------------------------------
void restoreCorruptedScreenBytes(const uint8_t* damagedScreenBytes) {

  delayMicroseconds(25);
  digitalWriteFast(Pin::Z80_NMI, LOW);      // NMI will loop spin for STOCK_ROM -> SNA_ROM swap
  digitalWriteFast(Pin::Z80_NMI, HIGH);
  delayMicroseconds(25);                    // Allow NMI routine time to reach its idle loop
  //
  // >>>  At this point we are now running the stock ROM  <<<
  //
  Z80Bus::setSnaRom();    // sna rom takes over loop spin with NOPs
  delayMicroseconds(25);    // Wait for Z80 to hit SNA ROM's '.IngameHook'
  Z80Registers* z80Registers = Utils::storeZ80States();

  // !!! Patch the 3-bytes of screen RAM we used used as working memory !!!
  MemoryPacket patchPkt(CMD_Copy, ZX_SCREEN_ADDRESS_START, 3);
  Z80Bus::sendBytes8((uint8_t*)&patchPkt, sizeof(patchPkt));
  Z80Bus::sendBytes8((uint8_t*)damagedScreenBytes, 3);

  Utils::restoreZ80States(z80Registers);
  delayMicroseconds(25);      // next idle loop so the stock ROM can take control

  // About to restore the stock ROM and continue game
  // We need to put something useful on the output pins for joystick port 0x1F.
  PORTD = Utils::readJoystick() & INPUT_MASK; 
  
  Z80Bus::setStockRom();      // again... stock rom escapes the idle loop via its NOPs
  delayMicroseconds(25);      // Let the stock rom catch up
//  Utils::readJoystick();    // flush junk
}


// -----------------------------------------------------------------------------------------------
// NOTES: Exiting the menu and restoring the snapshot (.sna/.z80). Before doing this, we must relocate the 
// Speccys Z80 stack to a safe working area. The menu defaults to 0xFFFF, which is a bad idea while loading a game.
// We set a temporary stack in screen memory (0x4000). This is safe because:
// - 0x4000 contains the Z80 "jp <patched-start-addr>" bootstrap instruction.       (JP = [1] byte)
// - We reuse 0x4003 for temporary 1-deep push/pop (at 0x4001/02).                  (STACK = [2] bytes)
// This temporary SP is later set by the load process to the games correct stack.
//
// We can safely (99.999%) set the SP at screen location during loading and simply sacrifice/corrupt [2] + [1] bytes.
// -----------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------
// .SNA FILE 
// -----------------------------------------------------------------------------------------------
void handleSnaFile(FatFile* pFile) {
  uint8_t borderColour;
   Utils::clearScreen(0);

  if (pFile->fileSize() == SNAPSHOT_FILE_SIZE) {
    // Set stack NOW before sending data over
    Z80Bus::setStackCommand(ZX_SCREEN_ADDRESS_START + 3);
    uint8_t mark = BufferManager::getMark();
    {
      Z80Registers* regs = (Z80Registers*)BufferManager::allocate(sizeof(Z80Registers));
      pFile->read((void*)&regs->header, sizeof(SNAHeader));
      regs->Z80Snapshot = false;

      //This damagedScreenBytes[] is used by restoreCorruptedScreenBytes()
      uint8_t damagedScreenBytes[3];
      pFile->read(damagedScreenBytes, 3);
      // pFile->seekSet(pFile->curPosition() - 3);

      constexpr bool ENABLE_LOADING_EFFECTS = true;
      constexpr uint8_t SKIP_STACK_BYTES = 3;
      Z80Bus::transferSnaData(pFile, ENABLE_LOADING_EFFECTS, SKIP_STACK_BYTES);



      // BUG HUNT ----> get_IO_Byte !!! AHHHHHHHHHHHHHHHHHHH

      // This is debugging code to test with and without get_IO_Byte
      // works if hand rolled without get_IO_Byte
      // this took ages to find, was not expecting get_IO_Byte to fail
      // think it could be timing thing, not had time to look at get_IO_Byte
      // will not use it for now for any new code!!! Need to setup at large automatated fuzzing test.
      // Was seeing get_IO_Byte when getting the PC from the stack like it was
      // offset ahead by three bytes (missed 3 reads).  This test code to pull down 16 bytes from 
      // speccy grabbed 13 from rom top then the other 3 from 0x0000 - 0x0002 rom!!!
      // Very odd bug - pain to find as get_IO_Byte has be reliable but I most have
      // some kind of H/W, S/W edge case.

  #if 0  // BEBUG
      Utils::clearScreen(COL::CYAN_BLACK);

     // PC is in the stack's game data - grab it
     // Z80 is little-endian, the stack holds the Low Byte first, followed by
      //the High Byte.
      Z80Registers* z80 = (Z80Registers*)snaHeaderPacket;
      uint16_t sp_addr = ((uint16_t)z80->sp_hi << 8) | (z80->sp_lo);

      sp_addr = 0xfff0;

      uint8_t data[16];
      // RequestSendDataPacket reqpkt(16, sp_addr);
      // Z80Bus::sendBytes((uint8_t*)&reqpkt, sizeof(RequestSendDataPacket));

      uint8_t cmd[6] = {cmd_addr(CMD_SendData) >> 8,
                        cmd_addr(CMD_SendData) & 0xff,
                        0,
                        16,
                        0xff,
                        0xf0};
      Z80Bus::sendBytes(cmd, 6);

      // Small delay buffer allowing hardware states to equalize
      Utils::delay16(1);

  #if 1
     for (uint16_t i = 0; i < 16; i++) {
      // IC timings/temperature ??!?!?!? 
      // To fix tried tweaking get_IO_Byte
      // Reverted the tweaks from get_IO_Byte to prove cdoe diffrence is working - but now it's working ok with old code ?!?!?!?! WTF
      // Left in tweaks on !!! Will need to keep an eye on this
      // maybe...current PCB I'm testing with does not have caps
      data[i]  = Z80Bus::get_IO_Byte();  
     }
  #else

      DDRD = 0x00;  // Set PORTD data pins to inputs
      digitalWriteFast(PIN_A5,
                       LOW);  // Enable output latch for reading bus data
      __asm__ __volatile__("nop; nop");

      // Stream data out of the Z80 bus directly onto the SD file stream
      for (uint16_t i = 0; i < 16; i++) {
        Z80Bus::waitHalt_syncWithZ80();
        data[i] = (uint8_t)PIND;  // Capture latched data from lines
        Z80Bus::triggerZ80NMI();
      }
      digitalWriteFast(PIN_A5, HIGH);  // Disable latch #OE line
      DDRD = 0xFF; 
  #endif

      char _c[8];
      ltoa(sp_addr, _c, 16);
      Draw::text(32, 32, _c);

      for (int i = 0; i < 16; i++) {
        itoa(data[i], _c, 16);
        Draw::text(128, (10 * i), _c);
      }
      delay(10000);
#endif

      Z80Bus::executeSnapshot(regs);
      borderColour =regs->header.borderCol;  // snaHeaderPacket[SNA_BORDER_COLOUR];
      restoreCorruptedScreenBytes(damagedScreenBytes);
    }
    BufferManager::freeToMark(mark);
    InGamePauseMenu::waitForUserExit(borderColour);
    Z80Bus::setSnaRom();
    // Z80Bus::resetZ80();
  }
}

// ---------------------
// .Z80 FILE 
// ---------------------
void handleZ80File(FatFile* pFile) {
  uint8_t borderColour;
  Utils::clearScreen(0);
  SnapZ80::Z80HeaderInfo headerInfo;
  uint8_t ver = readZ80Header(pFile, &headerInfo);
  
  if (ver != Z80_VERSION_UNKNOWN) {
    if (checkZ80FileValidity(pFile, &headerInfo)) {
      // Set stack NOW before sending data over
      Z80Bus::setStackCommand(ZX_SCREEN_ADDRESS_START + 3);  
      uint8_t mark = BufferManager::getMark();
      {
        Z80Registers* regs = (Z80Registers*) BufferManager::allocate(sizeof(Z80Registers));
        regs->Z80Snapshot = true;
        SnapZ80::convertSendZ80toSNA(pFile, &headerInfo, regs );
        SnapZ80::convertZ80HeaderToSna(&headerInfo.headerV1Data, regs);

        // Easer to scrape first 3 bytes from screen 0x4000, as above .z80 file data sent to convertSendZ80toSNA() is compressed!
        uint8_t damagedScreenBytes[3];
        RequestSendDataPacket pkt(sizeof(damagedScreenBytes), ZX_SCREEN_ADDRESS_START);
        Z80Bus::sendBytes((uint8_t*)&pkt, sizeof(RequestSendDataPacket));  // send request for N bytes of data
        damagedScreenBytes[0] = Z80Bus::get_IO_Byte();
        damagedScreenBytes[1] = Z80Bus::get_IO_Byte();
        damagedScreenBytes[2] = Z80Bus::get_IO_Byte();

        Z80Bus::executeSnapshot(regs);
        borderColour = regs->header.borderCol; // snaHeaderPacket[SNA_BORDER_COLOUR];
        restoreCorruptedScreenBytes(damagedScreenBytes);
      }
      BufferManager::freeToMark(mark);
      InGamePauseMenu::waitForUserExit(borderColour);
      Z80Bus::setSnaRom();
     // Z80Bus::resetZ80();
      return;  // load OK
    }
  }

  // drop down to report load error
  Utils::clearScreen(COL::BLACK_WHITE);
  Draw::text_P(0, 40, F("Can't load:"));

  char* buf = (char*)BufferManager::allocate(ZX_FILENAME_MAX_DISPLAY_LEN+1);
  uint8_t mark = BufferManager::getMark();
  pFile->getDisplayName7(buf,ZX_FILENAME_MAX_DISPLAY_LEN );
  Draw::text(0, 50,  buf);
  BufferManager::freeToMark(mark);

  if (SnapZ80::getMachineDetails(headerInfo.version, headerInfo.hw_mode) == MACHINE_128K) {
    Draw::text_P(80, 90, F("128K not supported"));
  }
  Menu::waitForAnyKey();
}


// ---------------------
// .TXT FILE
// ---------------------
void handleTxtFile(FatFile* pFile) {
  constexpr uint8_t charHeight = SmallFont::FNT_HEIGHT + SmallFont::FNT_GAP;
  constexpr uint8_t maxLines = ZX_SCREEN_HEIGHT_PIXELS / charHeight;
  constexpr uint8_t maxChars = ZX_SCREEN_WIDTH_PIXELS / SmallFont::FNT_CHAR_PITCH;

  uint16_t mark = BufferManager::getMark();
  char* buf = (char*)BufferManager::allocate(maxChars + 1);
  
  uint16_t currentPage = 0;
  uint32_t pageStartPos = 0; 

  while (true) {
    Utils::clearScreen(COL::BLACK_WHITE);
    pFile->seekSet(pageStartPos);
    
    uint8_t y = 0;
    for (uint8_t i = 0; i < maxLines; i++) {
      uint8_t len = 0;
      if (pFile->available()) {
        len = Utils::readLineTxt(pFile, buf, maxChars);
        if (!len) { buf[0] = ' '; len = 1; }
      }
      buf[len] = 0;
      Draw::textLine(y, buf);
      y += charHeight; // Faster/smaller than i * charH
    }

    uint32_t nextPos = pFile->curPosition();
    bool canFwd = pFile->available();
  
    Menu::waitForRelease();
    // Block until button press
    Menu::Button_t btn;
    while ((btn = Menu::getButton()) == Menu::BUTTON_NONE);

    if (btn == Menu::BUTTON_MENU) break;
    if (btn == Menu::BUTTON_ADVANCE && canFwd) {
      pageStartPos = nextPos;
      currentPage++;
    } 
    else if (btn == Menu::BUTTON_BACK && currentPage > 0) {
      currentPage--;
      pageStartPos = 0; // Re-scan logic
      for (uint16_t p = 0; p < currentPage; p++) {
        pFile->seekSet(pageStartPos);
        for (uint8_t l = 0; l < maxLines; l++) {
          Utils::readLineTxt(pFile, nullptr, maxChars); 
        }
        pageStartPos = pFile->curPosition();
      }
    }

    const uint32_t wait = millis() + 350;
    while (Menu::getButton() != Menu::BUTTON_NONE && millis() < wait);
  }

  BufferManager::freeToMark(mark);
  Menu::waitForRelease();
}



// -------------------------------------------------------------------------------------
// *** Some Useful Links ***
// ZX spectrum: https://mdfs.net/Docs/Comp/Spectrum/SpecIO
// Arduino    : https://devboards.info/boards/arduino-nano
//              https://arduino.stackexchange.com/questions/30968/how-do-interrupts-work-on-the-arduino-uno-and-similar-boards
// -------------------------------------------------------------------------------------

// Generate Map file
// >avr-nm -S --size-sort -t d C:\Users\Admin\Documents\GitHub\ZX-Spectrum-48k-Arduino-Interface\build.tmp\ZXInterface.ino.elf >c:\temp\2.txt





// Temp main code to transpose Adafruit5x7 font data into a 7x5 lookup table 
//
// #include <SPI.h>
// #include "src/fatlib/SdFat.h"
// #include "FontData.h" 

// SdFat SD;
// File file;

// void setup() {
  
//   file = SD.open("font_out.txt", O_WRITE | O_CREAT | O_TRUNC);
//   if (!file) {
//     return;
//   }

//   file.println("static const uint8_t __attribute__((progmem)) precalced_font5x7[] = {");
  
//   // Iterate through all 95 characters (0x20 to 0x7E)
//   for (int c = 0; c < 95; c++) {
//     // Read the 5 vertical columns for the current character[cite: 1]
//     uint8_t d0 = pgm_read_byte(&fudged_Adafruit5x7[c * 5 + 0]);
//     uint8_t d1 = pgm_read_byte(&fudged_Adafruit5x7[c * 5 + 1]);
//     uint8_t d2 = pgm_read_byte(&fudged_Adafruit5x7[c * 5 + 2]);
//     uint8_t d3 = pgm_read_byte(&fudged_Adafruit5x7[c * 5 + 3]);
//     uint8_t d4 = pgm_read_byte(&fudged_Adafruit5x7[c * 5 + 4]);

//     file.print("\t");
    
//     // Generate the 7 horizontal rows
//     for (int r = 0; r < 7; r++) {
//       // The original transposition logic[cite: 1]
//       uint8_t transposedRow = 
//           ((d0 & 1) << 4) | 
//           ((d1 & 1) << 3) | 
//           ((d2 & 1) << 2) | 
//           ((d3 & 1) << 1) | 
//            (d4 & 1);
      
//       // Write out in zero-padded hex format
//       file.print("0x");
//       if (transposedRow < 0x10) file.print("0");
//       file.print(transposedRow, HEX);
//       file.print(", ");
      
//       // Shift down to prepare for the next row[cite: 1]
//       d0 >>= 1; d1 >>= 1; d2 >>= 1; d3 >>= 1; d4 >>= 1;
//     }
    
//     // Append a comment identifying the character for easy code reading
//     file.print("// '");
//     file.print((char)(c + 0x20));
//     file.println("'");
//   }
  
//   file.println("};");
//   file.close();
//   Serial.println("Font successfully dumped to SD card!");
// }

// void loop() {
//   // Nothing to do here
// }