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
const byte interruptPin = 2;  // only pins 2 or 3 for the ATmega328P Microcontroller
const byte sdPin = 9;         // SD card pin
const byte ledPin = 13;

 // Arduino Pins that connect up to the z80 data pins
const byte Z80_D0Pin = 0;
const byte Z80_D1Pin = 1;
const byte Z80_D2Pin = 16;  // A2 (pin2 not available)
const byte Z80_D3Pin = 3;
const byte Z80_D4Pin = 4;
const byte Z80_D5Pin = 5;
const byte Z80_D6Pin = 6;
const byte Z80_D7Pin = 7;
// Z80 'Halt' Status 
const byte Z80_HALT = 8;   // PINB0 (PORT B)

// Message structure
const byte HEADER_START_G = 0;      // Index for first byte of header
const byte HEADER_START_O = 1;      // Index for second byte of header
const byte HEADER_PAYLOADSIZE = 2;  // Index for payload as bytes
const byte HEADER_HIGH_BYTE = 3;    // Index for high byte of address
const byte HEADER_LOW_BYTE = 4;     // Index for low byte of address

const byte SIZE_OF_HEADER = 5;
const byte PAYLOAD_BUFFER_SIZE = 250;
const byte BUFFER_SIZE = SIZE_OF_HEADER + PAYLOAD_BUFFER_SIZE;

byte head27[27+2]; 
//byte bytesReadHeader=0;

byte buffer[BUFFER_SIZE];  // stores header + payload
// NOTE: bufferIndex and bufferSize must be in bytes for fast unpacking in interrupt handling,
// as response time to Z80:IN is critical.
volatile byte bufferIndex = 0;  // Index of the next byte to be written to the buffer
volatile byte bufferSize = 0;   // Current size of valid data in the buffer

boolean readyToSend = false;

SdFat32 sd;
FatFile dataFile;

void setup() {

   //  Serial.begin(9600);
  //   while (! Serial);

  pinMode(interruptPin, OUTPUT);  // Don't trigger during setup
  pinMode(ledPin, OUTPUT);        // DEBUG

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
  
  pinMode(14, OUTPUT);   // "/NMI"
  pinMode(15, OUTPUT);   // "/ROMCS"
  // Connetcs to the Z80 /NMI which releases the z80's 'halt' state
//  bitSet(DDRC, DDC0);     
  bitClear(PORTC,DDC0);  // pin14 (A0), signals the Z80 /NMI line,

  // SELECT ROM (MOVE THIS, BUT NEED TO KEEP HERE FOR NOW ... NO MORE LOGICAL CHANGES YET)
  bitClear(PORTC,DDC1);  // pin15, A1 , swap out stock rom

  // Initialize SD card
  if (!sd.begin(sdPin)) {
     while (1) {
      pinMode(ledPin, OUTPUT);  
       digitalWrite(ledPin, HIGH); // sets the digital pin 13 on
        delay(250);            // waits for a second
        digitalWrite(ledPin, LOW);  // sets the digital pin 13 off
        delay(250);            // waits for a second
    }  
  }

  // Interrupt (INT0) on pin 2 - External Interrupt Control Register A (EICRA) / Mask Register (EIMSK).
  cli();
  bitSet(EICRA, ISC01);  // Rising Edge of INT0 generates interrupt
  bitSet(EICRA, ISC00);
  bitSet(EIMSK, INT0);   // enable INT0 interrupt
  sei();

  // Interrupts need to use pins 2 or 3 (I'm using a Nano/Pro Mini ).
  pinMode(interruptPin, INPUT);  // TODO  ... move somewhere better, maybe once buffer is full.. ISR will INC INDEX 
}

const char *fileNames[] = { "zynaps.sna" , "1.scr", "2.scr", "3.scr", "4.scr" };
//const byte items = sizeof(fileNames) / sizeof(fileNames[0]);
byte fileIndex = 0;

void loop() {

  if (!dataFile.open(fileNames[fileIndex])) {
    dataFile.close();
    sd.end();
    while (1) {
      pinMode(ledPin, OUTPUT);  
       digitalWrite(ledPin, HIGH); 
        delay(5000);          
        digitalWrite(ledPin, LOW);
        delay(5000);          
    }  
  }

  if (dataFile.available()) {
    byte bytesReadHeader = (byte)dataFile.read(&head27[0+2], 27);
    if (bytesReadHeader!=27) {
      dataFile.close();
      sd.end();
      while (1) {
        pinMode(ledPin, OUTPUT);  
        digitalWrite(ledPin, HIGH); 
        delay(3000);        
        digitalWrite(ledPin, LOW); 
        delay(3000);          
      }  
    }
  }


  uint16_t destinationAddress = 0x4000;  //screen address

  while (dataFile.available()) {
    buffer[HEADER_START_G] = 'G';  // Header
    buffer[HEADER_START_O] = 'O';  // Header

    const uint8_t high_byte = (destinationAddress >> 8) & 0xFF;
    const uint8_t low_byte = destinationAddress & 0xFF;
    buffer[HEADER_HIGH_BYTE] = high_byte;
    buffer[HEADER_LOW_BYTE] = low_byte;

    byte bytesRead = 0;
    if (dataFile.available()) {
      // Read up to PAYLOAD_BUFFER_SIZE bytes or until end of file
      bytesRead = (byte)dataFile.read(&buffer[SIZE_OF_HEADER], PAYLOAD_BUFFER_SIZE);
    }

    buffer[HEADER_PAYLOADSIZE] = bytesRead;

    // Pre-setting the data here first and in the main loop works well because the Z80 is in a HALT state.
    // While halted the Z80 cannot continuously request data from the Arduino via Z80:IN instructions.
    // Previously I used the ISR for sending data overwhelming the Arduino and causing transmition errors. 
        
    byte b = buffer[0]; // Pre-load the first byte (see INT0 for advancing the buffer index)
    PORTD = (PORTD & B00000100) | (b & B11111011);
    PORTC = (PORTC & B11111011) | (b & B00000100); 
    // Above is only needed for the very first time around... infact it could just set 'G'
    // Technically the last Z80:IN stalls and has to wait for the next new chunk.
    // Which means this [0] is never needed after the initial chuck.

    bufferSize = SIZE_OF_HEADER + bytesRead;
    bufferIndex = 0;

    if (destinationAddress == 0x4000) {
      releaseHalt();
    }

    bufferSize-=1;  // reading from index is always one ahead, stop early
    while ((bufferIndex-1) < bufferSize) { // -1 allow index to reach real end
      if ((bitRead(PINB, PINB0) == LOW) ) {
        // Z80 processor just read and is now halted, we can now  output future data
        // for next go around. These ports are picked for data alignment
        byte b = buffer[bufferIndex];  // bufferIndex no adjustment needed, reading ahead
        PORTD = (PORTD & B00000100) | (b & B11111011);  // 7bits sent preserving PORTD
        PORTC = (PORTC & B11111011) | (b & B00000100);  // 1bit sent preserving PORTC.
        bitClear(PORTC,DDC0); // this will un-halt the Z80.
        bitSet(PORTC,DDC0); // A0 signals the Z80 /NMI line,
      }
    }
    bufferSize = 0;
    destinationAddress += buffer[HEADER_PAYLOADSIZE];
  }

  //REG_I	 =00, REG_HL'=01, REG_DE'	=03, REG_BC' =05	(.sna file header)
  //REG_AF'=07, REG_HL =09, REG_DE	=11, REG_BC	 =13  (        27 bytes)
  //REG_IY =15, REG_IX =17, REG_IFF =19, REG_R	 =20 
  //REG_AF =21, REG_SP =23, REG_IM	=25, REG_BDR =26	 

  head27[0] = 'E';  // Execute command "EX"
  head27[1] = 'X';
  bufferSize = 27 + 2;
  bufferIndex = 0;
  byte b = head27[0];
  PORTD = (PORTD & B00000100) | (b & B11111011);
  PORTC = (PORTC & B11111011) | (b & B00000100);

  releaseHalt();

  bufferSize-=1; // again, one ahead (see above)
  while ((bufferIndex-1) < bufferSize) {
    if ((bitRead(PINB, PINB0) == LOW)) {
      b = head27[bufferIndex];
      PORTD = (PORTD & B00000100) | (b & B11111011);
      PORTC = (PORTC & B11111011) | (b & B00000100);
      bitClear(PORTC, DDC0);
      bitSet(PORTC, DDC0);
    }
  }

  while (bitRead(PINB, PINB0) == LOW) {}  // DEBUG - lock if we missed something

  dataFile.close();
  sd.end();
   while (1) { // DEBUG FLASH - END OF CODE WAS HIT OK
      pinMode(ledPin, OUTPUT);  
       digitalWrite(ledPin, HIGH);
        delay(200);         
        digitalWrite(ledPin, LOW);  
        delay(200);          
    }  
    
  //if (++fileIndex >= items) fileIndex = 0;

  delay(1000000);
}

void releaseHalt() {
  while ((bitRead(PINB, PINB0) != LOW)) {};
  // Once the input pin is LOW, toggle the output pin
  bitClear(PORTC,DDC0); // A0 signals the Z80 /NMI line,
  bitSet(PORTC,DDC0); // this will un-halt the Z80.
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
ISR(INT0_vect) {  // triggered by z80:IN
    bufferIndex++;
}





/*

z80 code for quick ref as of 4jul2024

;https://www.retronator.com/png-to-scr

; OPTIMISED CODE TIPS HERE:-
; https://www.smspower.org/Development/Z80ProgrammingTechniques

;https://github.com/konkotgit/ZX-external-ROM/blob/master/docs/zx_ext_eprom_512.pdf

; Contended Screen Ram
; On 48K Spectrums, the block of RAM between &4000 and &7FFF is contented, 
; that is access to the RAM is shared between the processor and the ULA. 

; ====================
; PORTS USED BY SYSTEM
; ====================
; ULA 		: ---- ---- ---- ---0  
;			  All even I/O  uses the ULA, officialy 0xFE is used.
; Keyboard 	: 0x7FFE to 0xFEFE
; Kempston  : ---- ---- 0001 1111  = 0x1F
;

WAIT EQU $8

org 0x8000
start:      
	DI
	
	im 1 ; Set interrupt mode 1

	LD HL,$5CB0 
    LD (HL),9
    INC HL      
    LD (HL),9

	;;ld hl,0x4000
	;;ld de,6912  ; data remaining is 6144 (bitmap) + 768 (Colour attributes) 

mainloop: 


	check_loop:  ; x2 bytes, header "GO"
	
	in a,($1f) 
	halt

	CP 'G'
	JR NZ, check_loop
	;;;LD BC,WAIT
    ;;;CALL DELAY 

	in a,($1f) 
	halt

	CP 'O'  
	JR NZ, check_loop
	; if we get here then we have found "GO" header 
	;;;LD BC,WAIT
    ;;;CALL DELAY

	; amount to transfer (small transfers, only 1 byte used)
 	in a,($1f)    ; 1 byte for amount    
	halt

    ld d, a   
	ld e, a       ; keep copy      
	;;;LD BC,WAIT
    ;;;CALL DELAY

	; dest (read 2 bytes) - Read the high byte
    in a,($1f)        
	halt

    ld H, a       ; 1st for high
	;;;LD BC,WAIT
    ;;;CALL DELAY
    in a,($1f)    ; 2nd for low byte
	halt

    ld L, a           
	;;;LD BC,WAIT
    ;;;CALL DELAY

screenloop:

	; ($1f) place on address buss bottom half (a0 to a7)
	; Accumulator top half (a8 to a15) - currently I don't care
	in a,($1f)   ; Read a byte from the I/O port
	halt

	ld (hl),a	 ; write to screen
    inc hl

	;;;LD BC,WAIT
   ;;; CALL DELAY

    dec d 	
    jp nz,screenloop
	
	; CRC CHECK AGIANT LAST DATA TRANSFER
;	ld d,0  ; not needed
;	or a
;	sbc hl,de ; rewind by amount copied
;	call _crc8b  ; a returns CRC-8
;
;	out ($1f),a  ; Send CRC-8 to arduino

; If the code fails, then nothing special needs to happen, as the data is just re-sent 
; using the same destination again. It will overwrite the failed transmission and get CRC checked again.
; *** This would be a good time to 'tune' and dial in the transmission speeds dynamically. 
; *** Could even start up with tuning first.

;;;;	LD BC,WAIT
;;;;    CALL DELAY

    jr mainloop
		
 DELAY:
    NOP
	DEC BC
	LD A,B
	OR C
	RET Z
	JR DELAY



;; =====================================================================
;; input - hl=start of memory to check, de=length of memory to check
;; returns - a=result crc
;; 20b
;; =====================================================================
_crc8b:
	xor a ; 4t - initial value of crc=0 so first byte can be XORed in (CCITT)
	ld c,$07 ; 7t - c=polyonimal used in loop (small speed up)
	_byteloop8b:
	xor (hl) ; 7t - xor in next byte, for first pass a=(hl)
	inc hl ; 6t - next mem
	ld b,8 ; 7t - loop over 8 bits
	_rotate8b:
	add a,a ; 4t - shift crc left one
	jr nc,_nextbit8b ; 12/7t - only xor polyonimal if msb set (carry=1)
	xor c ; 4t - CRC8_CCITT = 0x07
	_nextbit8b:
	djnz _rotate8b ; 13/8t
	ld b,a ; 4t - preserve a in b
	dec de ; 6t - counter-1
	ld a,d ; 4t - check if de=0
	or e ; 4t
	ld a,b ; 4t - restore a
	jr nz,_byteloop8b; 12/7t
	et ; 10t

Divide:                          ; this routine performs the operation BC=HL/E
  ld a,e                         ; checking the divisor; returning if it is zero
  or a                           ; from this time on the carry is cleared
  ret z
  ld bc,-1                       ; BC is used to accumulate the result
  ld d,0                         ; clearing D, so DE holds the divisor
DivLoop:                         ; subtracting DE from HL until the first overflow
  sbc hl,de                      ; since the carry is zero, SBC works as if it was a SUB
  inc bc                         ; note that this instruction does not alter the flags
  jr nc,DivLoop                  ; no carry means that there was no overflow
  ret
		
END start


; SAMPLE OF SPECCY ROM - LOCATION 0x0066
;;; RESET
;L0066:  PUSH    AF              ; save the
;        PUSH    HL              ; registers.
;        LD      HL,($5CB0)      ; fetch the system variable NMIADD.
;        LD      A,H             ; test address
;        OR      L               ; for zero.
;
;        JR      NZ,L0070        ; skip to NO-RESET if NOT ZERO
;
;        JP      (HL)            ; jump to routine ( i.e. L0000 )
;
;; NO-RESET
;L0070:  POP     HL              ; restore the
;        POP     AF              ; registers.
;        RETN   




; http://www.robeesworld.com/blog/33/loading_zx_spectrum_snapshots_off_microdrives_part_one

; https://www.seasip.info/ZX/spectrum.html

;----

;https://worldofspectrum.org/faq/reference/formats.htm

; https://github.com/oxidaan/zqloader/blob/master/z80snapshot_loader.cpp

; http://cd.textfiles.com/230/EMULATOR/SINCLAIR/JPP/JPP.TXT





;//use end connectors to fudge/repair speccy keyboard
;//4x3/4x5/1x6/1x4 Keys Matrix Keyboard Array Membrane Switch Keypad Keyboard UK


*/
