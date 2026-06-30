#include "Debug.h"
#include "Constants.h"


#ifdef DEBUG_OLED
#warning "*** DEBUG_OLED enabled: Pin A4 conflict with SD card CS! ***"
//
// OLED Configuration:
//   - Uses I2C pins: A4 (SDA) & A5 (SCL)
//   - Display: 128x32 pixel SSD1306
//   - I2C Address: 0x3C (alternative: 0x3D)
//
// WARNING: Pin A4 is used by the SD card's chip select 
//          Maybe tie CS to gnd to get you by for debugging!
//
#include <Wire.h>                // I2C for OLED.
#include "SSD1306AsciiAvrI2c.h"  // SSD1306 OLED displays on AVR.
#include "fontdata.h"

#define I2C_ADDRESS 0x3C         // 0x3C or 0x3D
SSD1306AsciiAvrI2c oled;

// debugging with a 128x32 pixel oled

bool Debug::setupOled() {  // DEBUG USE ONLY
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


// ----------------------------------------------------------------------------------

#ifdef SERIAL_DEBUG
#warning "*** SERIAL_DEBUG enabled: Z80 Bus Comms will be BROKEN! ***"
// NOTE: D0/D1 are shared with the Z80 data bus.
// USE ONLY when debugging the Nano standalone via USB.
// !!! Connecting USB while the Z80 board is powered will mix two 5V power sources !!!
#include <SPI.h>

void Debug::setupSerial() {
    Serial.begin(9600);
    while (!Serial)  { };
    Serial.println("SERIAL DEBUG BREAKS COMMS TO Z80");
}
#endif
// ----------------------------------------------------------------------------------
