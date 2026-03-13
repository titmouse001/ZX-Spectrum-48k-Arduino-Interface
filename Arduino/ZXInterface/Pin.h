#ifndef PINS_H
#define PINS_H

#include <stdint.h>
#include "Arduino.h"

/* ====================================================
 * Pin Assignments: Arduino Nano to Z80 + Peripherals
 * ====================================================
 * NANO        |   Z80 + Support IC's
 * ----------------------------------------------------
 * pin 0-7    <->  Z80:D0-D7 [!] Disable Serial in code
 * pin 8/D8   <-   Z80:/HALT  (Z80 output pin 15)
 * pin 9/D9   <-   IC:74HC165 (SR-data out IC pin 7)
 * pin 10/10   ->  IC:74HC165 (SR-Latch pin 1)  
 * pin 11/D11  ->  SPI:SD MOSI
 * pin 12/D12 <-   SPI:SD MISO
 * pin 13/D13  ->  SPI:SD SCK
 * pin 14/A0   ->  Z80:/NMI   (Z80 input pin)
 * pin 15/A1   ->  ROM:pin 27 (ROM bank select)
 * pin 16/A2   ->  IC:74HC165 (SR-Clock IC pin 2)
 * pin 17/A3   ->  Z80:/RESET (Z80 input - active low)
 * pin 18/A4   ->  SD-CARD CS  (level shifter (74LVC125) IC pin-5)
 * pin 19/A5   ->  /OE latch for 74HC574 (support for Z80 'OUT' instructions)
 * pin 20/A6   -   Unused (slow analogue input)  - Was used for buttons via resistor ladder in past, but
 * pin 21/A7   -   Unused (slow analogue input)  - joystick Shift Register had spares and was cleaner path)
 */

namespace Pin {
  
//------------------------------------------------------------
// Z80 data bus D0–D7 (Arduino digital pins 0–7)
// Note: pins 0/1 also serve as RX/TX serial, so use Serial.begin() with care.
constexpr uint8_t Z80_D0Pin = 0;  // Arduino to z80 data (pin0,RX)
constexpr uint8_t Z80_D1Pin = 1;  // Arduino to z80 data (pin1,TX)
constexpr uint8_t Z80_D2Pin = 2;  // Arduino to z80 data (pin2)
constexpr uint8_t Z80_D3Pin = 3;  // Arduino to z80 data (pin3)
constexpr uint8_t Z80_D4Pin = 4;  // Arduino to z80 data (pin4)
constexpr uint8_t Z80_D5Pin = 5;  // Arduino to z80 data (pin5)
constexpr uint8_t Z80_D6Pin = 6;  // Arduino to z80 data (pin6)
constexpr uint8_t Z80_D7Pin = 7;  // Arduino to z80 data (pin7)

// Z80 control/status signals
constexpr uint8_t Z80_HALT = 8;   // Arduino pin8, PINB0 (PORT B) to Z80 'HALT' Status
constexpr uint8_t Z80_NMI = A0;   // pin14, PIN_A0 to Z80 NMI
constexpr uint8_t ROM_HALF = A1;  // pin15, PIN_A1 to ROM pin27 high/low bank select (sna or modified stock rom)
constexpr uint8_t Z80_REST = A3; // 17;  // PIN_A3 to the Z80 Reset line
constexpr uint8_t SD_CARD_CS = A4;
constexpr uint8_t OE_LATCH = A5;
//TODO ... A4  <->  SD-CARD CS    PCB NOW USES THIS PIN ... NEED TO UPDATE ALL COMMENTS
//TODO add  PIN_A5  now used by get_IO_Byte

constexpr uint8_t ledPin = 13;  // onboard LED shares SPI:SD SCK

// ----------------------------------------------------------------------------------------------------
// 74HC165 shift-register (parallel-in to serial-out)
// Used to read 8 digital inputs (joystick) using only 3 Arduino pins
// ----------------------------------------------------------------------------------------------------
// Arduino connections to 74HC165:
// - PB1 (Arduino pin 9)  <- QH (pin 9 on 74HC165): Serial data output (read input bits here)
// - PC2 (Arduino pin A2) -> CLK (pin 2 on 74HC165): Clock input (shifts out next bit on rising edge)
// - PB2 (Arduino pin 10) -> SH/LD (pin 1 on 74HC165): Shift/Load control (LOW to load inputs, HIGH to shift)
constexpr uint8_t ShiftRegDataPin  = 9;    // PB1: Serial data in from 74HC165 QH
constexpr uint8_t ShiftRegClockPin = A2;   // PC2: Clock output to 74HC165 CLK
constexpr uint8_t ShiftRegLatchPin = 10;   // PB2: Latch control to 74HC165 SH/LD
// ----------------------------------------------------------------------------------------------------
// Analog input  (see "pins_arduino.h" pulled in by "Arduino.h" )
// NO LONGER USED ...constexpr uint8_t BUTTON_PIN = A7;  // analog pin 7 

}

#endif
