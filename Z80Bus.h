#ifndef Z80BUS_H
#define Z80BUS_H

#include "pins.h"
#include "buffers.h"

namespace Z80Bus {

//-------------------------------------------------
// Z80 Data Transfer and Control Routines
//-------------------------------------------------

__attribute__((optimize("-Ofast"))) 
void sendBytes(byte *data, uint16_t size) {
  for (uint16_t i = 0; i < size; i++) {
    // Wait for the Z80’s HALT line to be released (go LOW).
    while ((bitRead(PINB, PINB0) == HIGH)) {};
    PORTD = data[i];  // Arduino (d0-d7) data port to Z80 d0-d7
    WRITE_BIT(PORTC, DDC0, LOW);   // A0, pin14 low to Z80 /NMI
    WRITE_BIT(PORTC, DDC0, HIGH);  // A0, pin14 high to Z80 /NMI
    // Confirm the HALT line has returned to HIGH.
    while ((bitRead(PINB, PINB0) == LOW)) {};
  }
}

void waitHalt() {
  // Here we are waiting for the speccy to release the halt, it does this every 50fps.
  while ((bitRead(PINB, PINB0) == HIGH)) {};
  while ((bitRead(PINB, PINB0) == LOW)) {};
}

void waitRelease_NMI() {
  // Wait for the Z80’s HALT line to be released (go LOW).
  while (bitRead(PINB, PINB0) == HIGH) {}
  // Pulse the Z80’s /NMI line: LOW -> HIGH to un-halt the CPU.
  WRITE_BIT(PORTC, DDC0, LOW);    // A0, pin14 low to Z80 /NMI
  WRITE_BIT(PORTC, DDC0, HIGH);   // A0, pin14 high to Z80 /NMI
  // Confirm the HALT line has returned to HIGH.
  while (bitRead(PINB, PINB0) == LOW) {}
}

/*
void resetToSnaROM(){
  pinMode(Z80_REST, OUTPUT);
  pinMode(ROM_HALF, OUTPUT);       

  WRITE_BIT(PORTC, DDC3, LOW);    // z800 reset-line "LOW"
  WRITE_BIT(PORTC, DDC1, LOW);    // pin15 (A1) - Switch over to Sna ROM.
  delay(10);                      // reset line needs a delay
  WRITE_BIT(PORTC, DDC3, HIGH);   // z80 reset-line "HIGH" - reboot
}
*/

void resetZ80() { 
 // pinMode(Z80_REST, OUTPUT);
  WRITE_BIT(PORTC, DDC3, LOW);    // z80 reset-line "LOW"
  delay(250);                     // reset line needs a delay (this is way more than needed!)
  WRITE_BIT(PORTC, DDC3, HIGH);   // z80 reset-line "HIGH" - reboot
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


inline void resetToSnaRom() {

  WRITE_BIT(PORTC, DDC1, LOW);    // pin15 (A1) - Switch over to Sna ROM.

  WRITE_BIT(PORTC, DDC3, LOW);    // pin17 to z80 reset-line active low
  delay(250);                     // reset line needs a delay (this is way more than needed!)
  WRITE_BIT(PORTC, DDC3, HIGH);   // pin17 to z80 reset-line (now low to high) - reboot
}

inline void bankSwitchStockRom() {
  WRITE_BIT(PORTC, DDC1, HIGH);   // pin15 (A1) - Switch over to stock ROM.
}

void setupNMI() {
  // This line connetcs to the Z80 /NMI which releases 
  // the z80's from its 'HALT' state.
  pinMode(Z80_NMI, OUTPUT);       
  WRITE_BIT(PORTC, DDC0, HIGH);   // pin14 (A0), default Z80 /NMI line high
}

void setupHalt() {
  pinMode(Z80_HALT, INPUT);
}

void setupReset(){
  pinMode(Z80_REST, OUTPUT);
}


void setupDataLine(byte direction){
  pinMode(Z80_D0Pin, direction);
  pinMode(Z80_D1Pin, direction);
  pinMode(Z80_D2Pin, direction);
  pinMode(Z80_D3Pin, direction);
  pinMode(Z80_D4Pin, direction);
  pinMode(Z80_D5Pin, direction);
  pinMode(Z80_D6Pin, direction);
  pinMode(Z80_D7Pin, direction);
}

/* -------------------------------------------------
 * Section: Graphics Support for Zx Spectrum Screen
 * -------------------------------------------------
 * [F|B|P2|P1|P0|I2|I1|I0]
 * bit F sets the attribute FLASH mode
 * bit B sets the attribute BRIGHTNESS mode
 * bits P2 to P0 is the PAPER colour
 * bits I2 to I0 is the INK colour
 */
void setupScreenAttributes(const uint8_t attributes) {
    const uint16_t amount = 768;      
    const uint16_t startAddress = 0x5800;
    /* Fill mode */
    FILL_COMMAND(packetBuffer, amount, startAddress, attributes );
    Z80Bus::sendBytes(packetBuffer, 6);
}

void highlightSelection(uint16_t currentFileIndex,uint16_t startFileIndex, uint16_t& oldHighlightAddress) {
  /* Draw Cyan selector bar with black text (FBPPPIII) */
  /* BITS COLOUR KEY: 0 Black, 1 Blue, 2 Red, 3 Magenta, 4 Green, 5 Cyan, 6 Yellow, 7 White	*/
  const uint16_t amount = 32;
  const uint16_t destAddr = 0x5800 + ((currentFileIndex - startFileIndex) * 32);
  /* Highlight file selection - B00101000: Black text, Cyan background*/
  FILL_COMMAND(packetBuffer, amount, destAddr, B00101000 );    
  Z80Bus::sendBytes(packetBuffer, 6 );

  if (oldHighlightAddress != destAddr) {
    /* Remove old highlight - B01000111: Restore white text/black background for future use */
    FILL_COMMAND(packetBuffer, amount, oldHighlightAddress, B01000111 );
    Z80Bus::sendBytes(packetBuffer, 6);
    oldHighlightAddress = destAddr;
  }
}


}

#endif