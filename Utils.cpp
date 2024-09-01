
#include <Arduino.h>
#include "utils.h"

namespace MathUtils {

void joinBits(byte* output, uint8_t input, uint16_t bitWidth, uint16_t bitPosition) {
  int byteIndex = bitPosition / 8;
  int bitIndex = bitPosition % 8;
  // Using a WORD to allow for a boundary crossing
  uint16_t alignedInput = (uint16_t)input << (8 - bitWidth);
  output[byteIndex] |= alignedInput >> bitIndex;
  if (bitIndex + bitWidth > 8) {  // spans across two bytes
    output[byteIndex + 1] |= alignedInput << (8 - bitIndex);
  }
}

byte reverseBits(byte data) {
  data = (data & 0xF0) >> 4 | (data & 0x0F) << 4;
  data = (data & 0xCC) >> 2 | (data & 0x33) << 2;
  data = (data & 0xAA) >> 1 | (data & 0x55) << 1;
  return data;
}

void swap(byte &a, byte &b) {
  byte temp = a;
  a = b;
  b = temp;
}

}
