// -------------------------------------------------------------------------------------
// This is an Arduino-Based ZX Spectrum Game Loader - 2023/25 P.Overy
// ---------------------------------------------------------4----------------------------
// This software uses the Arduino's ATmega328P Nano (2K SRAM, 32K flash & 1K EEPROM)
// IMPORTANT: Do not modify PORTB directly without preserving the clock/crystal bits
// -------------------------------------------------------------------------------------

// Board detection - will cause compile error if wrong board selected
#if !defined(ARDUINO_AVR_NANO) && !defined(ARDUINO_AVR_UNO)
#error "This sketch is designed for Arduino Nano (ATmega328P). Please select the correct board in Tools > Board."
#endif

#if F_CPU != 16000000L
#warning "This sketch expects 16MHz clock speed."
#endif

#include <Arduino.h>
#include "digitalWriteFast.h"
#include "Z80_Snapshot.h"
#include "Menu.h"
#include "Utils.h"


#define VERSION ("0.22")  // Arduino firmware
#define DEBUG_OLED   0
#define SERIAL_DEBUG 0   

// ----------------------------------------------------------------------------------
#if (SERIAL_DEBUG == 1)
#warning "*** SERIAL_DEBUG is enabled ***"
// D0/D1 pins are shared with Z80 data bus. Enabling debug breaks Z80 communication.
#include <SPI.h>
#endif
// ----------------------------------------------------------------------------------
#if (DEBUG_OLED == 1)
#warning "*** DEBUG_OLED enabled: Pin A4 conflict with SD card CS! ***"
// 
// OLED Configuration:
//   - Uses I2C pins: A4 (SDA) & A5 (SCL)
//   - Display: 128x32 pixel SSD1306
//   - I2C Address: 0x3C (alternative: 0x3D)
//
// WARNING: Pin A4 serves dual purpose:
//   - OLED SDA (when DEBUG_OLED enabled)
//   - SD card Chip Select (in main code)
//   This creates a hardware conflict!
//
#include <Wire.h>                 // I2C for OLED.
#include "SSD1306AsciiAvrI2c.h"   // SSD1306 OLED displays on AVR.
#define I2C_ADDRESS 0x3C          // 0x3C or 0x3D
SSD1306AsciiAvrI2c oled;
extern bool setupOled();          // debugging with a 128x32 pixel oled 
#endif
// ----------------------------------------------------------------------------------

//void encodeTransferPacket(uint16_t input_len, uint16_t addr);

void setup() {

#if (SERIAL_DEBUG == 1)
  Serial.begin(9600);
  while (!Serial) {};                             
  Serial.println("DEBUG MODE BREAKS TRANSFERS");  
#endif

  Z80Bus::setupPins();     // Configures Arduino pins for Z80 bus interface.
  Utils::setupJoystick();  // Initializes joystick input pins.
  //setupOled();          
  // ---------------------------------------------------------------------------
  // *** Use stock ROM *** when select button or fire held at power up
  if (Utils::readJoystick() & (Utils::JOYSTICK_FIRE | Utils::JOYSTICK_SELECT)) {
    digitalWriteFast(Pin::ROM_HALF, HIGH);                            //Switches Spectrum to internal ROM.
    Z80Bus::resetZ80();                                               // Resets Z80 for a clean boot from internal ROM.
    while ((Utils::readJoystick() & Utils::JOYSTICK_SELECT) != 0) {}  // Debounces button release.
    while (true) {
      if (Utils::readJoystick() & Utils::JOYSTICK_SELECT) {
        digitalWriteFast(Pin::ROM_HALF, LOW);  //Switches back to Sna ROM.
        break;                                 // Returns to Sna loader.
      }
      delay(50);
    }
  }

  Z80Bus::resetZ80();
  //BufferManager::setupFunctions();
  CommandRegistry::initialize();

  Z80Bus::fillScreenAttributes(COL::BLACK_WHITE);
  Draw::text_P(256-24, 192-8, F(VERSION) );         

// !!! Whoops... 
// PIN CONFLICT: DSFat's init() defaults to pin 10 (PIN_SPI_SS) which conflicts with ShiftRegLatchPin
// used for button reading. This causes random button presses after startup when 
// SDFat disables the SD card. Fix: Use the free OLED debug pin A4.
  while (!SdCardSupport::init(PIN_A4)) { 
    //oled.println("SD card failed");
    Draw::text_P(80, 90, F("INSERT SD CARD")); 
  }
}

void loop() {

  Z80Bus::fillScreenAttributes(COL::BLACK_WHITE);  // Resets screen colors for menu display.

  uint16_t totalFiles = 0;
  while ((totalFiles = SdCardSupport::countSnapshotFiles()) == 0) {  // Waits until snapshot files are found on SD.
    Draw::text_P(80, 90, F("NO FILES FOUND"));                      // Displays error if no files found.
  }

  SdCardSupport::openFileByIndex(Menu::doFileMenu(totalFiles));  // Opens user-selected snapshot file via menu.

  FatFile& file = (SdCardSupport::file);
  // *****************
  // *** SCR FILES ***
  // *****************
  if (file.available() && file.fileSize() == ZX_SCREEN_TOTAL_SIZE) { // qualifies as a .SCR file
    Z80Bus::fillScreenAttributes(0);
    uint16_t currentAddress = ZX_SCREEN_ADDRESS_START;
    while (file.available()) {
// move BufferManager away from code this level ???
      byte bytesRead = (byte)file.read(&BufferManager::packetBuffer[E(CopyPacket::PACKET_LEN)], COMMAND_PAYLOAD_SECTION_SIZE);
      Z80Bus::sendCopyCommand(currentAddress, bytesRead);
      currentAddress += bytesRead;
    }

    SdCardSupport::fileClose();
    constexpr unsigned long maxButtonInputMilliseconds = 1000 / 50;
    while (Menu::getButton() == Menu::BUTTON_NONE) {
      delay(maxButtonInputMilliseconds);
    }

    // Clear screen before returning to menu
    Z80Bus::fillScreenAttributes(0);  
    Z80Bus::sendFillCommand(ZX_SCREEN_ADDRESS_START, ZX_SCREEN_BITMAP_SIZE, 0);
  } else {
    // *****************
    // *** SNA FILES ***
    // *****************
    if (file.fileSize() == SNAPSHOT_FILE_SIZE) {
      if (bootFromSnapshot()) {  // If not a screen dump, attempts to boot as a .SNA snapshot.
        SdCardSupport::fileClose();
        do {  // Loop to monitor Spectrum joystick inputs during game.
          unsigned long startTime = millis();
          PORTD = Utils::readJoystick() & Utils::JOYSTICK_MASK;           // Sends joystick state to Z80 via PORTD for Kempston emulation.
          Utils::frameDelay(startTime);                                   // Synchronizes joystick polling with Spectrum frame rate.
        } while ((Utils::readJoystick() & Utils::JOYSTICK_SELECT) == 0);  // Continues until SELECT is pressed (exit game).
        Z80Bus::resetToSnaRom();                                          // Resets Z80 and returns to snapshot loader ROM.
        while ((Utils::readJoystick() & Utils::JOYSTICK_SELECT) != 0) {}  // Waits for SELECT button release.
        delay(20);
      }
      //Buffers::setupFunctions();
      CommandRegistry::initialize();
    } else {
      // *****************
      // *** TXT FILES ***  ... not yet implemented
      // *****************
      char* fileName = (char*) &BufferManager::packetBuffer[FILE_READ_BUFFER_OFFSET];
      /*uint16_t len = */ file.getName7(fileName, 64);
      char* dotPtr = strrchr(fileName, '.');
      char* extension = dotPtr + 1;
      if (strcmp(extension, "txt") == 0) {
        Z80Bus::fillScreenAttributes(B00010000);
        delay(500);
        SdCardSupport::fileClose();  // back around to pick another file and don't reset speccy
      } else {
        // *****************
        // *** Z80 FILES ***
        // *****************
        if (convertZ80toSNA() == BLOCK_SUCCESS) {
          SdCardSupport::fileClose();
          bootFromSnapshot_z80_end();

          do {  // Loop to monitor Spectrum joystick inputs during game.
            unsigned long startTime = millis();
            PORTD = Utils::readJoystick() & Utils::JOYSTICK_MASK;           // Sends joystick state to Z80 via PORTD for Kempston emulation.
            Utils::frameDelay(startTime);                                   // Synchronizes joystick polling with Spectrum frame rate.
          } while ((Utils::readJoystick() & Utils::JOYSTICK_SELECT) == 0);  // Continues until SELECT is pressed (exit game).

          Z80Bus::resetToSnaRom();                                          // Resets Z80 and returns to snapshot loader ROM.
          while ((Utils::readJoystick() & Utils::JOYSTICK_SELECT) != 0) {}  // Waits for SELECT button release.
          //Buffers::setupFunctions();
          CommandRegistry::initialize();
        } else {  // .z80 file failed
          SdCardSupport::fileClose();
        }
      }
    }
  }
}

boolean bootFromSnapshot_z80_end() {
    
    uint8_t packetLen = PacketBuilder::buildWaitCommand(BufferManager::packetBuffer);
    Z80Bus::sendBytes(BufferManager::packetBuffer,packetLen);
    Z80Bus::waitHalt();

    PacketBuilder::buildExecuteCommand(BufferManager::head27_Execute);
    Z80Bus::sendSnaHeader(BufferManager::head27_Execute);
    Z80Bus::waitRelease_NMI();

    delayMicroseconds(7);
    digitalWriteFast(Pin::ROM_HALF, HIGH);

    while ((PINB & (1 << PINB0)) == 0) {};
    return true;
}

boolean bootFromSnapshot() {

    Z80Bus::sendStackCommand(ZX_SCREEN_ADDRESS_START+4);  // stack pointer (screen RAM)
    Z80Bus::waitRelease_NMI();  //Synchronize: Speccy will halt after loading SP

    FatFile& file = (SdCardSupport::file);
    if (file.available()) {   
        byte bytesReadHeader = (byte)file.read(&BufferManager::head27_Execute[E(ExecutePacket::PACKET_LEN)], SNA_TOTAL_ITEMS);  // Pre-load .sna 27-byte header (CPU registers)
        if (bytesReadHeader != SNA_TOTAL_ITEMS) {
            file.close();
            Draw::text_P(80, 90, F("Invalid sna file"));
            delay(3000);
            return false;
        }
    }
    
    uint16_t currentAddress = ZX_SCREEN_ADDRESS_START;  // Spectrum RAM start
    while (file.available()) {        // Load .sna data into Speccy RAM in chunks
        uint16_t bytesRead = file.read(&BufferManager::packetBuffer[E(TransferPacket::PACKET_LEN)], 255);
        Z80Bus::encodeTransferPacket(bytesRead, currentAddress);
        currentAddress += bytesRead;
    }

    // Special syncronise case: Enable speccys hardware 50Hz maskable interrupt (i.e. "IM 1" , "EI")
    // We need to create a useful time gap before the next h/w interrupt by doing one now.
    // Why syncronise : About to restore code - having the interupt use the stack would be bad.
    Z80Bus::sendWaitVBLCommand();
    Z80Bus::waitHalt();           

    PacketBuilder::buildExecuteCommand(BufferManager::head27_Execute);
    Z80Bus::sendSnaHeader(BufferManager::head27_Execute);   // Restore registers & execute code (ROM)

    // Final HALT called from screen RAM 
    Z80Bus::waitRelease_NMI();    // Wait for speccy to signal ready

    // Speccy is in or returning from the final HALT generated interrupt (T-state duration:1/3.5MHz = 0.2857 microseconds)
    // (1) Push PC onto Stack: 10 T-states , (2) RETN Instruction: 14 T-states. (Total:~24 T-states)
    delayMicroseconds(7); // Safety gap for NMI timings (Around this point the game starts with "JP nnnn" from screen memory)

    digitalWriteFast(Pin::ROM_HALF, HIGH); // Switch to 16K stock ROM

    while ((PINB & (1 << PINB0)) == 0) {};  // Wait for speccy to start executing code

    return true;
}



// // RLE and send with - TransferPacket and FillPacket (both send max of 255 bytes)
// // The Speccy will reconstruct the RLE at its end.
// void encodeTransferPacket(uint16_t input_len, uint16_t addr) {
//   constexpr uint16_t MAX_RUN_LENGTH = 255;
//   constexpr uint16_t MAX_RAW_LENGTH = 255;
//   constexpr uint8_t MIN_RUN_LENGTH = 5;  // about where RLE pays off over raw

//   uint8_t* input = &Buffers::packetBuffer[E(TransferPacket::PACKET_LEN)];

//   if (input_len == 0) return;
//   uint16_t i = 0;
//   while (i < input_len) {
//     uint8_t value = input[i];
//     uint16_t remaining = input_len - i;
//     uint16_t max_run = (remaining > MAX_RUN_LENGTH) ? MAX_RUN_LENGTH : remaining;
//     uint16_t run_len = 1;

//     // Check for run
//     while (run_len < max_run && input[i + run_len] == value) {
//       run_len++;
//     }
//     if (run_len >= MIN_RUN_LENGTH) {  // run found (with payoff)
//       uint8_t* pFill = &Buffers::packetBuffer[TOTAL_PACKET_BUFFER_SIZE - E(SmallFillPacket::PACKET_LEN)];      // send [PB-6] to [PB-1]
//       uint8_t packetLen = Buffers::buildSmallFillCommand(pFill, run_len,addr, value);
//       Z80Bus::sendBytes(pFill, packetLen);
    
//       addr += run_len;
//       i += run_len;
//     } else {  // No run found - raw data
//       uint16_t raw_start = i;
//       uint16_t max_raw = (remaining > MAX_RAW_LENGTH) ? MAX_RAW_LENGTH : remaining;
//       uint16_t raw_len = 1;  // We know current byte isn't part of a run
//       i++;
//       while (raw_len < max_raw && i < input_len) {
//         if (raw_len + 1 < max_raw && input[i] == input[i + 1]) {
//           break;  // stop - run found
//         }
//         raw_len++;
//         i++;
//       }
//       uint8_t* pTransfer = &Buffers::packetBuffer[raw_start];      // send [0] to [255]
//       uint8_t packetLen = Buffers::buildTransferCommand(pTransfer, addr, raw_len);
//       Z80Bus::sendBytes(pTransfer, packetLen + raw_len);
//       addr += raw_len;
//     }
//   }
// }

#if (DEBUG_OLED == 1)
// DEBUG USE ONLY
bool setupOled() {
  Wire.begin();
  Wire.beginTransmission(I2C_ADDRESS);
  bool result = (Wire.endTransmission() == 0);  // is OLED fitted
  Wire.end();

  if (result) {
    // Initialise OLED
    oled.begin(&Adafruit128x32, I2C_ADDRESS);   
    delay(1);
    // some hardware is slow to initialise, first call does not work.
    oled.begin(&Adafruit128x32, I2C_ADDRESS);
    // original Adafruit5x7 font with tweaks at start for VU meter
    oled.setFont(fudged_Adafruit5x7);
    oled.clear();
  // oled.print(F("ver"));
  // oled.println(F(VERSION));
  }
  return result;  // is OLED hardware available 
}
#endif


// -------------------------------------------------------------------------------------
// *** Some Useful Links ***
// ZX spectrum: https://mdfs.net/Docs/Comp/Spectrum/SpecIO
// Arduino    : https://devboards.info/boards/arduino-nano
//              https://arduino.stackexchange.com/questions/30968/how-do-interrupts-work-on-the-arduino-uno-and-similar-boards
// -------------------------------------------------------------------------------------



/* DEBUG EXAMPLE TO SPECTRUM SCREEN
uint16_t destAddr;
char _c[32];
for (uint8_t y = 0; y < 8; y++) {
  destAddr = Utils::zx_spectrum_screen_address(128, y);
  FILL_COMMAND(packetBuffer, 128 / 8, destAddr, 0x00);
  Z80Bus::sendBytes(packetBuffer, 6);
}
destAddr = 0x5800 + (128 / 8);
FILL_COMMAND(packetBuffer, 128 / 8, destAddr, B01000100);
Z80Bus::sendBytes(packetBuffer, 6);
sprintf(_c, "Delay:%d seconds", _delay / (1000 / 20));
Draw::text(256 - 128, 0, _c);
*/
