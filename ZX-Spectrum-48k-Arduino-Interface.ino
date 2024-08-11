// -------------------------------------------------------------------------------------
//  "Arduino Hardware Interface for ZX Spectrum" - 2023/24 P.Overy
// -------------------------------------------------------------------------------------
// This software uses the Arduino's ATmega328P range (Nano, Pro mini ... )
// for example here's the Nano specs:-
// - 32K bytes of in-system self-programmable flash program memory
// - 1K bytes EEPROM
// - 2K bytes internal SRAM
// IMPORTANT: Do not modify PORTB directly without preserving the clock/crystal bits.
// -------------------------------------------------------------------------------------
// SPEC OF SPECCY H/W PORTS HERE: - https://mdfs.net/Docs/Comp/Spectrum/SpecIO
// good links for Arduino info  :   https://devboards.info/boards/arduino-nano
//                                  https://arduino.stackexchange.com/questions/30968/how-do-interrupts-work-on-the-arduino-uno-and-similar-boards
// SNAPSHOT FILE HEADER (.sna files)
//  REG_I	 =00, REG_HL'=01, REG_DE'	=03, REG_BC' =05	(.sna file header)
//  REG_AF'=07, REG_HL =09, REG_DE	=11, REG_BC	 =13  (        27 bytes)
//  REG_IY =15, REG_IX =17, REG_IFF =19, REG_R	 =20
//  REG_AF =21, REG_SP =23, REG_IM	=25, REG_BDR =26
// -------------------------------------------------------------------------------------

#include <avr/pgmspace.h>
#include <SPI.h>
#include "SdFat.h"  // "SdFatConfig.h" options, I'm using "USE_LONG_FILE_NAMES 1"

// 'Z80_D0Pin' to 'Z80_D7Pin', Arduino Pins that connect up to the z80 data pins
const byte Z80_D0Pin = 0;
const byte Z80_D1Pin = 1;
const byte Z80_D2Pin = 2;  // 16;  // A2 (pin2 not available)
const byte Z80_D3Pin = 3;
const byte Z80_D4Pin = 4;
const byte Z80_D5Pin = 5;
const byte Z80_D6Pin = 6;
const byte Z80_D7Pin = 7;

const byte Z80_HALT = 8;  // PINB0 (PORT B), Z80 'Halt' Status

const byte ledPin = 13;
const byte Z80_NMI = 14;
const byte ROM_HALF = 15;

// Transfer structure
const byte HEADER_START_G = 0;      // Index for first byte of header
const byte HEADER_START_O = 1;      // Index for second byte of header
const byte HEADER_PAYLOADSIZE = 2;  // Index for payload as bytes
const byte HEADER_HIGH_BYTE = 3;    // Index for high byte of address
const byte HEADER_LOW_BYTE = 4;     // Index for low byte of address
const byte SIZE_OF_HEADER = HEADER_LOW_BYTE+1;
const byte PAYLOAD_BUFFER_SIZE = 250;
const byte BUFFER_SIZE = SIZE_OF_HEADER + PAYLOAD_BUFFER_SIZE; 

static byte buffer[BUFFER_SIZE]= { 'G', 'O' };   // Load program "GO"

static byte head27[27 + 2] = { 'E', 'X' };  // Execute command "EX"
 
SdFat32 sd;
FatFile root;
FatFile file;

File statusFile;
int currentIndex;

void debugFlash(int flashSpeed);

void setup() {
  /* 
  Serial.begin(9600);
  while (! Serial);
  while (1) {  
    byte pinState = bitRead(PINB, PINB1);
 //   byte val = digitalRead(Z80_A12);   
    Serial.println(pinState); 
  }; 
*/

  bitClear(PORTC, DDC1);
  pinMode(ROM_HALF, OUTPUT);  // LOW external/HIGH stock rom
  bitClear(PORTC, DDC1);      // pin15, A1 , swap out stock rom

  pinMode(Z80_NMI, OUTPUT);  // Connetcs to the Z80 /NMI which releases the z80's 'halt' state
  bitSet(PORTC, DDC0);       // pin14 (A0), Z80 /NMI line

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

  // Initialize SD card
  if (!sd.begin()) {}  

  if (!sd.exists("status.txt")) {
    statusFile.open("status.txt", O_RDWR);
    if (statusFile) {
      statusFile.seekSet(0);
      statusFile.truncate(0);
      statusFile.seekSet(0);
      statusFile.print(0);
      statusFile.close();
    }
  }

  statusFile.open("status.txt", O_RDWR);
  if (statusFile) {
    statusFile.seekSet(0);
    currentIndex = statusFile.parseInt();
    statusFile.close();
  }

  if (!root.open("/")) {
    debugFlash(200);
  }

  root.rewind();
  int index = 0;
  while (file.openNext(&root, O_RDONLY)) {
    if (file.isFile()) {
      if (file.fileSize() == 49179) {
        if (index == currentIndex) {
          //   char fileName[16];
          //   file.getName7(fileName, 16);
          //    Serial.println(fileName);
          break;
        }
        index++;
      }
    }
    file.close();
  }

}


void loop() {

  if (file.available()) {
    byte bytesReadHeader = (byte)file.read(&head27[0 + 2], 27);
    if (bytesReadHeader != 27) { debugFlash(3000); }
  }

  uint16_t destinationAddress = 0x4000;  //screen address
  while (file.available()) {
    byte bytesRead = (byte)file.read(&buffer[SIZE_OF_HEADER], PAYLOAD_BUFFER_SIZE);
    buffer[HEADER_PAYLOADSIZE] = bytesRead;
    buffer[HEADER_HIGH_BYTE] = (destinationAddress >> 8) & 0xFF;  // high byte
    buffer[HEADER_LOW_BYTE] = destinationAddress & 0xFF;          // low byte
    sendBytes(buffer, SIZE_OF_HEADER + bytesRead);
    destinationAddress += buffer[HEADER_PAYLOADSIZE];
  }

  // *** Send Snapshot Header Section ***

  // SWAP BC WITH DE
  byte reg_d = head27[2+3];  // store DE'
  byte reg_e = head27[2+4];
  head27[2+3] = head27[2+5];  // DE' is now BC'
  head27[2+4] = head27[2+6];
  head27[2+5] = reg_d;        // DB' is now DE'
  head27[2+6] = reg_e;

  sendBytes(head27, 27 + 2);
  sendBytes(&head27[2+13], 2);  // BC
  sendBytes(&head27[2+11], 2);  // DE
  sendBytes(&head27[2+21], 2);  // AF


//  REG_I	 =00, REG_HL'=01, REG_DE'	=03, REG_BC' =05	(.sna file header)
//  REG_AF'=07, REG_HL =09, REG_DE	=11, REG_BC	 =13  (        27 bytes)
//  REG_IY =15, REG_IX =17, REG_IFF =19, REG_R	 =20
//  REG_AF =21, REG_SP =23, REG_IM	=25, REG_BDR =26






 // Wait for the Z80 to halt. The maskable interrupt will handle releasing it during a gap in the 50FPS cycle.
  waitHalt(); 
 
  // Ensure we're running in memory before swapping ROMs.
  waitHalt();  
 
  // The rom maskable interrupt uses only RETI, so swapping back to original rom won't interfere.
  bitSet(PORTC, DDC1);  // pin15 (A1) - Switch to high part of the ROM.
  
  while (bitRead(PINB, PINB0) == LOW) {}  // DEBUG, block here if HALT still in action

  statusFile.open("status.txt", O_RDWR);
  if (statusFile) {
    statusFile.seekSet(0);
    currentIndex = statusFile.parseInt();
    statusFile.seekSet(0);
    statusFile.truncate(0);
    currentIndex++;
    if (currentIndex>8) {
      currentIndex=0;
    }
    statusFile.print(currentIndex);
    statusFile.close();
  }

  debugFlash(200);  // All ok - Flash led to show we have reached the end with no left over halts
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

/*
void waitA14() {  
  // z80 - monitor/wait for A12 to go LOW/HIGH
  while ((bitRead(PINB, PINB1) == HIGH)) {};
  while ((bitRead(PINB, PINB1) == LOW)) {};
}
*/
void waitHalt() {  
  // z80 side with clear halt line 
  // last HALT will be allowed to see the maskable interrupt
  while ((bitRead(PINB, PINB0) == HIGH)) {};
  while ((bitRead(PINB, PINB0) == LOW)) {};
}

void waitReleaseHalt() {
  while ((bitRead(PINB, PINB0) == HIGH)) {};
  // Once the input pin is LOW, toggle the output pin
  bitClear(PORTC, DDC0);  // A0 signals the Z80 /NMI line,
  bitSet(PORTC, DDC0);    // this will un-halt the Z80.
  /////// while ((bitRead(PINB, PINB0) == LOW)) {};
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
