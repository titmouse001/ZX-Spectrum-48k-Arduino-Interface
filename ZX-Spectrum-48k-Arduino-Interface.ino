// -------------------------------------------------------------------------------------
//  "Arduino Hardware Interface for ZX Spectrum" - 2023/24 P.Overy
// -------------------------------------------------------------------------------------
// This software uses the Arduino's ATmega328P range (Nano, Pro mini ... )
// for example here's the Nano specs:-
// - 32K bytes of in-system self-programmable flash program memory
// - 1K bytes EEPROM
// - 2K bytes internal SRAM
// IMPORTANT: Do not modify PORTB directly without preserving the clock/crystal bits.
// -------------------------------------------------------------------------------------
// SPEC OF SPECCY H/W PORTS HERE: - https://mdfs.net/Docs/Comp/Spectrum/SpecIO
// good links for Arduino info  :   https://devboards.info/boards/arduino-nano
//                                  https://arduino.stackexchange.com/questions/30968/how-do-interrupts-work-on-the-arduino-uno-and-similar-boards
// SNAPSHOT FILE HEADER (.sna files)
//  REG_I	 =00, REG_HL'=01, REG_DE'	=03, REG_BC' =05	(.sna file header)
//  REG_AF'=07, REG_HL =09, REG_DE	=11, REG_BC	 =13  (        27 bytes)
//  REG_IY =15, REG_IX =17, REG_IFF =19, REG_R	 =20
//  REG_AF =21, REG_SP =23, REG_IM	=25, REG_BDR =26
// -------------------------------------------------------------------------------------

#include <Arduino.h>

#include "utils.h"
#include "smallfont.h"
#include "pin.h"
#include "ScrSupport.h"
#include "Z80Bus.h"
#include "Buffers.h"
#include "SdCardSupport.h"
#include "Draw.h"
#include "Menu.h"

#include <Wire.h>  //  I2C devices
#include "SSD1306AsciiAvrI2c.h"  //  I2C displays, oled

// --------------------------------
#define SERIAL_DEBUG 0

#if SERIAL_DEBUG
  #warning "*** SERIAL_DEBUG is enabled"
  // Pins D0/D1 are shared between Serial RX/TX and the Z80 data bus.
  // This configuration will break Z80 communication. Enable SERIAL_DEBUG only for 
  // testing purposes and disable it in production builds.
  #include <SPI.h>  // Serial
#endif
// --------------------------------

#define VERSION ("0.14")

// ----------------------------------------------------------------------------------
// OLED support - *** now for debug ***
//#define I2C_ADDRESS 0x3C  // 0x3C or 0x3D
//SSD1306AsciiAvrI2c oled;
//bool haveOled = false;
// 

void setup() {

#if (SERIAL_DEBUG==1)
    Serial.begin(9600);
    while (! Serial) {};
    Serial.println("DEBUG MODE - BREAKS Z80 TRANSFERS - ARDUINO SIDE DEBUGGING ONLY");
#endif

  Z80Bus::setupNMI();
  Z80Bus::setupPins();
  Utils::setupJoystick();

  // BEBUG ONLY   setupOled();  // Optional OLED can be installed (128x32 pixel) 


  // ---------------------------------------------------------------------------
  // Use stock ROM (Select button held)
/*
  if (getAnalogButton() == BUTTON_SELECT) {
    Z80Bus::bankSwitchStockRom();
    Z80Bus::resetZ80();
    delay(1500);
    while(getAnalogButton() != BUTTON_SELECT) { delay(50); }
    // return to Sna loader rom
  }
*/
  // -----------------------------------------------------------------------------

  Z80Bus::resetToSnaRom();
  Z80Bus::fillScreenAttributes(Utils::Ink7Paper0);  // setup the whole screen

  while (!SdCardSupport::init()) {
    Draw::text(80, 90, "INSERT SD CARD");
  }

  //TODO ... add this to the menu 
  /*  
  if (getAnalogButton() == BUTTON_BACK) {
    ScrSupport::DemoScrFiles(root, file, packetBuffer);
  }
*/


}

void loop() {

  Z80Bus::resetToSnaRom();
  Z80Bus::fillScreenAttributes(Utils::Ink7Paper0);  // setup the whole screen

  uint16_t totalFiles=0;
  while ((totalFiles = SdCardSupport::countSnapshotFiles()) == 0) {
    Draw::text(80, 90, "NO FILES FOUND");
  }

  uint16_t fileIndex = doMenu(totalFiles);
  SdCardSupport::openFileByIndex(fileIndex);

  if (bootFromSnapshot()) {
    do {
      unsigned long startTime = millis();
      PORTD = Utils::readJoystick()&B00111111;  // send to the Z80 data lines (Kempston standard)
      Utils::frameDelay(startTime);
     } while ( (Utils::readJoystick()&B11000000) == 0 ); 
    while ((Utils::readJoystick()&B11000000) != 0) {} // wait for button release
  }
}

boolean bootFromSnapshot() {
  // Send the set "stack point" command.
  // This reuses the 2-byte jump address in screen memory's startup code,
  // which is unused until the final stage of execution.
  const uint16_t address = 0x4004;
  packetBuffer[0] = 'S';
  packetBuffer[1] = (uint8_t)(address >> 8);
  packetBuffer[2] = (uint8_t)(address & 0xFF);
  Z80Bus::sendBytes(packetBuffer, 3);
  Z80Bus::waitRelease_NMI();

  //-----------------------------------------
  // Read the Snapshots 27-byte header ahead of time
  if (file.available()) {
    head27_Execute[0] = 'E';
    byte bytesReadHeader = (byte)file.read(&head27_Execute[0 + 1], 27);
    if (bytesReadHeader != 27) {
        file.close();
        Draw::text(80, 90, "Invalid sna file");
        delay(3000);
        return false; // failed to load snapshot file
    }
  }
  //-----------------------------------------
  // Copy snapshot data into Speccy RAM (0x4000-0xFFFF)
  uint16_t currentAddress = 0x4000;  //starts at beginning of screen
  while (file.available()) {
    byte bytesRead = (byte)file.read(&packetBuffer[SIZE_OF_HEADER], PAYLOAD_BUFFER_SIZE);
    START_UPLOAD_COMMAND(packetBuffer, 'G', bytesRead);
    END_UPLOAD_COMMAND(packetBuffer, currentAddress);
    Z80Bus::sendBytes(packetBuffer, SIZE_OF_HEADER + (uint16_t)bytesRead);
    currentAddress += packetBuffer[HEADER_PAYLOADSIZE];
  }
  file.close();
  //-----------------------------------------
  // Command Speccy to wait for the next screen frefresh
  packetBuffer[0] = 'W';
  Z80Bus::sendBytes(packetBuffer, 1);
  //-----------------------------------------
  // The ZX Spectrum's maskable interrupt (triggered by the 50Hz screen refresh) will self release
  // the CPU from halt mode during the vertical blanking period.
  Z80Bus::waitHalt();  // wait for speccy to triggered the 50Hz maskable interrupt
  //-----------------------------------------
  Z80Bus::sendSnaHeader(&head27_Execute[0]);  // sends 27 commands allowing Z80 to patch together the Sna files saved register states.
  //-----------------------------------------
  // At this point the speccy is running code in screen memory to safely swap ROM banks.
  // A final HALT is given just before jumping back to the real snapshot code/game.
  Z80Bus::waitRelease_NMI();  // signal NMI line for the speccy to resume
  //-----------------------------------------
  // T-state duration:  1/3.5MHz = 0.2857μs
  // NMI Z80 timings -  Push PC onto Stack: ~10 or 11 T-states
  //                    RETN Instruction:    14 T-states.
  //                    Total:              ~24 T-states
  delayMicroseconds(7);  // (24×0.2857=6.857) wait for z80 to return to the main code.

  Z80Bus::bankSwitchStockRom();

  // At ths point rhe Spectrum's external ROM has switched to using the 16K stock ROM.
  // Wait for HALT line to return HIGH again (shows Z80 has resumed)
  while ((PINB & (1 << PINB0)) == 0) {};

  return true;
}

/*
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
    // original Adafruit5x7 font with tweeks at start for VU meter
    oled.setFont(fudged_Adafruit5x7);
    oled.clear();
    oled.print(F("ver"));
    oled.println(F(VERSION));
  }
  return result;  // is OLED hardware available 
}
*/


/*
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