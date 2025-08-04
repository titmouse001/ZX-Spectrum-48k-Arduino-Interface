// -------------------------------------------------------------------------------------
// This is an Arduino-Based ZX Spectrum Game Loader - 2023/25 P.Overy
// -------------------------------------------------------------------------------------
// This software uses the Arduino's ATmega328P Nano (2K SRAM, 32K flash & 1K EEPROM)
// IMPORTANT: Do not modify PORTB directly without preserving the clock/crystal bits
// -------------------------------------------------------------------------------------

#include <Arduino.h>

#include "digitalWriteFast.h"
#include "utils.h"
#include "smallfont.h"
#include "pin.h"
#include "Z80Bus.h"
#include "Buffers.h"
#include "SdCardSupport.h"
#include "Draw.h"
#include "Menu.h"
#include "Constants.h"
#include "Z802SNA.h"
#include "Z80_Snapshot.h"

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
#warning "*** DEBUG_OLED is enabled ***"
#include <Wire.h>                 // I2C for OLED.
#include "SSD1306AsciiAvrI2c.h"   // SSD1306 OLED displays on AVR.
#define I2C_ADDRESS 0x3C          // 0x3C or 0x3D
SSD1306AsciiAvrI2c oled;
extern bool setupOled();          // debugging with a 128x32 pixel oled 
#endif
// ----------------------------------------------------------------------------------

void encodeSend(size_t input_len, uint16_t addr);
uint16_t GetValueFromPulseStream();


void setup() {

#if (SERIAL_DEBUG == 1)
  Serial.begin(9600);
  while (!Serial) {};                                                                 // Waits for serial monitor connection.
  Serial.println("DEBUG MODE - BREAKS Z80 TRANSFERS - ARDUINO SIDE DEBUGGING ONLY");  // Crucial warning for debug mode's impact on Z80.
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
  Buffers::setupFunctions();

  while (!SdCardSupport::init()) {  // Loops until SD card is successfully initialized.
    //oled.println("SD card failed");
    Z80Bus::fillScreenAttributes(Utils::Ink7Paper0);  // Sets default screen colors for error message.
    Draw::text(80, 90, PSTR("INSERT SD CARD"));             // Displays SD card prompt on Spectrum screen.
  }
}

void loop() {

  Z80Bus::fillScreenAttributes(Utils::Ink7Paper0);  // Resets screen colors for menu display.

  uint16_t totalFiles = 0;
  while ((totalFiles = SdCardSupport::countSnapshotFiles()) == 0) {  // Waits until snapshot files are found on SD.
    Draw::text(80, 90, PSTR("NO FILES FOUND"));                      // Displays error if no files found.
  }

  SdCardSupport::openFileByIndex(doFileMenu(totalFiles));  // Opens user-selected snapshot file via menu.

  FatFile& file = (SdCardSupport::file);
  if (file.available() && file.fileSize() == 6912) {
    Z80Bus::fillScreenAttributes(0);
    uint16_t currentAddress = 0x4000;
    while (file.available()) {
      byte bytesRead = (byte)file.read(&packetBuffer[5], COMMAND_PAYLOAD_SECTION_SIZE);
      Buffers::buildCopyCommand(packetBuffer, currentAddress, bytesRead);
      Z80Bus::sendBytes(packetBuffer, 5 + bytesRead);
      currentAddress += bytesRead;
    }

    SdCardSupport::fileClose();
    constexpr unsigned long maxButtonInputMilliseconds = 1000 / 50;
    while (getCommonButton() == 0) {
      delay(maxButtonInputMilliseconds);
    }

    // Clear screen before returning to menu
    Z80Bus::fillScreenAttributes(0);  
    uint16_t amount = 6144;
    uint16_t fillAddr = 0x4000;
    Buffers::buildFillCommand(packetBuffer, amount, fillAddr, 0);
    Z80Bus::sendBytes(packetBuffer, 7);

  } else {
    // *****************
    // *** SNA FILES ***
    // *****************
    if (file.fileSize() == SdCardSupport::SNAPSHOT_FILE_SIZE) {
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
      Buffers::setupFunctions();
    } else {
      // *****************
      // *** TXT FILES ***  ... not yet implemented
      // *****************
      char* fileName = (char*)&packetBuffer[5 + SmallFont::FNT_BUFFER_SIZE];
      /*uint16_t len = */ file.getName7(fileName, 64);
      char* dotPtr = strrchr(fileName, '.');
      char* extension = dotPtr + 1;
      if (strcmp(extension, "txt") == 0) {
        Z80Bus::fillScreenAttributes(B00010000);
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
          Buffers::setupFunctions();
        } else {  // .z80 file failed
          SdCardSupport::fileClose();
        }
      }
    }
  }
}

boolean bootFromSnapshot_z80_end() {
    Buffers::buildWaitCommand(packetBuffer);
    Z80Bus::sendBytes(packetBuffer, 2);
    Z80Bus::waitHalt();
    Buffers::buildExecuteCommand(head27_Execute);
    Z80Bus::sendSnaHeader(&head27_Execute[0]);
    Z80Bus::waitRelease_NMI();
    delayMicroseconds(7);
    digitalWriteFast(Pin::ROM_HALF, HIGH);
    while ((PINB & (1 << PINB0)) == 0) {};
    return true;
}

boolean bootFromSnapshot() {

    const uint16_t address = 0x4004;  // startup stack pointer (screen RAM)
    Buffers::buildStackCommand(packetBuffer, address);
    Z80Bus::sendBytes(packetBuffer, 4);
    Z80Bus::waitRelease_NMI();  //Synchronize: Speccy will halt after loading SP

    FatFile& file = (SdCardSupport::file);
    if (file.available()) {   
        byte bytesReadHeader = (byte)file.read(&head27_Execute[0 + 2], 27);  // Pre-load .sna 27-byte header (CPU registers)
        if (bytesReadHeader != 27) {
            file.close();
            Draw::text(80, 90, PSTR("Invalid sna file"));
            delay(3000);
            return false;
        }
    }
    
    uint16_t currentAddress = 0x4000;  // Spectrum RAM start
    while (file.available()) {        // Load .sna data into Speccy RAM in chunks
        uint16_t bytesRead = file.read(&packetBuffer[5], 255);
        encodeSend(bytesRead, currentAddress);
        currentAddress += bytesRead;
    }

    // Special syncronise case: Enable speccys hardware 50Hz maskable interrupt (i.e. "IM 1" , "EI")
    // We need to create a useful time gap before the next h/w interrupt by doing one now.
    // Why syncronise : About to restore code - having the interupt use the stack would be bad.
    Buffers::buildWaitCommand(packetBuffer);  // RENAME THIS TO "BuildWait_VBL_Command"
    Z80Bus::sendBytes(packetBuffer, 2);     
    Z80Bus::waitHalt();           

    Buffers::buildExecuteCommand(head27_Execute);
    Z80Bus::sendSnaHeader(&head27_Execute[0]);   // Restore registers & execute code (ROM)

    // Final HALT called from screen RAM 
    Z80Bus::waitRelease_NMI();    // Wait for speccy to signal ready

    // Speccy is in or returning from the final HALT generated interrupt (T-state duration:1/3.5MHz = 0.2857 microseconds)
    // (1) Push PC onto Stack: 10 T-states , (2) RETN Instruction: 14 T-states. (Total:~24 T-states)
    delayMicroseconds(7); // Safety gap for NMI timings (Around this point the game starts with "JP nnnn" from screen memory)

    digitalWriteFast(Pin::ROM_HALF, HIGH); // Switch to 16K stock ROM

    while ((PINB & (1 << PINB0)) == 0) {};  // Wait for speccy to start executing code

    return true;
}


static const uint16_t MAX_RUN_LENGTH = 255;  //TOTAL_PACKET_BUFFER_SIZE-(6+SIZE_OF_HEADER);;
static const uint16_t MAX_RAW_LENGTH = 255;
static const uint8_t MIN_RUN_LENGTH = 5;  // 5 is where RLE pays off

// RLE and send with - Transferring raw using 'G' and filling runs 'f' (both max send of 255 bytes)
// The Speccy will reconstruct this data on its end.
void encodeSend(/*const uint8_t* input,*/ uint16_t input_len, uint16_t addr) {

  uint8_t* input = &packetBuffer[5];

  if (input_len == 0) return;
  uint16_t i = 0;
  while (i < input_len) {
    uint8_t value = input[i];
    uint16_t remaining = input_len - i;
    uint16_t max_run = (remaining > MAX_RUN_LENGTH) ? MAX_RUN_LENGTH : remaining;
    uint16_t run_len = 1;

    // Check for run
    while (run_len < max_run && input[i + run_len] == value) {
      run_len++;
    }
    if (run_len >= MIN_RUN_LENGTH) {  // run found (with payoff)
      Buffers::buildSmallFillCommand(&packetBuffer[TOTAL_PACKET_BUFFER_SIZE-6],addr, run_len, value);
      Z80Bus::sendBytes(&packetBuffer[TOTAL_PACKET_BUFFER_SIZE - 6], 6);
      addr += run_len;
      i += run_len;
    } else {  // No run found - raw data
      uint16_t raw_start = i;
      uint16_t max_raw = (remaining > MAX_RAW_LENGTH) ? MAX_RAW_LENGTH : remaining;
      uint16_t raw_len = 1;  // We know current byte isn't part of a run
      i++;
      while (raw_len < max_raw && i < input_len) {
        if (raw_len + 1 < max_raw && input[i] == input[i + 1]) {
          break;  // stop - run found
        }
        raw_len++;
        i++;
      }
      uint8_t* p = &packetBuffer[raw_start];
      Buffers::buildTransferCommand(p, addr, raw_len);
      Z80Bus::sendBytes(p, 5 + raw_len);
      addr += raw_len;
    }
  }
}


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
