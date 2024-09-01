#ifndef UTILS_H
#define UTILS_H

namespace MathUtils {
  void joinBits(byte* output,uint8_t input, uint16_t bitWidth, uint16_t bitPosition);
  byte reverseBits(byte data);
  void swap(byte &a, byte &b);
}

namespace StringUtils {
}

#endif // UTILS_H
