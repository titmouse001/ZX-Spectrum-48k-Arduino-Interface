
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

}
