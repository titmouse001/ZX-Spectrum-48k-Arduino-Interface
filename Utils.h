#ifndef UTILS_H
#define UTILS_H

namespace Utils {
  void joinBits(byte* output,uint8_t input, uint16_t bitWidth, uint16_t bitPosition);
  byte reverseBits(byte data);
  void swap(byte &a, byte &b);
  uint16_t getSnaFileCount();
  uint16_t zx_spectrum_screen_address(uint8_t x, uint8_t y);
}

#endif // UTILS_H
