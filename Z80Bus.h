#ifndef Z80BUS_H
#define Z80BUS_H

#include "pin.h"
#include "buffers.h"
#include "digitalWriteFast.h"

/* -------------------------------------------------
 * ZX Spectrum Screen Attribute Byte Format
 * -------------------------------------------------
 * Format: [F | B | P2 | P1 | P0 | I2 | I1 | I0]
 *
 * Bit Fields:
 *   F  - FLASH mode (1 = flash, 0 = steady)
 *   B  - BRIGHT mode (1 = bright, 0 = normal)
 *   P2-P0 - PAPER colour (background)
 *   I2-I0 - INK colour (foreground)
 *
 * Colour Key (FBPPPIII): 
 *   000 = Black, 001 = Blue, 010 = Red, 011 = Magenta,
 *   100 = Green, 101 = Cyan, 110 = Yellow, 111 = White
 */

namespace Z80Bus {

//-------------------------------------------------
// Z80 Data Transfer and Control Routines
//-------------------------------------------------

void setupPins() {

  // best do ROM_HALF setting first
  pinModeFast(Pin::ROM_HALF, OUTPUT);
  digitalWriteFast(Pin::ROM_HALF, LOW);  //  Switch over to Sna ROM.

  pinModeFast(Pin::Z80_HALT, INPUT);
  pinModeFast(Pin::Z80_NMI, OUTPUT);
  pinModeFast(Pin::Z80_REST, OUTPUT);
  pinModeFast(Pin::Z80_D0Pin, OUTPUT);
  pinModeFast(Pin::Z80_D1Pin, OUTPUT);
  pinModeFast(Pin::Z80_D2Pin, OUTPUT);
  pinModeFast(Pin::Z80_D3Pin, OUTPUT);
  pinModeFast(Pin::Z80_D4Pin, OUTPUT);
  pinModeFast(Pin::Z80_D5Pin, OUTPUT);
  pinModeFast(Pin::Z80_D6Pin, OUTPUT);
  pinModeFast(Pin::Z80_D7Pin, OUTPUT);

  // setup /NMI - Z80 /NMI, used to trigger a 'HALT' so the Z80 can be released and resume
  digitalWriteFast(Pin::Z80_NMI, HIGH);  // put into a default state
}

/*
 * waitHalt:
 * Waits for Z80 HALT line to go LOW then HIGH (active low)
 * This shows the Z80 has resumed
*/ 
void waitHalt() {
  while (digitalReadFast(Pin::Z80_HALT) != 0) {};
  while (digitalReadFast(Pin::Z80_HALT) == 0) {};
}

void resetZ80() {
  digitalWriteFast(Pin::Z80_REST, LOW);   // z80 reset-line "LOW"
  delay(250);                             // reset line needs a delay (this is way more than needed!)
  digitalWriteFast(Pin::Z80_REST, HIGH);  // z80 reset-line "HIGH" - reboot
}

inline void resetToSnaRom() {
  digitalWriteFast(Pin::ROM_HALF, LOW);   // pin15 (A1) - Switch over to Sna ROM.
  digitalWriteFast(Pin::Z80_REST, LOW);   // pin17 to z80 reset-line active low
  delay(250);                             // reset line needs a delay (this is way more than needed!)
  digitalWriteFast(Pin::Z80_REST, HIGH);  // pin17 to z80 reset-line (now low to high) - reboot
}

void waitRelease_NMI() {
  // Wait for Z80 HALT line to go LOW (active low)
  while (digitalReadFast(Pin::Z80_HALT) != 0) {};
  // Pulse the Z80’s /NMI line: LOW -> HIGH to un-halt the CPU.
  digitalWriteFast(Pin::Z80_NMI, LOW);
  //   asm volatile(
  //   "nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n"  // 8 NOPs = 500ns @ 16MHz
  //   "nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n"  // 16 total NOPs
  // );
  digitalWriteFast(Pin::Z80_NMI, HIGH);
  // Wait for HALT line to return HIGH again (shows Z80 has resumed)
  while (digitalReadFast(Pin::Z80_HALT) == 0) {};
}

__attribute__((optimize("-Ofast"))) 
void sendBytes(byte* data, uint16_t size) {
// cli();  // Not really needed
  for (uint16_t i = 0; i < size; i++) {
    // Wait for Z80 HALT line to go LOW (active low)
    while (digitalReadFast(Pin::Z80_HALT) != 0) {};
    PORTD = data[i];  // Send Arduino d0-d7 to Z80 d0-d7
    // Pulse the Z80’s /NMI line: LOW -> HIGH to un-halt the CPU.
    digitalWriteFast(Pin::Z80_NMI, LOW);
   //   asm volatile(
   //     "nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n"  // 8 NOPs = 500ns @ 16MHz
    //    "nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n"  // 16 total NOPs
    //  );
    digitalWriteFast(Pin::Z80_NMI, HIGH);
    // Wait for HALT line to return HIGH again (shows Z80 has resumed)
    while (digitalReadFast(Pin::Z80_HALT) == 0) {};
  }
 // sei();
}

/* Send Snapshot Header Section */
void sendSnaHeader(byte* info) {
  // head27_2[0] already contains "E" which informs the speccy this packets a execute command.
  // Z80Bus::sendBytes(&info[0], 1 + 1 + 2 + 2 + 2 + 2);  // Send command "E" then I,HL',DE',BC',AF'
  // Z80Bus::sendBytes(&info[1 + 15], 2 + 2 + 1 + 1);     // Send IY,IX,IFF2,R (packet data continued)
  // Z80Bus::sendBytes(&info[1 + 23], 2);                 // Send SP                     "
  // Z80Bus::sendBytes(&info[1 + 9], 2);                  // Send HL                     "
  // Z80Bus::sendBytes(&info[1 + 25], 1);                 // Send IM                     "
  // Z80Bus::sendBytes(&info[1 + 26], 1);                 // Send BorderColour           "
  // Z80Bus::sendBytes(&info[1 + 11], 2);                 // Send DE                     "
  // Z80Bus::sendBytes(&info[1 + 13], 2);                 // Send BC                     "
  // Z80Bus::sendBytes(&info[1 + 21], 2);                 // Send AF                     "


  Z80Bus::sendBytes(&info[0], 2 + 1 + 2 + 2 + 2 + 2);  // Send COMMAND ADDR then  I,HL',DE',BC',AF'
  Z80Bus::sendBytes(&info[2 + 15], 2 + 2 + 1 + 1);     // Send IY,IX,IFF2,R (packet data continued)
  Z80Bus::sendBytes(&info[2 + 23], 2);                 // Send SP                     "
  Z80Bus::sendBytes(&info[2 + 9], 2);                  // Send HL                     "
  Z80Bus::sendBytes(&info[2 + 25], 1);                 // Send IM                     "
  Z80Bus::sendBytes(&info[2 + 26], 1);                 // Send BorderColour           "
  Z80Bus::sendBytes(&info[2 + 11], 2);                 // Send DE                     "
  Z80Bus::sendBytes(&info[2 + 13], 2);                 // Send BC                     "
  Z80Bus::sendBytes(&info[2 + 21], 2);                 // Send AF                     "


}

void fillScreenAttributes(const uint8_t attributes) {
  const uint16_t amount = 768;
  const uint16_t fillAddr = 0x5800;
  /* Fill mode */
//  FILL_COMMAND(packetBuffer, amount, fillAddr, attributes);
//  Z80Bus::sendBytes(packetBuffer, 6);

    packetBuffer[0] = (uint8_t)((command_Fill) >> 8); 
    packetBuffer[1] = (uint8_t)((command_Fill)&0xFF); 
    packetBuffer[2] = (uint8_t)((amount) >> 8); 
    packetBuffer[3] = (uint8_t)((amount)&0xFF); 
    packetBuffer[4] = (uint8_t)((fillAddr) >> 8); 
    packetBuffer[5] = (uint8_t)((fillAddr)&0xFF); 
    packetBuffer[6] = (uint8_t)(attributes); 

    Z80Bus::sendBytes(packetBuffer, 7);
}

void highlightSelection(uint16_t currentFileIndex, uint16_t startFileIndex, uint16_t& oldHighlightAddress) {
  const uint16_t amount = 32;
  const uint16_t fillAddr = 0x5800 + ((currentFileIndex - startFileIndex) * 32);

  if (oldHighlightAddress != fillAddr) {
    /* Remove old highlight - B01000111: Restore white text/black background for future use */
//    FILL_COMMAND(packetBuffer, amount, oldHighlightAddress, B01000111);
//    Z80Bus::sendBytes(packetBuffer, 6);

    packetBuffer[0] = (uint8_t)((command_Fill) >> 8); 
    packetBuffer[1] = (uint8_t)((command_Fill)&0xFF); 
    packetBuffer[2] = (uint8_t)((amount) >> 8); 
    packetBuffer[3] = (uint8_t)((amount)&0xFF); 
    packetBuffer[4] = (uint8_t)((oldHighlightAddress) >> 8); 
    packetBuffer[5] = (uint8_t)((oldHighlightAddress)&0xFF); 
    packetBuffer[6] = (uint8_t)(B01000111); 
    Z80Bus::sendBytes(packetBuffer, 7);

    oldHighlightAddress = fillAddr;
  }

  /* Highlight file selection - B00101000: Black text, Cyan background*/
  //FILL_COMMAND(packetBuffer, amount, destAddr, B00101000);
 // Z80Bus::sendBytes(packetBuffer, 6);

   packetBuffer[0] = (uint8_t)((command_Fill) >> 8); 
    packetBuffer[1] = (uint8_t)((command_Fill)&0xFF); 
    packetBuffer[2] = (uint8_t)((amount) >> 8); 
    packetBuffer[3] = (uint8_t)((amount)&0xFF); 
    packetBuffer[4] = (uint8_t)((fillAddr) >> 8); 
    packetBuffer[5] = (uint8_t)((fillAddr)&0xFF); 
    packetBuffer[6] = (uint8_t)(B00101000); 
    Z80Bus::sendBytes(packetBuffer, 7);
}

uint8_t GetKeyPulses() {


/* Timings sample: z80 code from Speccy side
DELAY_LOOP:
	NOP
	NOP
	NOP
  DJNZ DELAY_LOOP				; 25t-states 

  ; 25/35000000 = 7 microseconds (7.14)
  ; say 20 iteration above will give :-
  ; 1.428571428571429e-5 = 0.000011428 seconds
   ;1.428571428571429e-5 * 1000000 = 11.428 microseconds
*/  

  constexpr uint8_t DELAY_ITERATIONS_PARAM= 20;  // 20 loops of 25 t-states
  constexpr uint16_t PULSE_TIMEOUT_US = 70;
  uint8_t pulseCount = 0;
  uint32_t lastPulseTime = 0;

  // packetBuffer[0] = 'T';
  // packetBuffer[1] = DELAY_CMD_VALUE;  // delay after pulses
  // 20 / 0.285714 = 70 microseconds

  packetBuffer[0] = (uint8_t)((command_TransmitKey) >> 8);
  packetBuffer[1] = (uint8_t)((command_TransmitKey)&0xFF);
  packetBuffer[2] = DELAY_ITERATIONS_PARAM;  // delay use as end marker
  Z80Bus::sendBytes(packetBuffer, 3);

  while (1) {                          // Tally halts, noting the end marker is just a time gap
    if ((PINB & (1 << PINB0)) == 0) {  // halt active
      // Signal the speccy to un-halt the Z80 - Pulse the Z80’s /NMI line: LOW -> HIGH
      digitalWriteFast(Pin::Z80_NMI, LOW);
      digitalWriteFast(Pin::Z80_NMI, HIGH);
      lastPulseTime = micros();  // reset timer, allow another pulse to be sampled
      pulseCount++;
    }

    // Detect end of transmission (delay timeout after last halt)
    if ((pulseCount > 0) && ((micros() - lastPulseTime) > PULSE_TIMEOUT_US)) {
      return pulseCount - 1;
    }
  }
}


}

#endif