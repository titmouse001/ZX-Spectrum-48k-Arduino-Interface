#ifndef PINS_H
#define PINS_H

#ifdef HIGH
#undef HIGH  // can't use Arduino "0x1", need plain "1" for macros
#endif

#ifdef LOW
#undef LOW
#endif

//------------------------------------------------------------
// Pin Assignments: Arduino Nano to Z80 + Peripherals 
//------------------------------------------------------------

// pin 11 MOSI - used by SD CARD
// pin 12 MISO - used by SD CARD
// pin 13 SCK  - used by SD CARD
// pin 18 SDA - used by OLED
// pin 19 SCL - used by OLED
// pin 20 A6 - I'm Free!!!

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
static constexpr uint8_t ledPin        = 13;  // onboard LED (error indicator)

// 74HC165 shift-register (parallel-in -> serial-out)
static constexpr uint8_t ShiftRegDataPin = 9;    // Arduino pin9 to 74HC165 QH (pin-9 on chip)
static constexpr uint8_t ShiftRegClockPin = A2;  // pin16, connected to 74HC165 CLK (pin-2 on 165)
static constexpr uint8_t ShiftRegLatchPin = 10;  // Arduino pin10 to 74HC165 SH/LD (pin-1 on chip)

// Analog input  (see "pins_arduino.h" pulled in by "Arduino.h" )
static constexpr uint8_t BUTTON_PIN = A7;  // analog pin 7 (labeled 21)

/* Taken from pins_arduino.h for reference
#define PIN_A0   (14) defines these also... static const uint8_t A0 = PIN_A0;
#define PIN_A1   (15)
#define PIN_A2   (16)
#define PIN_A3   (17)
#define PIN_A4   (18)
#define PIN_A5   (19)
#define PIN_A6   (20)
#define PIN_A7   (21)
*/
//------------------------------------------------------------


// Bit support macros
#define HIGH 1
#define LOW 0

#define WRITE_BIT(reg, bit, val) WRITE_BIT_IMPL(reg, bit, val)

// Internal bit support - not for use
#define _WRITE_BIT_0(reg, bit) CLEAR_BIT(reg, bit)
#define _WRITE_BIT_1(reg, bit) SET_BIT(reg, bit)
#define WRITE_BIT_IMPL(reg, bit, val) _WRITE_BIT_##val(reg, bit)
#define SET_BIT(reg, bit) ((reg) |= _BV(bit))
#define CLEAR_BIT(reg, bit) ((reg) &= ~_BV(bit))

namespace NanoPins {

void setupShiftRegister() {
  // Setup pins for "74HC165" shift register
  pinMode(ShiftRegDataPin, INPUT);
  pinMode(ShiftRegLatchPin, OUTPUT);
  pinMode(ShiftRegClockPin, OUTPUT);
}

}

#endif


//static inline void writeBit(volatile uint8_t& reg, uint8_t b, bool v) {
//  if (v) reg |= _BV(b);
//  else   reg &= ~_BV(b);
//}
