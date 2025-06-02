#ifndef Z80BUS_H
#define Z80BUS_H

#include "pin.h"
#include "buffers.h"

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

void setupPins(){
  pinMode(Pin::Z80_HALT, INPUT);

  pinMode(Pin::Z80_REST, OUTPUT);

  pinMode(Pin::Z80_D0Pin, OUTPUT);
  pinMode(Pin::Z80_D1Pin, OUTPUT);
  pinMode(Pin::Z80_D2Pin, OUTPUT);
  pinMode(Pin::Z80_D3Pin, OUTPUT);
  pinMode(Pin::Z80_D4Pin, OUTPUT);
  pinMode(Pin::Z80_D5Pin, OUTPUT);
  pinMode(Pin::Z80_D6Pin, OUTPUT);
  pinMode(Pin::Z80_D7Pin, OUTPUT);
}

void setupNMI() {
  // Line to the Z80 /NMI, used to trigger a 'HALT' release and resume
  pinMode(Pin::Z80_NMI, OUTPUT);       
  WRITE_BIT(PORTC, DDC0, _HIGH);   // pin14 (A0), default Z80 /NMI line high
}

void waitHalt() {
    // Wait for Z80 HALT line (PINB0) to go LOW (active low)
  while ( (PINB & (1 << PINB0)) != 0 ) {};  // wait while HALT is HIGH
    // Wait for HALT line to return HIGH again (shows Z80 has resumed)
  while ( (PINB & (1 << PINB0)) == 0 ) {};
}

void resetZ80() { 
  WRITE_BIT(PORTC, DDC3, _LOW);    // z80 reset-line "LOW"
  delay(250);                     // reset line needs a delay (this is way more than needed!)
  WRITE_BIT(PORTC, DDC3, _HIGH);   // z80 reset-line "HIGH" - reboot
}

inline void resetToSnaRom() {
  WRITE_BIT(PORTC, DDC1, _LOW);    // pin15 (A1) - Switch over to Sna ROM.
  WRITE_BIT(PORTC, DDC3, _LOW);    // pin17 to z80 reset-line active low
  delay(250);                     // reset line needs a delay (this is way more than needed!)
  WRITE_BIT(PORTC, DDC3, _HIGH);   // pin17 to z80 reset-line (now low to high) - reboot
}

inline void bankSwitchStockRom() {
  WRITE_BIT(PORTC, DDC1, _HIGH);   // pin15 (A1) - Switch over to stock ROM.
}

void waitRelease_NMI() {
  // Wait for Z80 HALT line (PINB0) to go LOW (active low)
  while ( (PINB & (1 << PINB0)) != 0 ) {};  // wait while HALT is HIGH
  // Pulse the Z80’s /NMI line: LOW -> HIGH to un-halt the CPU.
  WRITE_BIT(PORTC, DDC0, _LOW);    // A0, pin14 low to Z80 /NMI
 //   asm volatile(
 //   "nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n"  // 8 NOPs = 500ns @ 16MHz
 //   "nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n"  // 16 total NOPs
 // );
  WRITE_BIT(PORTC, DDC0, _HIGH);   // A0, pin14 high to Z80 /NMI
  // Wait for HALT line to return HIGH again (shows Z80 has resumed)
  while ( (PINB & (1 << PINB0)) == 0 ) {};
}

__attribute__((optimize("-Ofast"))) 
void sendBytes(byte *data, uint16_t size) {
  cli(); // Not really needed
  for (uint16_t i = 0; i < size; i++) {
    // Wait for Z80 HALT line to go LOW (active low)
    while ( (PINB & (1 << PINB0)) != 0 ) {};  // wait while HALT is HIGH
    // Arduino (d0-d7) data port to Z80 d0-d7
    PORTD = data[i]; 
     // Pulse the Z80’s /NMI line: LOW -> HIGH to un-halt the CPU.
    WRITE_BIT(PORTC, DDC0, _LOW);   // A0, pin14 low
//  asm volatile(
//    "nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n"  // 8 NOPs = 500ns @ 16MHz
//    "nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n"  // 16 total NOPs
//  );
    WRITE_BIT(PORTC, DDC0, _HIGH);  // A0, pin14 high
    // Wait for HALT line to return HIGH again (shows Z80 has resumed)
    while ( (PINB & (1 << PINB0)) == 0 ) {};
  }
  sei();
}

/* Send Snapshot Header Section */
void sendSnaHeader(byte* info) {
  // head27_2[0] already contains "E" which informs the speccy this packets a execute command.
  Z80Bus::sendBytes(&info[   0], 1+1+2+2+2+2 );  // Send command "E" then I,HL',DE',BC',AF'
  Z80Bus::sendBytes(&info[1+15], 2+2+1+1 );   // Send IY,IX,IFF2,R (packet data continued)
  Z80Bus::sendBytes(&info[1+23], 2);          // Send SP                     "
  Z80Bus::sendBytes(&info[1+ 9], 2);          // Send HL                     "
  Z80Bus::sendBytes(&info[1+25], 1);          // Send IM                     "
  Z80Bus::sendBytes(&info[1+26], 1);          // Send BorderColour           "
  Z80Bus::sendBytes(&info[1+11], 2);          // Send DE                     "
  Z80Bus::sendBytes(&info[1+13], 2);          // Send BC                     "
  Z80Bus::sendBytes(&info[1+21], 2);          // Send AF                     "
}

void fillScreenAttributes(const uint8_t attributes) {
    const uint16_t amount = 768;      
    const uint16_t startAddress = 0x5800;
    /* Fill mode */
    FILL_COMMAND(packetBuffer, amount, startAddress, attributes );
    Z80Bus::sendBytes(packetBuffer, 6);
}

void highlightSelection(uint16_t currentFileIndex,uint16_t startFileIndex, uint16_t& oldHighlightAddress) {
  const uint16_t amount = 32;
  const uint16_t destAddr = 0x5800 + ((currentFileIndex - startFileIndex) * 32);

  if (oldHighlightAddress != destAddr) {
    /* Remove old highlight - B01000111: Restore white text/black background for future use */
    FILL_COMMAND(packetBuffer, amount, oldHighlightAddress, B01000111 );
    Z80Bus::sendBytes(packetBuffer, 6);
    oldHighlightAddress = destAddr;
  }

  /* Highlight file selection - B00101000: Black text, Cyan background*/
  FILL_COMMAND(packetBuffer, amount, destAddr, B00101000 );    
  Z80Bus::sendBytes(packetBuffer, 6 );
}

}

#endif