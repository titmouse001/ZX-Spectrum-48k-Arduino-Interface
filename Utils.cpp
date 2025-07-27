
#include <Arduino.h>
#include "utils.h"
#include "Pin.h" 
#include "digitalWriteFast.h"

namespace Utils {
  
__attribute__((optimize("-Ofast"))) 
void memsetZero(byte* b, unsigned int len){
	for (; len != 0; len--) {
		*b++ = 0;
  }
}

__attribute__((optimize("-Ofast"))) 
void join6Bits(byte* output, uint8_t input, uint16_t bitPosition) {
  constexpr uint8_t bitWidth = 6;
  uint16_t byteIndex = bitPosition >> 3;   // /8
  uint8_t bitIndex  = bitPosition & 7;    // %8
  uint8_t maskedInput = input & ((1U << bitWidth) - 1);
  uint16_t aligned = (uint16_t)maskedInput << (16 - bitWidth - bitIndex);
  uint8_t highByte = aligned >> 8;
  uint8_t lowByte = aligned;
  output[byteIndex] |= highByte;
  if (lowByte) {  
    output[byteIndex + 1] |= lowByte;
  }
}

__attribute__((optimize("-Ofast"))) 
void joinBitsxxx(byte* output, uint8_t input, uint16_t bitWidth, uint16_t bitPosition) {
  uint16_t byteIndex = bitPosition >> 3;   // /8
  uint8_t  bitIndex  = bitPosition & 7;    // %8
  // Using a WORD to allow for a boundary crossing
  uint16_t aligned = (uint16_t)input << (8 - bitWidth);
  output[byteIndex] |= aligned >> bitIndex;
  if (bitIndex + bitWidth > 8) {  // spans across two bytes
    output[byteIndex + 1] |= aligned << (8 - bitIndex);
  }
}

__attribute__((optimize("-Ofast"))) 
byte reverseBits(byte data) {
  data = (data & 0xF0) >> 4 | (data & 0x0F) << 4;
  data = (data & 0xCC) >> 2 | (data & 0x33) << 2;
  data = (data & 0xAA) >> 1 | (data & 0x55) << 1;
  return data;
}

__attribute__((optimize("-Ofast"))) 
void swap(byte &a, byte &b) {
  byte temp = a;
  a = b;
  b = temp;
}

constexpr unsigned long maxButtonInputMilliseconds = 1000 / 50;  

void frameDelay(unsigned long start) {
  const unsigned long timeSpent = millis() - start;
  if (timeSpent < maxButtonInputMilliseconds) {
    delay(maxButtonInputMilliseconds - timeSpent);  // aiming for 50 FPS
  }
}

// Kempston Joystick Support:
// 1. The Arduino polls the joystick at 50 FPS via a 74HC165 shift register.
// 2. When the Spectrum performs an IN (C) instruction (e.g. LD C,$1F; IN D,(C)), the Arduino drives the Z80 data bus.
//    A 74HC32 combines the /IORQ, /RD, and /A7 signals to detect when the joystick port is being accessed.
//    A 74HC245D transceiver connects the Arduino to the Z80 to send joystick data, and it is tri-stated when not in use.
__attribute__((optimize("-Ofast"))) 
uint8_t readJoystick() {
  PORTB |= (1 << PB2);      // Latch HIGH (enable shifting)
  PORTC |= (1 << PC2);      // Initial Clock HIGH
  uint8_t data = 0;
  for (uint8_t i = 0; i < 8; i++) {
    data <<= 1;
    if (PINB & (1 << PB1)) {
      data |= 1;
    }
    PORTC &= ~(1 << PC2);   // Clock LOW
//    __builtin_avr_delay_cycles(16); // 1microsecond at 16 MHz (fine-tuned delay)
    delayMicroseconds(16);
    PORTC |= (1 << PC2);    // Clock HIGH
  }
  PORTB &= ~(1 << PB2);     // Latch LOW (disable shifting)
  return data;              // read bits are "000FUDLR"
}

void setupJoystick() {
  // Setup pins for "74HC165" shift register
  pinModeFast(Pin::ShiftRegDataPin, INPUT);
  pinModeFast(Pin::ShiftRegLatchPin, OUTPUT);
  pinModeFast(Pin::ShiftRegClockPin, OUTPUT);
  Utils::readJoystick(); // call once to init - fudge
}

}
