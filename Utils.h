#ifndef UTILS_H
#define UTILS_H

#include "Constants.h"

namespace Utils {


constexpr uint8_t JOYSTICK_MASK = B00111111;
constexpr uint8_t JOYSTICK_FIRE = B00100000;
constexpr uint8_t JOYSTICK_DOWN = B00000100;
constexpr uint8_t JOYSTICK_SELECT = B01000000;


void memsetZero(byte* b, unsigned int len);
void join6Bits(byte* output, uint8_t input, uint16_t bitPosition);
void joinBits(byte* output, uint8_t input, uint16_t bitWidth, uint16_t bitPosition);
byte reverseBits(byte data);
void swap(byte& a, byte& b);
uint16_t getSnaFileCount();
void openFileByIndex(uint8_t searchIndex);
void frameDelay(unsigned long start);
uint8_t readJoystick(); 
void setupJoystick();
/*
 * The 256×192-pixel bitmap sits at 0x4000 in three 64-line bands (each 2048 bytes apart),
 * with each band storing the first scan-row of all eight 8-pixel blocks, 
 * then the second scan-row, etc. so that for any pixel (x,y) the byte address is computed as
 * address = 0x4000
 *          + (y >> 6) * 0x0800   // select 64-line section (0, 0x0800, or 0x1000)
 *          + ((y & 0x07) << 8)   // scan-row within 8-line block × 256 bytes
 *          + ((y & 0x38) << 2)   // block-row within section (bits 5–3) × 32 bytes
 *          + (x >> 3)            // horizontal byte index (x/8)
 */
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

}

#endif  // UTILS_H
