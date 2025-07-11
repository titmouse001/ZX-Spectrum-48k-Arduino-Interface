#ifndef PINS_H
#define PINS_H

//------------------------------------------------------------
// Pin Assignments: Arduino Nano to Z80 + Peripherals
/*
 * pin 0-7    <->  Z80:D0-D7 [!] Disable Serial in code
 * pin 8      <-   Z80:/HALT (input)
 * pin 9      <-   IC:74HC165 (data pin-7)
 * pin 10      ->  IC:74HC165 (latch pin-1)
 * pin 11      ->  SPI:SD MOSI
 * pin 12     <-   SPI:SD MISO
 * pin 13      ->  SPI:SD SCK
 * pin 14/A0   ->  Z80:/NMI 
 * pin 15/A1   ->  ROM:pin-27 (bank select)
 * pin 16/A2   ->  IC:74HC165 (clock pin-2)
 * pin 17/A3   ->  Z80:/RESET (active low)
 * pin 18/A4  <->  I2C:OLED SDA (data)
 * pin 19/A5   ->  I2C:OLED SCL (clock)
 * pin 20/A6   -   *** NC (unused) ***
 * pin 21/A7  <-   VOLTAGE: analog input
 * GND         -   SD_CS
 */
//------------------------------------------------------------

namespace Pin {

// Z80 data bus D0–D7 (Arduino digital pins 0–7)
// Note: pins 0/1 also serve as RX/TX serial, so use Serial.begin() with care.
static constexpr uint8_t Z80_D0Pin = 0;  // Arduino to z80 data (pin0,RX)
static constexpr uint8_t Z80_D1Pin = 1;  // Arduino to z80 data (pin1,TX)
static constexpr uint8_t Z80_D2Pin = 2;  // Arduino to z80 data (pin2)
static constexpr uint8_t Z80_D3Pin = 3;  // Arduino to z80 data (pin3)
static constexpr uint8_t Z80_D4Pin = 4;  // Arduino to z80 data (pin4)
static constexpr uint8_t Z80_D5Pin = 5;  // Arduino to z80 data (pin5)
static constexpr uint8_t Z80_D6Pin = 6;  // Arduino to z80 data (pin6)
static constexpr uint8_t Z80_D7Pin = 7;  // Arduino to z80 data (pin7)

// Z80 control/status signals
static constexpr uint8_t Z80_HALT = 8;   // Arduino pin8, PINB0 (PORT B) to Z80 'HALT' Status
static constexpr uint8_t Z80_NMI = A0;   // pin14, PIN_A0 to Z80 NMI
static constexpr uint8_t ROM_HALF = A1;  // pin15, PIN_A1 to ROM pin27 high/low bank select (sna or stock rom)
static constexpr uint8_t Z80_REST = 17;  // PIN_A3 to the Z80 Reset line

// Status LED
static constexpr uint8_t ledPin = 13;  // onboard LED (error indicator)

// ----------------------------------------------------------------------------------------------------
// 74HC165 shift-register (parallel-in to serial-out)
// Used to read 8 digital inputs (joystic) using only 3 Arduino pins
// ----------------------------------------------------------------------------------------------------
// Arduino connections to 74HC165:
// - PB1 (Arduino pin 9)  <- QH (pin 9 on 74HC165): Serial data output (read input bits here)
// - PC2 (Arduino pin A2) -> CLK (pin 2 on 74HC165): Clock input (shifts out next bit on rising edge)
// - PB2 (Arduino pin 10) -> SH/LD (pin 1 on 74HC165): Shift/Load control (LOW to load inputs, HIGH to shift)
static constexpr uint8_t ShiftRegDataPin  = 9;    // PB1: Serial data in from 74HC165 QH
static constexpr uint8_t ShiftRegClockPin = A2;   // PC2: Clock output to 74HC165 CLK
static constexpr uint8_t ShiftRegLatchPin = 10;   // PB2: Latch control to 74HC165 SH/LD
// ----------------------------------------------------------------------------------------------------


// Analog input  (see "pins_arduino.h" pulled in by "Arduino.h" )
static constexpr uint8_t BUTTON_PIN = A7;  // analog pin 7 (labeled 21)


// -----------------------------------------------------------
// Bit-level Write Macros for Direct Register Manipulation
// 
// Usage:
//   WRITE_BIT(PORTB, PB2, _HIGH); // Sets bit PB2 in PORTB
//   WRITE_BIT(PORTC, PC2, _LOW);  // Clears bit PC2 in PORTC

// Section replaced with digitalWriteFast.h 
//#define _HIGH 1
//#define _LOW 0
//#define WRITE_BIT(reg, bit, val) _INTERNAL_WRITE_BIT_##val(reg, bit)  // USE ME
//#define _INTERNAL_WRITE_BIT__HIGH(reg, bit) ((reg) |= _BV(bit))       // SET
//#define _INTERNAL_WRITE_BIT__LOW(reg, bit) ((reg) &= ~_BV(bit))       // CLEAR

}

#endif
