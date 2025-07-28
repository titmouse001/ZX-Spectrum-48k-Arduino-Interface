#ifndef UTILS_H
#define UTILS_H

namespace Utils {

constexpr uint16_t SCREEN_BASE = 0x4000;
constexpr uint8_t Ink7Paper0 = B01000111;
constexpr uint8_t JOYSTICK_MASK = B00111111;
constexpr uint8_t JOYSTICK_FIRE = B00100000;
constexpr uint8_t JOYSTICK_DOWN = B00000100;
constexpr uint8_t JOYSTICK_SELECT = B01000000;

/**
  * Setup the Fill Command
  *
  * @param buf       byte array with at least 6 bytes
  * @param amount    amount of memory to fill in bytes
  * @param address   16-bit destination address
  * @param value     fill value
  */
#define FILL_COMMAND(buf, amount, address, value) \
  do { \
    (buf)[0] = 'F'; \
    (buf)[1] = (uint8_t)((amount) >> 8); \
    (buf)[2] = (uint8_t)((amount)&0xFF); \
    (buf)[3] = (uint8_t)((address) >> 8); \
    (buf)[4] = (uint8_t)((address)&0xFF); \
    (buf)[5] = (uint8_t)(value); \
  } while (0)

#define SMALL_FILL_COMMAND(buf, amount, address, value) \
  do { \
    (buf)[0] = 'f'; \
    (buf)[1] = (uint8_t)(amount); \
    (buf)[2] = (uint8_t)((address) >> 8); \
    (buf)[3] = (uint8_t)((address)&0xFF); \
    (buf)[4] = (uint8_t)(value); \
  } while (0)


/** 
  * Build the packet header for a 16-bit command:
  * @param buf          byte array to write into
  * @param cmd          8-bit command letter
  * @param payloadSize  number of data bytes that follow (must fit in 8 bits)
  *
  * This macro writes the command letter and the payload length.
  */
#define START_UPLOAD_COMMAND(buf, cmd, payloadSize) \
  do { \
    (buf)[HEADER_TOKEN] = (uint8_t)(cmd); \
    (buf)[HEADER_PAYLOADSIZE] = (uint8_t)(payloadSize); \
  } while (0)

/** 
  * Insert a 16-bit address into the packet header:
  *  @param buf       byte array holding the command header
  *  @param address   16-bit target address (e.g., VRAM or register)
  *
  * This splits the 16-bit address into high and low bytes.
  */
#define ADDR_UPLOAD_COMMAND(buf, address) \
  do { \
    (buf)[HEADER_HIGH_BYTE] = (uint8_t)(((address) >> 8) & 0xFF); \
    (buf)[HEADER_LOW_BYTE] = (uint8_t)((address)&0xFF); \
  } while (0)

void memsetZero(byte* b, unsigned int len);
void join6Bits(byte* output, uint8_t input, uint16_t bitPosition);
//void joinBits(byte* output, uint8_t input, uint16_t bitWidth, uint16_t bitPosition);
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
  return SCREEN_BASE + section_part + interleave_part + (x >> 3); // x/8 gives 32 columns
}

}

#endif  // UTILS_H
