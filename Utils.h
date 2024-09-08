#ifndef UTILS_H
#define UTILS_H

namespace Utils {
  void memsetZero(byte* b, unsigned int len);
  void joinBits(byte* output,uint8_t input, uint16_t bitWidth, uint16_t bitPosition);
  byte reverseBits(byte data);
  void swap(byte &a, byte &b);

  uint16_t zx_spectrum_screen_address(uint8_t x, uint8_t y);

  uint16_t getSnaFileCount();
  void openFileByIndex(uint8_t searchIndex);
}

#endif // UTILS_H
