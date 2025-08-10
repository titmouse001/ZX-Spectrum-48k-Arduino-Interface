#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include "digitalWriteFast.h"
#include "Constants.h"
#include "Pin.h" 

namespace Utils {

__attribute__((optimize("-Ofast")))
inline uint16_t zx_spectrum_screen_address(uint8_t x, uint8_t y) {
  uint16_t section_part = uint16_t(y >> 6) * 0x0800;  //  upper/middle/lower 64 lines
  uint16_t interleave_part = ((y & 0x07) << 8) | ((y & 0x38) << 2);
  return ZX_SCREEN_ADDRESS_START + section_part + interleave_part + (x >> 3); // x/8 gives 32 columns
}

__attribute__((optimize("-Ofast")))
inline uint16_t zx_spectrum_screen_address(uint8_t y) {
  const uint16_t section_part = uint16_t(y >> 6) * 0x0800;  //  upper/middle/lower 64 lines
  uint16_t interleave_part = ((y & 0x07) << 8) | ((y & 0x38) << 2);
  return ZX_SCREEN_ADDRESS_START + section_part + interleave_part ; 
}

__attribute__((optimize("-Ofast"))) 
inline void memsetZero(byte* b, unsigned int len){
	for (; len != 0; len--) {
		*b++ = 0;
  }
}

__attribute__((optimize("-Ofast"))) 
inline void join6Bits(byte* output, uint8_t input, uint16_t bitPosition) {
  constexpr uint8_t bitWidth = 6;
  uint16_t byteIndex = bitPosition >> 3;   // /8
  uint8_t bitIndex  = bitPosition & 7;    // %8
  uint8_t maskedInput = input & ((1U << bitWidth) - 1);
  uint16_t aligned = (uint16_t)maskedInput << (16 - bitWidth - bitIndex);
  output[byteIndex] |= aligned >> 8;
  if (aligned) {  
    output[byteIndex + 1] |= aligned;
  }
}

void frameDelay(unsigned long start);
void setupJoystick();
uint8_t readJoystick();
uint16_t readPulseEncodedValue();

/* unused so far...
void joinBits(byte* output, uint8_t input, uint16_t bitWidth, uint16_t bitPosition);
byte reverseBits(byte data);
void swap(byte &a, byte &b);
*/

}  // namespace Utils

#endif 