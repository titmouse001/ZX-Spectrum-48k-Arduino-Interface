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

void debugFlash(int flashSpeed);

void setup() {

  /*
  Serial.begin(9600);
  while (! Serial);
  while (1) {  
    byte pinState = bitRead(PINC, DDC2);
    byte val = digitalRead(16);   
    Serial.println(pinState); 
  }; 
*/

  bitClear(PORTC, DDC1);
  pinMode(ROM_HALF, OUTPUT);    // LOW external/HIGH stock rom
  bitClear(PORTC, DDC1);  // pin15, A1 , swap out stock rom

  pinMode(Z80_NMI, OUTPUT);  // Connetcs to the Z80 /NMI which releases the z80's 'halt' state
  bitSet(PORTC, DDC0);  // pin14 (A0), Z80 /NMI line

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
  if (!sd.begin()) {}  // no longer using a pin for CS

  bool have = false;
  do {  //  DEBUG TEST LOADING - SOMETHING RANDOM TO TEST EACH TIME WE RESET
    if (!root.open("/")) {
      root.rewind();
      randomSeed(analogRead(4));
      while (file.openNext(&root, O_RDONLY)) {

        if (file.isFile()) {

 //           char fileName[16];
 //           file.getName7(fileName, 16);

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

//  buffer[HEADER_START_G] = 'G';          // Header
//  buffer[HEADER_START_O] = 'O';          // Header
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
  sendBytes(head27, 27 + 2);

  waitHalt();   // Wait for z80 to Halt, but we don't do anyting this side to release to halt.
                // Maskable takes care of it this time, as z80 needs to find a gap in
                // the 50FPS interrupt so things don't will walk over the stack when it's finally enabled.

  waitReleaseHalt();

  // USING BOTH HALFS OF THE EPROM.  
  // LOW PART FOR GAME LOADER/HIGH PART FOR 48k rom.
  bitSet(PORTC, DDC1);  // pin15 (A1) , put back original stock rom

  while (bitRead(PINB, PINB0) == LOW) {}  // DEBUG, block here if HALT still in action

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
