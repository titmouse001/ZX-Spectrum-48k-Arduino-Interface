
#include <Arduino.h>
#include "utils.h"
#include "Pin.h" 

namespace Utils {
  
__attribute__((optimize("-Ofast"))) 
void memsetZero(byte* b, unsigned int len){
	for (; len != 0; len--) {
		*b++ = 0;
  }
}

__attribute__((optimize("-Ofast"))) 
void joinBits(byte* output, uint8_t input, uint16_t bitWidth, uint16_t bitPosition) {
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

// Joystick Support:
// The Arduino polls the joystick at 50 fps via a 74HC165 shift register.
// When the Spectrum performs an IN (C) (like LD C,$1F; IN D,(C)), the Arduino drives the Z80 data bus.
// A 74HC245D transceiver connects the Arduino to the Z80 and is tri-stated when not in use.
// This setup emulates a Kempston interface without bus conflicts.
uint8_t readJoystick() {
  // Enable shifting by pulling latch high
  PORTB |= (1 << PB2);    // Latch HIGH (enable shifting)
  PORTC |= (1 << PC2);    // Initial Clock HIGH

  uint8_t data = 0;
  for (uint8_t i = 0; i < 8; i++) {
    data <<= 1;
    if (PINB & (1 << PB1)) {
      data |= 1;
    }

    // Clock low-high transition
    PORTC &= ~(1 << PC2);       // Clock LOW
    __builtin_avr_delay_cycles(16); // 1microsecond at 16 MHz (fine-tuned delay)
    PORTC |= (1 << PC2);        // Clock HIGH
  }

  // Disable shifting by pulling latch low
  PORTB &= ~(1 << PB2);    // Latch LOW (disable shifting)
  return data;
}

void setupJoystick() {
  // Setup pins for "74HC165" shift register
  pinMode(Pin::ShiftRegDataPin, INPUT);
  pinMode(Pin::ShiftRegLatchPin, OUTPUT);
  pinMode(Pin::ShiftRegClockPin, OUTPUT);
}

}
