#ifndef UTILS_H
#define UTILS_H

namespace Utils {

  /**
  * Setup the Fill Command
  *
  * @param buf       byte array with at least 6 bytes
  * @param amount    amount of memory to fill in bytes
  * @param address   16-bit destination address
  * @param value     fill value
  */
  #define FILL_8BIT_COMMAND(buf,amount,address,value) \
      do {                                                \
          (buf)[0] = 'F';                                 \
          (buf)[1] = (uint8_t)((amount)   >> 8);           \
          (buf)[2] = (uint8_t)((amount)   & 0xFF);         \
          (buf)[3] = (uint8_t)((address) >> 8);           \
          (buf)[4] = (uint8_t)((address) & 0xFF);         \
          (buf)[5] = (uint8_t)(value);                  \
      } while (0)

  /** 
  * Build the packet header for a 16-bit command:
  * @param buf          byte array to write into
  * @param cmd          8-bit command letter
  * @param payloadSize  number of data bytes that follow (must fit in 8 bits)
  *
  * This macro writes the command letter and the payload length.
  */
  #define ASSIGN_16BIT_COMMAND(buf, cmd, payloadSize) \
      do {                                        \
          (buf)[HEADER_TOKEN]       = (uint8_t)(cmd);          \
          (buf)[HEADER_PAYLOADSIZE] = (uint8_t)(payloadSize);  \
      } while (0)

  /** 
  * Insert a 16-bit address into the packet header:
  *  @param buf       byte array holding the command header
  *  @param address   16-bit target address (e.g., VRAM or register)
  *
  * This splits the 16-bit address into high and low bytes.
  */
  #define ADDR_16BIT_COMMAND(buf, address)     \
      do {                                                           \
          (buf)[HEADER_HIGH_BYTE] = (uint8_t)(((address) >> 8) & 0xFF); \
          (buf)[HEADER_LOW_BYTE]  = (uint8_t)((address)        & 0xFF); \
      } while (0)

  void memsetZero(byte* b, unsigned int len);
  void joinBits(byte* output,uint8_t input, uint16_t bitWidth, uint16_t bitPosition);
  byte reverseBits(byte data);
  void swap(byte &a, byte &b);
  uint16_t zx_spectrum_screen_address(uint8_t x, uint8_t y);
  uint16_t getSnaFileCount();
  void openFileByIndex(uint8_t searchIndex);
}

#endif // UTILS_H
