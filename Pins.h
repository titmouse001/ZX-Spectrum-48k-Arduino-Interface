#ifndef PINS_H
#define PINS_H

#ifdef HIGH
#undef HIGH
#endif

#ifdef LOW
#undef LOW 
#endif

// 'Z80_D0Pin' to 'Z80_D7Pin'
const byte Z80_D0Pin = 0;          // Arduino to z80 data pins
const byte Z80_D1Pin = 1;          //  ""
const byte Z80_D2Pin = 2;          //  ""
const byte Z80_D3Pin = 3;          //  ""
const byte Z80_D4Pin = 4;          //  ""
const byte Z80_D5Pin = 5;          //  ""
const byte Z80_D6Pin = 6;          //  ""
const byte Z80_D7Pin = 7;          //  ""
const byte Z80_HALT = 8;           // PINB0 (PORT B), Z80 'Halt' Status
const byte ShiftRegDataPin = 9;    // connected to 74HC165 QH (pin-9 on chip)
const byte ShiftRegLatchPin = 10;  // connected to 74HC165 SH/LD (pin-1 on chip)
//                   pin 11 MOSI - SD CARD
//                   pin 12 MISO - SD CARD
//                   pin 13 SCK  - SD CARD
const byte ledPin = 13;  // only used for critical errors (flashes)
const byte Z80_NMI = 14;
const byte ROM_HALF = 15;
const byte ShiftRegClockPin = 16;  // connected to 74HC165 CLK (pin-2 on 165)
const byte Z80_REST = 17;
//                   pin 18 SDA - OLED
//                   pin 19 SCL - OLED

// Bit support macros
#define HIGH 1
#define LOW  0

#define WRITE_BIT(reg, bit, val)  WRITE_BIT_IMPL(reg, bit, val)

// Internal bit support - not for use
#define _WRITE_BIT_0(reg, bit) CLEAR_BIT(reg, bit)
#define _WRITE_BIT_1(reg, bit) SET_BIT(reg, bit)
#define WRITE_BIT_IMPL(reg, bit, val)  _WRITE_BIT_##val(reg, bit)
#define SET_BIT(reg, bit)    ((reg) |=  _BV(bit))
#define CLEAR_BIT(reg, bit)  ((reg) &= ~_BV(bit))

#endif