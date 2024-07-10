org 0x8040

start:      
	DI
;;;;;;	im 1 ; Set interrupt mode 1
;;;;	ld a,9
;;;;    LD ($5CB0),a   ; NMI will just return - changing 1byte is enough
  
  
	LD HL,$5CB0 
    LD (HL),9
    INC HL      
    LD (HL),9

mainloop: 

; First loop to check for either 'G' or 'E'
check_initial:  
    in a, ($1f)       ; Read input from port $1f
    halt              ; Halt to simulate waiting for the next input
    cp 'G'            ; Compare input with 'G'
    jr z, check_GO     ; If 'G', jump to check_G
    cp 'E'            ; Compare input with 'E'
    jr z, check_EX     ; If 'E', jump to check_E
    jr check_initial  ; Otherwise, keep checking

; Subroutine to handle 'GO' command
check_GO:
    in a, ($1f)       ; Read input from port $1f
    halt              ; Halt to simulate waiting for the next input
    cp 'O'            ; Compare input with 'O'
    jr nz, check_initial ; If not 'O', go back to initial check
    jp command_GO     ; If 'O', jump to command_GO

; Subroutine to handle 'EX' command
check_EX:
    in a, ($1f)       ; Read input from port $1f
    halt              ; Halt to simulate waiting for the next input
    cp 'X'            ; Compare input with 'X'
    jr nz, check_initial ; If not 'X', go back to initial check

command_EX:  
	ld SP,TINY_STACK  ; ONLY 1 PUSH!
	ld c,$1f

	ld a,2
	out ($fe), a

	in h,(c)   ;0
	halt
	in l,(c)   ;1
	halt
	in d,(c)   ;2
	halt
	in e,(c)   ;3
	halt
	
	in b,(c)   ;4 
	halt
	in c,(c)   ;5 
	halt
	push BC
	pop AF	

	ld c,$1f
	in b,(c)   ;6
	halt
	in c,(c)   ;7	
	halt
	ex	af,af'
	exx		; Alternates restored

	ld c,$1f
	in h,(c)  ;8
	halt
	in l,(c)  ;9
	halt
	in d,(c)  ;10
	halt
	in e,(c)  ;11
	halt

	in b,(c)  ;12 - temp
	halt
	in c,(c)  ;13 - temp
	halt
	push bc
	pop IY
	
	ld c,$1f
	in b,(c)  ;14 - temp
	halt
	in c,(c)  ;15 - temp
	halt
	push bc
	pop IX

;;;	in b,(c)   dont restore BC yet, keep as working spare
;;;	in c,(c)
	
	; Restore interrupt mode
	in a,($1f)  ;16
	halt
	or	a
	jr	nz,notim0
	im	0
	jr	setIM
notim0:	dec	a
	jr	nz,notim1
	im	1
	jr	setIM
notim1:	im	2

 	; Restore 'I'
setIM: in a,($1f)   ;17  
	halt
	ld	i,a

	; Restore interrupt enable flip-flop (IFF) 
	in a,($1f)  	;18 - REG_IFF
	halt
	AND	%00000100
	jr	z,skipEI
	EI	 ; restore Interrupt (this code uses DI - so if needed we only need to enable)
skipEI:

; R REG ... TODO  
	in A,($1f)   ;19 - read but ignore for now
	halt

	; Restore AF
	ld c,$1f
	in b,(c) ;20
	halt
	in c,(c) ;21
	halt
	push BC
	pop AF

	; Restore SP
	ld c,$1f
	in B,(c)	;22 - SP - high byte
	halt
	in C,(c)	;23 - SP - low byte
	halt
	push BC
	pop BC
	ld SP,(TINY_STACK-2)

	; Restore BC register pair
	ld c,$1f
	in b,(c)   ;24 
	halt
	in c,(c)   ;25
	halt

	;26 col ignore for now - don't even read

	RETN  ; PC ready on stack! return to start snapshot code

command_GO:

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
	in a,($1f)   ; Read a byte from the I/O port
	ld (hl),a	 ; write to screen
    inc hl
	halt 		 ; DO THIS LAST CRITICAL HALT AFTER MEMORY WRITE

    dec d 	
    jp nz,screenloop

    jp mainloop

db 0,0
TINY_STACK:  ; push decrements stack
	


END start


; SAMPLE OF SPECCY ROM - LOCATION 0x0066 - "NMI"
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




; Use end connectors to fudge/repair speccy keyboard
; GOOGLE THIS:
; 4x3/4x5/1x6/1x4 Keys Matrix Keyboard Array Membrane Switch Keypad Keyboard UK