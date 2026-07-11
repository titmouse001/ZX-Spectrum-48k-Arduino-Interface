#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include "digitalWriteFast.h"
#include "Constants.h"
#include "src/fatlib/SdFat.h"

// SNAPSHOT (.SNA) FILE HEADER (27 bytes, no PC value stored):
//      REG_I  =00, REG_HL'=01, REG_DE'=03, REG_BC'=05
//      REG_AF'=07, REG_HL =09, REG_DE =11, REG_BC =13
//      REG_IY =15, REG_IX =17, REG_IFF=19, REG_R  =20
//      REG_AF =21, REG_SP =23, REG_IM =25, REG_BDR=26
struct __attribute__ ((packed)) Z80Registers {
    // This wil be raw copied to SNA file header so must be Little-Endian order (Low byte first).
    uint8_t i;
    uint8_t l_prime,h_prime;
    uint8_t e_prime,d_prime;
    uint8_t c_prime,b_prime;
    uint8_t f_prime,a_prime; 
    uint8_t l,h;
    uint8_t e,d;
    uint8_t c,b;
    uint8_t iyl,iyh;
    uint8_t ixl,ixh;
    uint8_t iff2;
    uint8_t r;  // included, not not true value at capture (pain and not really needed)
    uint8_t f;
    uint8_t a;
    uint8_t sp_lo,sp_hi;
    uint8_t im;
    uint8_t borderCol; // included, not tue at capture time
    
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


// #ifndef ARDUINO_AVR_NANO
// #error "This sketch is designed for Arduino Nano (ATmega328P). Please select the correct board in Tools > Board."
// #endif
// #if (F_CPU != 16000000L)
// #warning "This sketch expects 16MHz clock speed."
// #endif



#endif