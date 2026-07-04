#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include "digitalWriteFast.h"
#include "Constants.h"
#include "SdFat.h" 

// struct Z80Registers {
//     uint8_t a, f, d, e, b, c, h, l;
//     uint8_t iff2;
//     uint8_t ixh, ixl;

//     uint16_t AllocMark;
// };

struct Z80Registers {
    uint8_t a, f, b, c, d, e, h, l;
    uint8_t a_prime, f_prime, b_prime, c_prime, d_prime, e_prime, h_prime, l_prime;
    uint8_t ixh, ixl, iyh, iyl;
    uint8_t sp_hi, sp_lo;
    uint8_t i;
    uint8_t r;
    uint8_t iff2;
    
    uint16_t AllocMark;
};


namespace Utils {

void clearScreen(uint8_t col);

uint8_t readJoystick();

void frameDelay(unsigned long start);
void setupJoystick();

Z80Registers* storeZ80States();
void restoreZ80States(Z80Registers* regs);

//void saveScreen(const char* filename);
//void restoreScreen(const char* filename);
void saveMemory(const char* filename, uint16_t address, uint16_t size);
void loadMemory(const char* filename, uint16_t address, uint16_t size);
void saveSnapshot(Z80Registers* regs);

uint16_t readLineTxt(FatFile* f, char* buf, uint16_t maxChars);

bool exportScreenshot(const char* folderName);
void viewSpeccyMemory();

void stockRomBoot_Blocking();
void waitForSDCard_Blocking(bool clearScreen= false);
void show5VoltRailStatus();
void resetSystem();

uint16_t zx_spectrum_screen_address(uint8_t x, uint8_t y);
uint16_t zx_spectrum_screen_address(uint8_t y);

void memsetZero(byte* b, uint16_t len);
void join6Bits(byte* output, uint8_t input, uint16_t bitPosition);
void delay16(uint16_t ms);

/* unused so far...
void joinBits(byte* output, uint8_t input, uint16_t bitWidth, uint16_t bitPosition);
byte reverseBits(byte data);
void swap(byte &a, byte &b);
*/

}  // namespace Utils


#ifndef ARDUINO_AVR_NANO
#error "This sketch is designed for Arduino Nano (ATmega328P). Please select the correct board in Tools > Board."
#endif
#if (F_CPU != 16000000L)
#warning "This sketch expects 16MHz clock speed."
#endif



#endif