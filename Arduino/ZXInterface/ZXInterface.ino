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
#include "SnapZ80.h"
#include "Menu.h"
#include "Utils.h"
#include "Draw.h"
#include "SdCardSupport.h"

#include "CommandRegistry.h"
#include "PacketTypes.h"
#include "BufferManager.h"
#include "Z80Bus.h"

#define VERSION ("0.23")  // Arduino firmware
#define DEBUG_OLED 0
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
#include <Wire.h>                // I2C for OLED.
#include "SSD1306AsciiAvrI2c.h"  // SSD1306 OLED displays on AVR.
#define I2C_ADDRESS 0x3C         // 0x3C or 0x3D
SSD1306AsciiAvrI2c oled;
extern bool setupOled();  // debugging with a 128x32 pixel oled
#endif
// ----------------------------------------------------------------------------------

void setup() {

#if (SERIAL_DEBUG == 1)
  Serial.begin(9600);
  while (!Serial) {};
  Serial.println("DEBUG MODE BREAKS TRANSFERS");
#endif

  Z80Bus::setupPins();     // Configures Arduino pins for Z80 bus interface.
  Utils::setupJoystick();  // Initializes joystick input pins.
  //setupOled();           // For debugging
  // ---------------------------------------------------------------------------
  // *** Use stock ROM *** when select button or fire held at power up
  if (Utils::readJoystick() & (JOYSTICK_FIRE | JOYSTICK_SELECT)) {
    digitalWriteFast(Pin::ROM_HALF, HIGH);                     //Switches Spectrum to internal ROM.
    Z80Bus::resetZ80();                                        // Resets Z80 for a clean boot from internal ROM.
    while ((Utils::readJoystick() & JOYSTICK_SELECT) != 0) {}  // Debounces button release.
    while (true) {
      if (Utils::readJoystick() & JOYSTICK_SELECT) {
        digitalWriteFast(Pin::ROM_HALF, LOW);  //Switches back to Sna ROM.
        break;                                 // Returns to Sna loader.
      }
      delay(50);
    }
  }

  Z80Bus::resetZ80();
  CommandRegistry::initialize();
  Z80Bus::fillScreenAttributes(COL::BLACK_WHITE);
  Draw::text_P(256 - 24, 192 - 8, F(VERSION));

  // SD Init must eat a pin for CS! We use the free OLED debug pin A4 but we dont care about this pin (SD Card's CS pin is fix to GND).
  while (!SdCardSupport::init(PIN_A4)) {
    Draw::text_P(80, 90, F("INSERT SD CARD"));
  }
}

// ---------------------
// .SCR FILE 
// ---------------------
void handleScrFile() {
  Z80Bus::fillScreenAttributes(0);
  Z80Bus::clearScreen();
  Z80Bus::transferSnaData(false);  // No loading effects.
  SdCardSupport::fileClose();
  constexpr unsigned long maxButtonInputMilliseconds = 1000 / 50;
  while (Menu::getButton() == Menu::BUTTON_NONE) { delay(maxButtonInputMilliseconds); }
  Z80Bus::clearScreen();
}

// ---------------------
// .SNA FILE 
// ---------------------
void handleSnaFile() {
  uint8_t* snaPtr = &BufferManager::head27_Execute[E(ExecutePacket::PACKET_LEN)];
  SdCardSupport::fileRead(snaPtr, SNA_TOTAL_ITEMS);
  if (Z80Bus::bootFromSnapshot()) {
    SdCardSupport::fileClose();
    Utils::waitForUserExit();
    Z80Bus::resetToSnaRom();
    CommandRegistry::initialize();
  }
}

// ---------------------
// .Z80 FILE 
// ---------------------
void handleZ80File() {
  if (SnapZ80::convertZ80toSNA() == BLOCK_SUCCESS) {
    SdCardSupport::fileClose();
    Z80Bus::synchronizeForExecution();
    Z80Bus::executeSnapshot();
    Utils::waitForUserExit();
    Z80Bus::resetToSnaRom();
    CommandRegistry::initialize();
  } else {
    SdCardSupport::fileClose();
  }
}

// ---------------------
// .TXT FILE 
// ---------------------
void handleTxtFile() {
  const int maxCharsPerLine = ZX_SCREEN_WIDTH_PIXELS / SmallFont::FNT_CHAR_PITCH;
  const int charHeight = SmallFont::FNT_HEIGHT + SmallFont::FNT_GAP;
  const int maxLinesPerScreen = ZX_SCREEN_HEIGHT_PIXELS / charHeight;
  char* lineBuffer = (char*)&BufferManager::packetBuffer[FILE_READ_BUFFER_OFFSET];
  SdCardSupport::fileSeek(0);

  do {
    Z80Bus::clearScreen(COL::BLACK_WHITE);
    int currentLine = 0;
    while (currentLine < maxLinesPerScreen && SdCardSupport::fileAvailable()) {
      int bytesRead = 0;
      // Read characters until we hit the end of the line, a newline, or the buffer is full.
      while (bytesRead < maxCharsPerLine && SdCardSupport::fileAvailable()) {
        char c = SdCardSupport::file.read();
        if (c == '\r') {  //  Check for Windows-style CRLF
          if (SdCardSupport::fileAvailable() && SdCardSupport::file.peek() == '\n') {
            SdCardSupport::file.read();
          }
          break;  // End of line
        }
        if (c == '\n') { break; }                                 // Unix-style line ending
        if (c >= 32 && c < 127) { lineBuffer[bytesRead++] = c; }  // only ASCII
      }
      lineBuffer[bytesRead] = '\0';  // Null-terminate the string.
      if (bytesRead > 0) {
        Draw::textLine(currentLine * charHeight, lineBuffer);
      }
      currentLine++;
    }
    while (Menu::getButton() == Menu::BUTTON_NONE) {}
  } while (SdCardSupport::fileAvailable());

  SdCardSupport::fileClose();
}

void loop() {
  uint16_t totalFiles;
  while ((totalFiles = SdCardSupport::countSnapshotFiles()) == 0) {
    Z80Bus::clearScreen(COL::BLACK_WHITE);
    Draw::text_P(80, 90, F("NO FILES FOUND"));
  }

  uint16_t FileIndex = Menu::selectFileMenu(totalFiles);
  SdCardSupport::openFileByIndex(FileIndex);

  if (SdCardSupport::fileSize() == ZX_SCREEN_TOTAL_SIZE) {
    handleScrFile();
  } else if (SdCardSupport::fileSize() == SNAPSHOT_FILE_SIZE) {
    handleSnaFile();
  } else if (strcasestr(SdCardSupport::getFileName(), ".z80")) {
    handleZ80File();
  } else if (strcasestr(SdCardSupport::getFileName(), ".txt")) {
    handleTxtFile();
  } else {
    SdCardSupport::fileClose();
  }

  Z80Bus::fillScreenAttributes(COL::BLACK_WHITE);
}


#if (DEBUG_OLED == 1)
// DEBUG USE ONLY
bool setupOled() {
  Wire.begin();
  Wire.beginTransmission(I2C_ADDRESS);
  bool result = (Wire.endTransmission() == 0);  // is OLED fitted
  Wire.end();

  if (result) {
    oled.begin(&Adafruit128x32, I2C_ADDRESS);  // Initialise OLED
    delay(1);                                  // some hardware is slow to initialise, and first call may not work
    oled.begin(&Adafruit128x32, I2C_ADDRESS);
    oled.setFont(fudged_Adafruit5x7);  // original Adafruit5x7 font
    oled.clear();
    oled.print(F("ver"));
    oled.println(F(VERSION));
  }
  return result;
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
