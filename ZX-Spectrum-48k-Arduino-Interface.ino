// -------------------------------------------------------------------------------------
//"Arduino Hardware Interface for ZX Spectrum" - 2023/24 P.Overy
// -------------------------------------------------------------------------------------
// This software uses the Arduino's ATmega328P range (Nano, Pro mini ... )
// for example here's the Nano specs:-
// - 32K bytes of in-system self-programmable flash program memory
// - 1K bytes EEPROM
// - 2K bytes internal SRAM
//
// (IMPORTANT: Do not modify PORTB directly without preserving the clock/crystal bits)
// -------------------------------------------------------------------------------------
// SPEC OF SPECCY H/W PORTS HERE: - https://mdfs.net/Docs/Comp/Spectrum/SpecIO
// good links for Arduino info: https://devboards.info/boards/arduino-nano
//https://arduino.stackexchange.com/questions/30968/how-do-interrupts-work-on-the-arduino-uno-and-similar-boards
//
// SNAPSHOT FILE HEADER (27 bytes, no PC value stored):
//REG_I  =00, REG_HL'=01, REG_DE'=03, REG_BC'=05
//REG_AF'=07, REG_HL =09, REG_DE =11, REG_BC =13
//REG_IY =15, REG_IX =17, REG_IFF=19, REG_R  =20
//REG_AF =21, REG_SP =23, REG_IM =25, REG_BDR=26
// -------------------------------------------------------------------------------------

#include <Arduino.h>

#include "digitalWriteFast.h"  // Provides faster direct I/O, bypassing standard Arduino function overhead.
#include "utils.h"             // General utilities, including joystick input and timing.
#include "smallfont.h"         // Font data for display on OLED or Spectrum screen.
#include "pin.h"               // Centralized pin definitions for Spectrum bus lines.
#include "ScrSupport.h"        // Functions for direct interaction with Spectrum screen memory.
#include "Z80Bus.h"            // Core Z80 bus communication: data transfer and control signals.
#include "Buffers.h"           // Manages memory buffers for Arduino-Z80 data transfer.
#include "SdCardSupport.h"     // SD card initialization and file operations.
#include "Draw.h"              // Higher-level drawing primitives using ScrSupport.
#include "Menu.h"              // Menu system for file selection on the Spectrum display.

#include "Constants.h"   
#include "Z802SNA.h" 
#include "Z80_Snapshot.h"   

#include <Wire.h>                // I2C communication library for peripherals like OLED.
#include "SSD1306AsciiAvrI2c.h"  // Specific driver for SSD1306 OLED displays on AVR.

// --------------------------------
#define SERIAL_DEBUG 0  // Conditional compilation flag for serial debug output.

#if SERIAL_DEBUG
#warning "*** SERIAL_DEBUG is enabled"
// D0/D1 pins are shared with Z80 data bus. Enabling debug breaks Z80 communication.
#include <SPI.h>  // Included for potential SPI-based debugging peripherals.
#endif
// --------------------------------

#define VERSION ("0.14")  // Firmware version string.

// ----------------------------------------------------------------------------------
// OLED support removed - *** now for debug only ***
// Retained for potential developer debugging, not part of the main release.
#define I2C_ADDRESS 0x3C// 0x3C or 0x3D
SSD1306AsciiAvrI2c oled;
extern bool setupOled();
//bool haveOled = false;
//

void setup() {

#if (SERIAL_DEBUG == 1)
  Serial.begin(9600);
  while (!Serial) {};                                                                 // Waits for serial monitor connection.
  Serial.println("DEBUG MODE - BREAKS Z80 TRANSFERS - ARDUINO SIDE DEBUGGING ONLY");  // Crucial warning for debug mode's impact on Z80.
#endif

  Z80Bus::setupPins();     // Configures Arduino pins for Z80 bus interface.
  Utils::setupJoystick();  // Initializes joystick input pins.
  setupOled();             // ! DEBUG ONLY ! Optional OLED can be installed for dev debugging (128x32 pixel oled) // Optional OLED setup for development debugging.

  // ---------------------------------------------------------------------------
  // Use stock ROM- Select button or fire held at power up
  // Provides an escape hatch: boot Spectrum with its internal 16K ROM by holding joystick buttons.
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

  Z80Bus::resetZ80();  // Ensures Z80 is reset before SD card/snapshot operations.
  while (!SdCardSupport::init()) {  // Loops until SD card is successfully initialized.
    oled.println("SD card failed.");
    Z80Bus::fillScreenAttributes(Utils::Ink7Paper0);  // Sets default screen colors for error message.
    Draw::text(80, 90, "INSERT SD CARD");             // Displays SD card prompt on Spectrum screen.
  }
}

void loop() {
  // FatFile& root = (SdCardSupport::root);
  FatFile& file = (SdCardSupport::file);


  Z80Bus::fillScreenAttributes(Utils::Ink7Paper0);  // Resets screen colors for menu display.

  uint16_t totalFiles = 0;
  while ((totalFiles = SdCardSupport::countSnapshotFiles()) == 0) {  // Waits until snapshot files are found on SD.
    Draw::text(80, 90, "NO FILES FOUND");                            // Displays error if no files found.
  }

  SdCardSupport::openFileByIndex(doFileMenu(totalFiles));  // Opens user-selected snapshot file via menu.

  if (file.available() && file.fileSize() == 6912) {  // Checks if file is a standard 6912-byte raw screen dump.
    Z80Bus::fillScreenAttributes(0);                  // Clears screen attributes for direct screen data upload.
    uint16_t currentAddress = 0x4000;                 // Spectrum screen memory start.
    while (file.available()) {                        // Reads and sends file data in chunks to Spectrum.
      byte bytesRead = (byte)file.read(&packetBuffer[SIZE_OF_HEADER], COMMAND_PAYLOAD_SECTION_SIZE);
      START_UPLOAD_COMMAND(packetBuffer, 'C', bytesRead);           // Prepares 'Copy' command for Z80.
      END_UPLOAD_COMMAND(packetBuffer, currentAddress);             // Appends target Spectrum RAM address.
      Z80Bus::sendBytes(packetBuffer, SIZE_OF_HEADER + bytesRead);  // Transmits command and data.
      currentAddress += bytesRead;                                  // Advances target address.
    }
    file.close();

    while (getCommonButton() == 0) {}  // Waits for user input to view loaded screen.

    Z80Bus::fillScreenAttributes(0);  // Clears screen attributes again.
    currentAddress = 0x4000;
    constexpr uint16_t amount = 6144 + 768;          // Total size of Spectrum display memory.
    packetBuffer[0] = 'F';                           // 'Fill' command for Z80 memory operation.
    packetBuffer[1] = (amount >> 8) & 0xFF;          // High byte of fill amount.
    packetBuffer[2] = amount & 0xFF;                 // Low byte of fill amount.
    packetBuffer[3] = (currentAddress >> 8) & 0xFF;  // High byte of fill start address.
    packetBuffer[4] = currentAddress & 0xFF;         // Low byte of fill start address.
    packetBuffer[5] = 0;                             // Value to fill memory with (0 for black).
    Z80Bus::sendBytes(packetBuffer, 6);              // Sends fill command.

  } else {
    if (file.fileSize() == SdCardSupport::SNAPSHOT_FILE_SIZE) {
      if (bootFromSnapshot()) {  // If not a screen dump, attempts to boot as a .SNA snapshot.
        do {                     // Loop to monitor Spectrum joystick inputs during game.
          unsigned long startTime = millis();
          PORTD = Utils::readJoystick() & Utils::JOYSTICK_MASK;           // Sends joystick state to Z80 via PORTD for Kempston emulation.
          Utils::frameDelay(startTime);                                   // Synchronizes joystick polling with Spectrum frame rate.
        } while ((Utils::readJoystick() & Utils::JOYSTICK_SELECT) == 0);  // Continues until SELECT is pressed (exit game).
        Z80Bus::resetToSnaRom();                                          // Resets Z80 and returns to snapshot loader ROM.
        while ((Utils::readJoystick() & Utils::JOYSTICK_SELECT) != 0) {}  // Waits for SELECT button release.
      }
    } else {

      int a = convertZ80toSNA();
     	bootFromSnapshot_z80_end();
//      oled.print("END:");
//      oled.print(a);
      if (a == 0) {
        do {  // Loop to monitor Spectrum joystick inputs during game.
          unsigned long startTime = millis();
          PORTD = Utils::readJoystick() & Utils::JOYSTICK_MASK;           // Sends joystick state to Z80 via PORTD for Kempston emulation.
          Utils::frameDelay(startTime);                                   // Synchronizes joystick polling with Spectrum frame rate.
        } while ((Utils::readJoystick() & Utils::JOYSTICK_SELECT) == 0);  // Continues until SELECT is pressed (exit game).
        Z80Bus::resetToSnaRom();                                          // Resets Z80 and returns to snapshot loader ROM.
        while ((Utils::readJoystick() & Utils::JOYSTICK_SELECT) != 0) {}  // Waits for SELECT button release.
      }
    }
  }
}



boolean bootFromSnapshot_z80_end() {

  //-----------------------------------------
  // Wait for the next vertial blank to synchronize (Enables Interrupt Mode and Halts).
  packetBuffer[0] = 'W';                // 'W' command: "Wait for vertical blank" 
  Z80Bus::sendBytes(packetBuffer, 1);   // We just send the 1 character "W" command
  //-----------------------------------------
  // Special: Here we wait for the ZX Spectrum's vertical blank interrupt (triggered automatically by the 50Hz screen refresh)
  Z80Bus::waitHalt();  // Continue after vertial blank interrupt
  //-----------------------------------------
  // 'E' command - Load Registers & Executes code
  head27_Execute[0] = 'E';                    
  Z80Bus::sendSnaHeader(&head27_Execute[0]);  // Internally reorganizes registers to help direct loading by the Z80 itself.
  //-----------------------------------------
  // Now we wait for the speccy to signal it's ready with a last final HALT.
  Z80Bus::waitRelease_NMI();  // Synchronize: Speccy knows it must halt - Aruindo waits for NMI release.
  // At this point, synchronization is confirmed and we know the Speccy is executing code 
  // from screen memory, so it's now safe to swap ROM banks.
  //-----------------------------------------
  // Allow a safety gap in the timings (NMI Z80 timings)
  //  Speccy is in or returning from the interrupt (T-state duration:1/3.5MHz = 0.2857 microseconds)
  //  (1) Push PC onto Stack: ~10 or 11 T-states , (2) RETN Instruction: 14 T-states. (Total:~24 T-states)
  delayMicroseconds(7);  // (24×0.2857=6.857) delay for NMI routine completion.
  // At this pint (or sooner) the Speccy does it's last start-up sequence instruction "JP nnnn" to actual game code.
  //-----------------------------------------
  // Switch to 16K stock ROM (still external, see notes on why)
  digitalWriteFast(Pin::ROM_HALF, HIGH);  
  //-----------------------------------------
  // Wait for the speccy to start executing code
  while ((PINB & (1 << PINB0)) == 0) {};  // Waits for Z80 HALT line to go HIGH, confirming snapshot execution resumed.
  //
  // At this point the ZX Spectrum should be happly running the newly loaded snapshot code.
  return true;  // Snapshot boot successful.
}


boolean bootFromSnapshot() {
  //FatFile& root = (SdCardSupport::root);
  FatFile& file = (SdCardSupport::file);


  // Send the set stack point command.
  // Reuses a jump address in screen memory, unused until final execution.
  const uint16_t address = 0x4004;  // Temporary Z80 jump target in screen memory.
  packetBuffer[0] = 'S';            // 'S' command: Set Stack Pointer (SP) on Z80.
  packetBuffer[1] = (uint8_t)(address >> 8);
  packetBuffer[2] = (uint8_t)(address & 0xFF);
  Z80Bus::sendBytes(packetBuffer, 3); // 3 = character command + 16bit address
  Z80Bus::waitRelease_NMI();  //Synchronize: Z80 knows it must halt after loading SP - Aruindo waits for NMI release.

  //-----------------------------------------
  // Pe-load .sna 27-byte header (CPU registers)
  if (file.available()) {
    byte bytesReadHeader = (byte)file.read(&head27_Execute[0 + 1], 27);  // +1 leave room for command i.e. "E"
    if (bytesReadHeader != 27) {                                        
      file.close();
      Draw::text(80, 90, "Invalid sna file");
      delay(3000);
      return false;  // Failed snapshot load.
    }
  }
  //-----------------------------------------
  // Load .sna data into Speccy RAM (0x4000–0xFFFF) in chunks of COMMAND_PAYLOAD_SECTION_SIZE bytes.
  uint16_t currentAddress = 0x4000;  // Spectrum user RAM start.
  while (file.available()) {
    byte bytesRead = (byte)file.read(&packetBuffer[SIZE_OF_HEADER], COMMAND_PAYLOAD_SECTION_SIZE);
    START_UPLOAD_COMMAND(packetBuffer, 'G', bytesRead);  // 'G' command: Generic data upload to Z80.
    END_UPLOAD_COMMAND(packetBuffer, currentAddress);    // Specifies destination address in Spectrum RAM.
    Z80Bus::sendBytes(packetBuffer, SIZE_OF_HEADER + (uint16_t)bytesRead);
    currentAddress += packetBuffer[HEADER_PAYLOADSIZE];  // Advances target address.
  }
  file.close();
  //-----------------------------------------
  // Wait for the next vertial blank to synchronize (Enables Interrupt Mode and Halts).
  packetBuffer[0] = 'W';                // 'W' command: "Wait for vertical blank" 
  Z80Bus::sendBytes(packetBuffer, 1);   // We just send the 1 character "W" command
  //-----------------------------------------
  // Special: Here we wait for the ZX Spectrum's vertical blank interrupt (triggered automatically by the 50Hz screen refresh)
  Z80Bus::waitHalt();  // Continue after vertial blank interrupt
  //-----------------------------------------
  // 'E' command - Load Registers & Executes code
  head27_Execute[0] = 'E';                    
  Z80Bus::sendSnaHeader(&head27_Execute[0]);  // Internally reorganizes registers to help direct loading by the Z80 itself.
  //-----------------------------------------
  // Now we wait for the speccy to signal it's ready with a last final HALT.
  Z80Bus::waitRelease_NMI();  // Synchronize: Speccy knows it must halt - Aruindo waits for NMI release.
  // At this point, synchronization is confirmed and we know the Speccy is executing code 
  // from screen memory, so it's now safe to swap ROM banks.
  //-----------------------------------------
  // Allow a safety gap in the timings (NMI Z80 timings)
  //  Speccy is in or returning from the interrupt (T-state duration:1/3.5MHz = 0.2857 microseconds)
  //  (1) Push PC onto Stack: ~10 or 11 T-states , (2) RETN Instruction: 14 T-states. (Total:~24 T-states)
  delayMicroseconds(7);  // (24×0.2857=6.857) delay for NMI routine completion.
  // At this pint (or sooner) the Speccy does it's last start-up sequence instruction "JP nnnn" to actual game code.
  //-----------------------------------------
  // Switch to 16K stock ROM (still external, see notes on why)
  digitalWriteFast(Pin::ROM_HALF, HIGH);  
  //-----------------------------------------
  // Wait for the speccy to start executing code
  while ((PINB & (1 << PINB0)) == 0) {};  // Waits for Z80 HALT line to go HIGH, confirming snapshot execution resumed.
  //
  // At this point the ZX Spectrum should be happly running the newly loaded snapshot code.
  return true;  // Snapshot boot successful.
}



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









