#include "utils.h"
#include "Z80Bus.h"

#include "draw.h"

 __attribute__((optimize("-Os")))
void Utils::frameDelay(unsigned long start) {
  const unsigned long timeSpent = millis() - start;
  if (timeSpent < MAX_BUTTON_READ_MILLISECONDS) {
    delay(MAX_BUTTON_READ_MILLISECONDS - timeSpent);  // aiming for 50 FPS
  }
}

// Kempston Joystick Support:
// 1. The Arduino polls the joystick at 50 FPS via a 74HC165 shift register.
// 2. When the Spectrum performs an IN (C) instruction (e.g. LD C,$1F; IN D,(C)), the Arduino drives the Z80 data bus.
//    A 74HC32 combines the /IORQ, /RD, and /A7 signals to detect when the joystick port is being accessed.
//    A 74HC245D transceiver connects the Arduino to the Z80 to send joystick data, and it is tri-stated when not in use.
__attribute__((optimize("-Ofast")))
uint8_t Utils::readJoystick() {
  PORTB &= ~(1 << PB2);     // Latch LOW: Take a snapshot of all joystick inputs
  delayMicroseconds(1);     // Small delay to let the latch complete
  PORTB |= (1 << PB2);      // Latch HIGH: Enable shifting and make the first bit available on the data pin

  uint8_t data = 0;
  for (uint8_t i = 0; i < 8; i++) {
    data <<= 1;      
    PORTC &= ~(1 << PC2);     // Clock LOW
    if (PINB & (1 << PB1)) {  // serial data pin
      data |= 1;
    }
    PORTC |= (1 << PC2);      // Clock HIGH (next bit)
  }

  // The remaining 3 bits are FireB, Button Select (PCB button) & unused.
  return data;  // read bits are "uSfFUDLR"  
}

 __attribute__((optimize("-Os")))
void Utils::setupJoystick() {
  // Setup pins for "74HC165" shift register
  pinModeFast(Pin::ShiftRegDataPin, INPUT);
  pinModeFast(Pin::ShiftRegLatchPin, OUTPUT);
  pinModeFast(Pin::ShiftRegClockPin, OUTPUT);
}

// 1,000,000 microseconds to a second
// T-States: 4 + 4 + 12 = 20 ish (NOP,dec,jr)
// 1 T-state on a ZX Spectrum 48K is approximately 0.2857 microseconds.
// 20 T-States / 0.285714 = 70 t-states

// get16bitPulseValue: Used to get the command functions (addresses) from the speccy.
// The speccy broadcasts all the functions at power up.
uint16_t Utils::get16bitPulseValue() {
  constexpr uint16_t PULSE_TIMEOUT_US = 70; //120+20;
  uint16_t value = 0;
  for (uint8_t i = 0; i < 16; i++) { // 16 bits, 2 bytes
    uint8_t pulseCount = 0;
    uint32_t lastPulseTime = 0;
    while (1) {
      // Service current HALT if active
      if ((PINB & (1 << PINB0)) == 0) {  // Waits for Z80 HALT line to go HIGH
        // Pulse the Z80's /NMI line: LOW -> HIGH to un-halt the CPU.
        digitalWriteFast(Pin::Z80_NMI, LOW);
        digitalWriteFast(Pin::Z80_NMI, HIGH);
        pulseCount++;
        lastPulseTime = micros();  // reset timer, allow another pulse to be sampled
      }
      // Detect end marker delay at end of each single bit 
      if ((pulseCount > 0) && ((micros() - lastPulseTime) > PULSE_TIMEOUT_US)) {
        break;
      }
    }
    if (pulseCount == 2) {  // collect set bit
      value += 1 << (15 - i);
    }
  }
  return value;
}

uint8_t Utils::get8bitPulseValue() {
  constexpr uint16_t PULSE_TIMEOUT_US = 70; //120+20;
  uint8_t value = 0;
  for (uint8_t i = 0; i < 8; i++) { // 16 bits, 2 bytes
    uint8_t pulseCount = 0;
    uint32_t lastPulseTime = 0;
    while (1) {
      // Service current HALT if active
      if ((PINB & (1 << PINB0)) == 0) {  // Waits for Z80 HALT line to go HIGH
        // Pulse the Z80's /NMI line: LOW -> HIGH to un-halt the CPU.
        digitalWriteFast(Pin::Z80_NMI, LOW);
        digitalWriteFast(Pin::Z80_NMI, HIGH);
        pulseCount++;
        lastPulseTime = micros();  // reset timer, allow another pulse to be sampled
      }
      // Detect end marker delay at end of each single bit 
      if ((pulseCount > 0) && ((micros() - lastPulseTime) > PULSE_TIMEOUT_US)) {
        break;
      }
    }
    if (pulseCount == 2) {  // collect set bit
      value += 1 << (7 - i);
    }
  }
  return value;
}

__attribute__((optimize("-Os"))) void Utils::waitForUserExit() {
  do {
    unsigned long startTime = millis();
    PORTD = Utils::readJoystick() & JOYSTICK_MASK;

  //  Utils::frameDelay(startTime);
    uint8_t b = Utils::readJoystick();
    if (b & JOYSTICK_SELECT) {

      //----------------------------------------------------------------
      // Fire an NMI in the stock ROM
      // (uses modified unused locations: L0066 & L04AA)
      digitalWriteFast(Pin::Z80_NMI, LOW);
      digitalWriteFast(Pin::Z80_NMI, HIGH);
      //----------------------------------------------------------------
      // Allow the Z80 to push registers and then idle in a loop forever
      delay(5);
      digitalWriteFast(Pin::ROM_HALF, LOW);  // SNA ROM
      //----------------------------------------------------------------

      // free purging halt
      //     while (digitalReadFast(Pin::Z80_HALT) != 0) {}; // Z80 has halted
      //    digitalWriteFast(Pin::Z80_NMI, LOW);
      //   digitalWriteFast(Pin::Z80_NMI, HIGH);
      //   while (digitalReadFast(Pin::Z80_HALT) == 0) {}; // Z80 has resumed

      //----------------------------------------------------------------
      // TEST CODE - MAKE SURE WE CAN STILL DO USEFUL THINGS !!!
      // (we are running inside an NMI handler that itself uses NMI!)
      for (uint8_t i = 80; i < 80 + 64; i += 8) {
        Draw::text_P(16 + (i / 4), i, F("THIS IS A TEST"));
        delay(100);
      }
      //----------------------------------------------------------------
      // Send a jump instruction to the Z80
      // This will land in a forever loop in the SNA ROM.
      // That loop starts the path back to the stock ROM.
      uint8_t a[2];
      uint16_t val = 0x04AD;  // jp here
      a[0] = (uint8_t)(val >> 8);
      a[1] = (uint8_t)(val & 0xFF);
      Z80Bus::sendBytes(a, 2);
      //----------------------------------------------------------------
      // Allow the Z80 time to restore EI (if it was previously enabled)
      // and then start the idle loop.
      delay(5);
      //----------------------------------------------------------------
      // Switch back to stock ROM. This ROM does not have the idle loop.
      // It will POP the game registers and use "RET" to exit the NMI
      // (not "RETN"), because we must control the restoration of interrupts.
      // We’ve been doing non-standard things with NMIs inside NMI!
      digitalWriteFast(Pin::ROM_HALF, HIGH);  // STOCK 48K ROM
    }


  } while (true);  //} while ((Utils::readJoystick() & JOYSTICK_SELECT) == 0);

  while ((Utils::readJoystick() & JOYSTICK_SELECT) != 0) {}
}


// Not used ...
// 
// __attribute__((optimize("-Ofast"))) 
// void joinBits(byte* output, uint8_t input, uint16_t bitWidth, uint16_t bitPosition) {
//   uint16_t byteIndex = bitPosition >> 3;   // /8
//   uint8_t  bitIndex  = bitPosition & 7;    // %8
//   // Using a WORD to allow for a boundary crossing
//   uint16_t aligned = (uint16_t)input << (8 - bitWidth);
//   output[byteIndex] |= aligned >> bitIndex;
//   if (bitIndex + bitWidth > 8) {  // spans across two bytes
//     output[byteIndex + 1] |= aligned << (8 - bitIndex);
//   }
// }
// 
// __attribute__((optimize("-Ofast"))) 
// byte reverseBits(byte data) {
//   data = (data & 0xF0) >> 4 | (data & 0x0F) << 4;
//   data = (data & 0xCC) >> 2 | (data & 0x33) << 2;
//   data = (data & 0xAA) >> 1 | (data & 0x55) << 1;
//   return data;
// }
// 
// __attribute__((optimize("-Ofast"))) 
// void swap(byte &a, byte &b) {
//   byte temp = a;
//   a = b;
//   b = temp;
// }
