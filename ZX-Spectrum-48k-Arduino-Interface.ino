// -------------------------------------------------------------------------------------
//  "Arduino Hardware Interface for ZX Spectrum" - 2023 P.Overy
// -------------------------------------------------------------------------------------
//
// This software uses the Arduino's ATmega328P range (Nano, Pro mini ... )
// for example here's the Nano specs:-
// - 32K bytes of in-system self-programmable flash program memory
// - 1K bytes EEPROM
// - 2K bytes internal SRAM

// SPEC OF SPECCY H/W PORTS HERE: -  https://mdfs.net/Docs/Comp/Spectrum/SpecIO
// ASM CODE FOR REF AT EOF (may be old if I forget to update)
//
// good link for Arduino info here: https://devboards.info/boards/arduino-nano

// -------------------------------------------------------------------------------------

// ATmega328P type Arduino (i.e Nano), only pins 2 and 3 are capable of generating interrupts.
// interrupt vector_0 activated on INT0/digital pin 2 and
// interrupt vector_1 activated on INT1/digital pin 3.
// Interrupt trigger modes are:
//  - LOW to trigger the interrupt whenever the pin is low,
//  - CHANGE to trigger the interrupt whenever the pin changes value
//  - RISING to trigger when the pin goes from low to high,
//  - FALLING for when the pin goes from high to low.
//
// ISC01	  ISC00	  DESCRIPTION
//   0	     0	    Low Level of INT0 generates interrupt
//   0	     1	    Logical change of INT0 generates interrupt
//   1	     0	    Falling Edge of INT0 generates interrupt
//   1	     1	    Rising Edge of INT0 generates interrupt
// For Example:-
//   bitClear(EICRA, ISC01);  // bit position "ISC01"
//   bitSet  (EICRA, ISC00);  // bit position "ISC00", both result in a Logical change on INT0

//https://arduino.stackexchange.com/questions/30968/how-do-interrupts-work-on-the-arduino-uno-and-similar-boards

/* IMPORTANT: Do not modify PORTB directly without preserving the clock/crystal bits.
 * NO LONGER WORKS THIS WAY... PORTB (digital pins 8 to 13) - Pin 8 is used here to replace pin 2 on PORTD, which is needed for interrupts.
 *                                The two high bits map to the crystal pins and should not be used.
 * NOW REPLACING PORTB with... PORTC PIN 16
 * PORTD (digital pins 0 to 7) - Pins 0, 1, 3, 4, 5, 6, 7 are used; pin 2 is reserved for interrupts.
 *
 * Note: Ideally, we could use a single byte, but constraints force us to use two bytes.
 */

// SNAPSHOT FILE HEADER (.sna files)
//REG_I	 =00, REG_HL'=01, REG_DE'	=03, REG_BC' =05	(.sna file header)
//REG_AF'=07, REG_HL =09, REG_DE	=11, REG_BC	 =13  (        27 bytes)
//REG_IY =15, REG_IX =17, REG_IFF =19, REG_R	 =20
//REG_AF =21, REG_SP =23, REG_IM	=25, REG_BDR =26



#include <avr/pgmspace.h>
#include <SPI.h>
//#include <CircularBuffer.h>  ...  very nice but in the end had to write my own optimsed circular buffer.
//#include <SD.h>   //  more than I need with UTF8 out the box, so using "SdFat.h"
#include "SdFat.h"  // "SdFatConfig.h" options, I'm using "USE_LONG_FILE_NAMES 1"
// 'SDFAT.H' : implementation allows 7-bit characters in the range

//#include <FastCRC.h>
//FastCRC8 CRC8;
//uint8_t buf[5] = {'h', 'e', 'l', 'l', 'o'};
//  Serial.println( CRC8.smbus(buf, sizeof(buf)), HEX );



// Arduino pin assignment
//const byte interruptPin = 2;  // only pins 2 or 3 for the ATmega328P Microcontroller
const byte sdPin = 9;  // SD card pin
const byte ledPin = 13;

// Arduino Pins that connect up to the z80 data pins
const byte Z80_D0Pin = 0;
const byte Z80_D1Pin = 1;
const byte Z80_D2Pin = 2;  // 16;  // A2 (pin2 not available)
const byte Z80_D3Pin = 3;
const byte Z80_D4Pin = 4;
const byte Z80_D5Pin = 5;
const byte Z80_D6Pin = 6;
const byte Z80_D7Pin = 7;
const byte Z80_HALT  = 8;  // PINB0 (PORT B), Z80 'Halt' Status

// Message structure
const byte HEADER_START_G = 0;      // Index for first byte of header
const byte HEADER_START_O = 1;      // Index for second byte of header
const byte HEADER_PAYLOADSIZE = 2;  // Index for payload as bytes
const byte HEADER_HIGH_BYTE = 3;    // Index for high byte of address
const byte HEADER_LOW_BYTE = 4;     // Index for low byte of address

const byte SIZE_OF_HEADER = 5;
const byte PAYLOAD_BUFFER_SIZE = 250;
const byte BUFFER_SIZE = SIZE_OF_HEADER + PAYLOAD_BUFFER_SIZE;

static byte head27[27 + 2] = { 'E', 'X' };  // Execute command "EX"
static byte buffer[BUFFER_SIZE];  // stores header + payload

SdFat32 sd;
FatFile root;
FatFile file;

void debugFlash(int flashSpeed);

void setup() {


  //   Serial.begin(9600);
  //    while (! Serial);

  bitClear(PORTC, DDC1);
  pinMode(15, OUTPUT);    // LOW external/HIGH stock rom
  bitClear(PORTC, DDC1);  // pin15, A1 , swap out stock rom

  //  pinMode(interruptPin, OUTPUT);  // Don't trigger during setup

  // These pins are connected to the address lines on the Z80
  pinMode(Z80_D0Pin, OUTPUT);
  pinMode(Z80_D1Pin, OUTPUT);
  pinMode(Z80_D2Pin, OUTPUT);
  pinMode(Z80_D3Pin, OUTPUT);
  pinMode(Z80_D4Pin, OUTPUT);
  pinMode(Z80_D5Pin, OUTPUT);
  pinMode(Z80_D6Pin, OUTPUT);
  pinMode(Z80_D7Pin, OUTPUT);
  pinMode(Z80_HALT, INPUT);

  bitSet(PORTC, DDC0);  // (pins default as input) - can you still set while in input mode?
  // Connetcs to the Z80 /NMI which releases the z80's 'halt' state
  pinMode(14, OUTPUT);  // "/NMI"
  bitSet(PORTC, DDC0);  // pin14 (A0), Z80 /NMI line

  // Initialize SD card
  if (!sd.begin(sdPin)) {}

  // Interrupt (INT0) on pin 2 - External Interrupt Control Register A (EICRA) / Mask Register (EIMSK).
  //  cli();
  //  bitSet(EICRA, ISC01);  // Rising Edge of INT0 generates interrupt
  //  bitSet(EICRA, ISC00);
  //  bitSet(EIMSK, INT0);   // enable INT0 interrupt
  //  sei();

  // Interrupts need to use pins 2 or 3 (I'm using a Nano/Pro Mini ).
  //  pinMode(interruptPin, INPUT);  // TODO  ... move somewhere better, maybe once buffer is full.. ISR will INC INDEX


  //char name[20];
  bool have = false;
  do {
    if (!root.open("/")) {
      root.rewind();
      randomSeed(analogRead(4));
      while (file.openNext(&root, O_RDONLY)) {
        if (file.isFile()) {

          if (file.fileSize() == 49179) {
            if (random(5) == 0) {
              //    file.printName(&Serial);
              //      Serial.println();
              have = true;
              break;
            }
          }
        }
        file.close();
      }
    }
  } while (!have);

}


void loop() {

  if (file.available()) {
    byte bytesReadHeader = (byte)file.read(&head27[0 + 2], 27);
    if (bytesReadHeader != 27) { debugFlash(3000); }
  }

  delay(50);

  buffer[HEADER_START_G] = 'G';          // Header
  buffer[HEADER_START_O] = 'O';          // Header
  uint16_t destinationAddress = 0x4000;  //screen address

  while (file.available()) {

    byte bytesRead = (byte)file.read(&buffer[SIZE_OF_HEADER], PAYLOAD_BUFFER_SIZE);
    buffer[HEADER_PAYLOADSIZE] = bytesRead;
    buffer[HEADER_HIGH_BYTE] = (destinationAddress >> 8) & 0xFF;  // high byte
    buffer[HEADER_LOW_BYTE] = destinationAddress & 0xFF;          // low byte

    byte bufferSize = SIZE_OF_HEADER + bytesRead;
    sendBytes(buffer, bufferSize);

    destinationAddress += buffer[HEADER_PAYLOADSIZE];
  }

  // *** Send Snapshot Header Section ***
  sendBytes(head27, 27+2);

  while ((bitRead(PINB, PINB0) == HIGH)) {};  // wait for HALT (LOW)
  bitSet(PORTC, DDC1);                        // pin15 (A1) , put back original stock rom
 // pinMode(15, INPUT);

  delay(10);

  // Un-Halt Z80, take line HIGH,LOW,HIGH
  bitClear(PORTC, DDC0);  // Z80 /NMI line,
  bitSet(PORTC, DDC0);

  pinMode(14, INPUT);  

  while (bitRead(PINB, PINB0) == LOW) {}  // DEBUG - lock if we missed something

  debugFlash(200);

  delay(1000000);
}

void sendBytes(byte* data, byte size) {
  for (byte bufferIndex = 0; bufferIndex < size; bufferIndex++) {
    while ((bitRead(PINB, PINB0) == HIGH)) {};
    PORTD = data[bufferIndex];
    bitClear(PORTC, DDC0);
    bitSet(PORTC, DDC0);
    while ((bitRead(PINB, PINB0) == LOW)) {};
  }
}

void releaseHalt() {
  while ((bitRead(PINB, PINB0) == HIGH)) {};
  // Once the input pin is LOW, toggle the output pin
  bitClear(PORTC, DDC0);  // A0 signals the Z80 /NMI line,
  bitSet(PORTC, DDC0);    // this will un-halt the Z80.
  while ((bitRead(PINB, PINB0) == LOW)) {};
}

void debugFlash(int flashspeed) {
  file.close();
  root.close();
  sd.end();
  while (1) {
    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, HIGH);
    delay(flashspeed);
    digitalWrite(ledPin, LOW);
    delay(flashspeed);
  }
}


/* 
 * Interrupt Service Routine for INT0
 * Updates buffer index upon receiving interrupt from Z80
 * 
 * The main loop now pre-sets data on PORTD and PORTC while the Z80 is halted.
 * Previously, sending data via this ISR was rather overwhelming for it to keep up 100% or even fire in time.
 * Historically, the ISR originally manipulated PORTC and PORTD directly for efficiency as no bit alignment was needed, 
 * and before PORTC I was using PORTB that was not aligned so required extra bit shifts. All adds up when 
 * trying to keep inside the Z80's "IN" timings!!!
 */
//ISR(INT0_vect) {  // triggered by z80:IN
//   bufferIndex++;
//}
