
#include <Arduino.h>
#include "utils.h"
#include "SdFat.h" 

extern FatFile file;
extern FatFile root;

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



uint8_t readJoystick() {
  bitSet(PORTB, DDB2);  // HIGH, pin 10, disable input latching/enable shifting
  bitSet(PORTC, DDC2);  // HIGH, pin 16, clock to retrieve first bit

  byte data = 0;
  // Read the data byte using shiftIn replacement with direct port manipulation
  for (uint8_t i = 0; i < 8; i++) {  // only need first 5 bits "000FUDLR"
    data <<= 1;
    if (bitRead(PINB, 1)) {  // Reads data from pin 9 (PB1)
      bitSet(data, 0);
    }
    // Toggle clock pin low to high to read the next bit
    bitClear(PORTC, DDC2);  // Clock low
    delayMicroseconds(1);   // Short delay to ensure stable clocking
    bitSet(PORTC, DDC2);    // Clock high
  }
  bitClear(PORTB, DDB2);  //LOW, pin 10 (PB2),  Disable shifting (latch)
  return data;
}


/*
__attribute__((optimize("-Ofast"))) 
uint16_t zx_spectrum_screen_addressxxx(uint8_t x, uint8_t y) {
  // Base screen address in ZX Spectrum
  uint16_t base_address = 0x4000;

  // Calculate section offset based on the Y coordinate
  uint16_t section_offset;
  if (y < 64) {
    section_offset = 0;  // First section
  } else if (y < 128) {
    section_offset = 0x0800;  // Second section
  } else {
    section_offset = 0x1000;  // Third section
  }

  // Calculate the correct interleaved line address
  uint8_t block_in_section = (y & 0b00111000) >> 3;  // Extract bits 3-5 (block number)
  uint8_t line_in_block = y & 0b00000111;            // Extract bits 0-2 (line within block)
  uint16_t row_within_section = (line_in_block * 256) + (block_in_section * 32);

  // Calculate the horizontal byte index (each byte represents 8 pixels)
  uint8_t x_byte_index = x >> 3;

  // Calculate and return the final screen address
  return base_address + section_offset + row_within_section + x_byte_index;
}
*/

//-------------------------------------------------
// SD Card - File loading Support Section 
//-------------------------------------------------

__attribute__((optimize("-Ofast"))) 
void openFileByIndex(uint8_t searchIndex) {
  root.rewind();
  uint8_t index = 0;
  while (file.openNext(&root, O_RDONLY)) {
    if (file.isFile()) {
      if (file.fileSize() == 49179) {
        if (index == searchIndex) {
          break;
        }
        index++;
      }
    }
    file.close();
  }
}

__attribute__((optimize("-Ofast"))) 
uint16_t getSnaFileCount() {
  uint16_t totalFiles = 0;
  root.rewind();
  while (file.openNext(&root, O_RDONLY)) {
    if (file.isFile()) {
      if (file.fileSize() == 49179) {
        totalFiles++;
      }
    }
    file.close();
  }
  return totalFiles;
}

}
