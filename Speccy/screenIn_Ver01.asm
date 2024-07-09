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

	in a,($1f) 
	halt
	CP 'O'  
	JR NZ, check_loop
	; if we get here then we have found "GO" header 

	; amount to transfer (small transfers, only 1 byte used)
 	in a,($1f)    ; 1 byte for amount    
	halt

    ld d, a   
	ld e, a       ; keep copy      

	; dest (read 2 bytes) - Read the high byte
    in a,($1f)    ; 1st for high  
	halt
    ld H, a      
    in a,($1f)    ; 2nd for low byte
	halt
    ld L, a           

screenloop:

	; ($1f) place on address buss bottom half (a0 to a7)
	; Accumulator top half (a8 to a15) - currently I don't care

	in a,($1f)   ; Read a byte from the I/O port
	; At this point it could technically be the last byte.
	; Meaing the Arduino does not have to un-halt and this could stay
	; halted for a while, i.e last byte will be VERY LATE, depending on what 
	; other things come after if anything at all.  
	; Long story short - "LAST CRITICAL HALT - DO AFTER MEMORY WRITE"

	ld (hl),a	 ; write to screen
    inc hl
	halt 		 ; DO THIS LAST CRITICAL HALT AFTER MEMORY WRITE

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