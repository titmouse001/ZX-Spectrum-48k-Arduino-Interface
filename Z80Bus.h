#ifndef Z80BUS_H
#define Z80BUS_H

#include "pins.h"

uint8_t head27_Execute[27 + 1] = { 'E' };   // pre populate with Execute command 

namespace Z80Bus {

//-------------------------------------------------
// Z80 Data Transfer and Control Routines
//-------------------------------------------------

__attribute__((optimize("-Ofast"))) 
void sendBytes(byte *data, uint16_t size) {
  for (uint16_t i = 0; i < size; i++) {
    while ((bitRead(PINB, PINB0) == HIGH)) {};
    PORTD = data[i];
    WRITE_BIT(PORTC, DDC0, LOW);
    WRITE_BIT(PORTC, DDC0, HIGH);
    while ((bitRead(PINB, PINB0) == LOW)) {};
  }
}

void waitHalt() {
  // Here we are waiting for the speccy to release the halt, it does this every 50fps.
  while ((bitRead(PINB, PINB0) == HIGH)) {};
  while ((bitRead(PINB, PINB0) == LOW)) {};
}

void waitRelease_NMI() {
  // Here we MUST release the z80 for it's HALT state.
  while ((bitRead(PINB, PINB0) == HIGH)) {};  // (1) Wait for Halt line to go LOW.
  WRITE_BIT(PORTC, DDC0, LOW);                      // (2) A0 (pin-8) signals the Z80 /NMI line,
  WRITE_BIT(PORTC, DDC0, HIGH);                        //     LOW then HIGH, this will un-halt the Z80.
  while ((bitRead(PINB, PINB0) == LOW)) {};   // (3) Wait for halt line to go HIGH again.
}

void resetToSnaROM(){
  pinMode(Z80_REST, OUTPUT);
  pinMode(ROM_HALF, OUTPUT);       

  WRITE_BIT(PORTC, DDC3, LOW);    // z800 reset-line "LOW"
  WRITE_BIT(PORTC, DDC1, LOW);    // pin15 (A1) - Switch over to Sna ROM.
  delay(10);                      // reset line needs a delay
  WRITE_BIT(PORTC, DDC3, HIGH);   // z80 reset-line "HIGH" - reboot
}


/* Send Snapshot Header Section */
void sendSnaHeader() {
  // head27_2[0] already contains "E" which informs the speccy this packets a execute command.
  Z80Bus::sendBytes(&head27_Execute[   0], 1+1+2+2+2+2 );  // Send command "E" then I,HL',DE',BC',AF'
  Z80Bus::sendBytes(&head27_Execute[1+15], 2+2+1+1 );   // Send IY,IX,IFF2,R (packet data continued)
  Z80Bus::sendBytes(&head27_Execute[1+23], 2);          // Send SP                     "
  Z80Bus::sendBytes(&head27_Execute[1+ 9], 2);          // Send HL                     "
  Z80Bus::sendBytes(&head27_Execute[1+25], 1);          // Send IM                     "
  Z80Bus::sendBytes(&head27_Execute[1+26], 1);          // Send BorderColour           "
  Z80Bus::sendBytes(&head27_Execute[1+11], 2);          // Send DE                     "
  Z80Bus::sendBytes(&head27_Execute[1+13], 2);          // Send BC                     "
  Z80Bus::sendBytes(&head27_Execute[1+21], 2);          // Send AF                     "
}


void bankSwitchSnaRom() {
  WRITE_BIT(PORTC, DDC1, LOW);    // pin15, A1, back switch rom
}

void bankSwitchStockRom() {
  WRITE_BIT(PORTC, DDC1, HIGH);   // pin15, A1, back switch rom
}

void setupNMI() {
  // This line connetcs to the Z80 /NMI which releases 
  // the z80's from its 'HALT' state.
  pinMode(Z80_NMI, OUTPUT);       
  WRITE_BIT(PORTC, DDC0, HIGH);   // pin14 (A0), Z80 /NMI line
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

}

#endif