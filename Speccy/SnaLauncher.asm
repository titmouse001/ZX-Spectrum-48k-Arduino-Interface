;******************************************************************************
;   CONSTANTS
;******************************************************************************
SCREEN_END			EQU $5AFF
FUTURE_STACK_BASE  	EQU $5AFF-4 ; one push later
SCREEN_ATRIBUTES    EQU $5800
;SCREEN_START		EQU $4000

;******************************************************************************
;   MACRO SECTION
;******************************************************************************
MACRO READ_ACC_WITH_HALT 
    in a, ($1f)  
    halt     
ENDM
MACRO READ_PAIR_WITH_HALT  ,reg1,reg2
    in reg1,(c)
    halt
    in reg2,(c)
    halt
ENDM
MACRO SET_BORDER ,colour  ; DEBUG
	;0 Black, 1 Blue, 2 Red, 3 Magenta, 4 Green, 5 Cyan, 6 Yellow, 7 White		
	ld a,colour
	AND %00000111
	out ($fe), a
ENDM

;******************************************************************************
;   CODE START
;******************************************************************************

ORG $0000
L0000:  
		DI                     
       	; Stack goes down from SCREEN_END. 
    	ld SP,SCREEN_END 	; using screen attribute area works are a bonus debugging tool!
		SET_BORDER 4
	
mainloop: 
		ld C,$1F  	; Initial setup for use with the IN command, i.e. "IN <REG>,(c)" 
		HALT     ; Let the Arduino signal when ready to start sending
;**************************************************************************************************
; At start of each chunck transfer, the first 2 bytes are checked for 'action' indicators.
; ACTIONS ARE:-
;  - Transfer data = "GO"
;  - Execute code = "EX"
;
check_initial:   		; First loop to check for either 'G' or 'E'
	READ_ACC_WITH_HALT
    cp 'G'            
    jr z, check_GO     	; If 'G', jump to check_G
    cp 'E'            
    jr z, check_EX     	; If 'E', jump to check_E
    jr check_initial   	; Otherwise, keep checking
check_GO:  				; Subroutine to handle 'GO' command
	READ_ACC_WITH_HALT
    cp 'O'            	; Compare input with 'O'
    jr nz, check_initial ; If not 'O', go back to initial check
    jp command_GO     	; If 'O', jump to command_GO
check_EX:  				; Subroutine to handle 'EX' command
	READ_ACC_WITH_HALT
    cp 'X'            
    jr nz, check_initial ; If not 'X', go back to initial check
	JP command_EX

;******************************************************************************
; MASKABLE INTERRUPT
ORG $0038
L0038:   
		SET_BORDER 2
        EI 
		RET
;******************************************************************************
; NON MASKABLE INTERRUPT
ORG $0066
L0066:
		RETN
;******************************************************************************

command_GO:  ; FIRST STAGE - TRANSFER DATA
	
	; The actual maximum transfer size is smaller than 1 byte; it's really 250,
	; as 'amount', 'destination', and "GO" are included in each transfer chunk.
	; Transfers here are sequential, but any destination location can be targeted.
	in b,(c)  ; transfer size (1 byte)
	HALT

	; Set HL with the 'destination' address (reading 2 bytes)
	READ_PAIR_WITH_HALT h,l

readDataLoop:
	; liking the boarder effect, so going with 'in' not 'ini' as need value in Acc
	in a,($1f)   ; Read a byte from the I/O port
	ld (hl),a	 ; write to screen
    inc hl
    SET_BORDER a ; DEBUG for now... may keep it!
	halt 		 ; do this one after memory write (so final write does not miss out)
    djnz readDataLoop

    jr mainloop

;**************************************************************************************************

command_EX:  ; SECOND STAGE - Restore snapshot states & execute stored jump point from the stack

	; Relocate code to run in screen memory
	LD HL,relocate
	;;;;;;LD DE,SCREEN_START
	LD DE,SCREEN_ATRIBUTES  ; DEBUG
	LD BC, relocateEnd - relocate
	LDIR

	; Restore HL',DE',AF',BC'
	READ_PAIR_WITH_HALT h,l
	READ_PAIR_WITH_HALT d,e ; use this one for scratch, not BC ?????
	READ_PAIR_WITH_HALT b,c
	push BC
	pop AF	
	ld c,$1f  ; just trashed BC - reload C as needed for more READ_PAIR IN's
	READ_PAIR_WITH_HALT b,c
	ex	af,af'	; Alternate AF' restored
	exx			; Alternates registers restored

	; Restore HL,DE,IY,IX
	ld c,$1f  ; EXX used - renew c
	READ_PAIR_WITH_HALT h,l
	READ_PAIR_WITH_HALT d,e
	READ_PAIR_WITH_HALT b,c
	push bc
	pop IY
	ld c,$1f
	READ_PAIR_WITH_HALT b,c
	push bc
	pop IX


	; Restore interrupt mode; IM0, IM1 or IM2
	READ_ACC_WITH_HALT
	or	a
	jr	nz,notim0
	im	0
	jr	IMset
notim0:	
	dec	a
	jr	nz,notim1
	im	1
	jr	IMset
notim1:	
	im	2
IMset:

 	; Restore 'I'
	READ_ACC_WITH_HALT
	ld	i,a

	; Restore interrupt enable flip-flop (IFF) 
	;;;;;;;;;;;;;;;;;;;;;READ_ACC_WITH_HALT
	in a, ($1f)     
	AND	%00000100
	jr	z,skip_EI
	EI	 ; restore Interrupt
skip_EI:
	halt     

	; R REG ... TODO  
	READ_ACC_WITH_HALT ; currently goes to the void

	; Restore AF
	ld c,$1f
	READ_PAIR_WITH_HALT b,c
	push BC
	pop AF

	; Restore SP
	ld c,$1f
	READ_PAIR_WITH_HALT b,c
	push BC
	push BC
	push BC
	pop BC
	pop BC
	pop BC
;;;;;;;;;	ld SP,(FUTURE_STACK_BASE)  

	; Restore BC register pair
;;;;	ld c,$1f
;;;;	READ_PAIR_WITH_HALT b,c

	; NOTE: Reading the border colour is ignored for now - header byte 26 is not sent by the Arduino

	JP SCREEN_ATRIBUTES

relocate:

	ld c,$1f
	READ_PAIR_WITH_HALT b,c

;;;;	HALT	; LAST HALT NOTED ARDUINO SIDE - /ROMCS will be set LOW (use stock rom)	NOP

	ld SP,(FUTURE_STACK_BASE)  
	
	RETN

relocateEnd:

;	ld d,0
;	OR A  
;	SBC HL, DE
;	call _crc8b


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
ret ; 10


org $3ff0
debug:
	SET_BORDER a
	inc a
	jr debug

last:
DS  16384 - last




; todo
; rename "EX" to "SN" 
; "SN" - restore SNapshot regs, but not do the exe.
; "EX" now just EXecute JMP <LOCATION>
; "GO" needs renaming ... maybe, "GD" get data