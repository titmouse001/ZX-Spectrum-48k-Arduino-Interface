;******************************************************************************
;   CONSTANTS
;******************************************************************************
SCREEN_START			EQU $4000	  ; USE ME FOR FINAL BUILD LOCATION	
;SCREEN_ATTRIBUTES   	EQU $5800+32  ; DEBUG LOCATION
;SCREEN_END				EQU $5AFF
WORKING_STACK			EQU $57FF

;******************************************************************************
;   MACRO SECTION
;******************************************************************************
MACRO READ_ACC_WITH_HALT 
	halt
    in a, ($1f)  
ENDM
MACRO READ_PAIR_WITH_HALT  ,reg1,reg2
	halt
    in reg1,(c)
	halt
    in reg2,(c)
ENDM
MACRO SET_BORDER ,colour 
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
  
	CALL ClearScreen

	; Stack goes down
	ld SP,WORKING_STACK 	; using screen attribute area works are a bonus debugging tool!
	
mainloop: 

;******************************************************************************
; For each incoimg chunck of data, the first 2 bytes are 'action' indicators.
;  # Transfer data = "GO"
;  # Execute code = "EX"
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
; MASKABLE INTERRUPT - (EI HAS TO BE CALLED BEFORE RETN - SO PLAYING IT SAFE)
ORG $0038
L0038:   
        EI  
		RETI
;******************************************************************************
; NON MASKABLE INTERRUPT - USED TO UN-HALT 
ORG $0066
L0066:
		RETN
;******************************************************************************

command_GO:  ; FIRST STAGE - TRANSFER DATA
	
	ld C,$1F  	; Initial setup for use with the IN command, i.e. "IN <REG>,(c)" 

	; The actual maximum transfer size is smaller than 1 byte; it's really 250,
	; as 'amount', 'destination', and "GO" are included in each transfer chunk.
	; Transfers are typically sequential, but any destination location can be targeted.
	halt
	in b,(c)  ; transfer size (1 byte)

	; Set HL with the 'destination' address (reading 2 bytes)
	READ_PAIR_WITH_HALT h,l

readDataLoop:
	; liking the boarder effect, so going with 'in' not 'ini' as need value in Acc
	halt
	in a,($1f)   ; Read a byte from the I/O port
	ld (hl),a	 ; write to screen
    inc hl
    
	AND %00000111  ; set border 0 to 7
	out ($fe), a   ; 

    djnz readDataLoop

    jr mainloop

command_EX:  ; SECOND STAGE - Restore snapshot states & execute stored jump point from the stack
	
;;;;SET_BORDER 6

	; Relocate code to run in screen memory
	LD HL,relocate
	LD DE,SCREEN_START 
	LD BC, relocateEnd - relocate
	LDIR
	
 	; Restore 'I'
	READ_ACC_WITH_HALT
	ld	i,a

	ld c,$1f  
	; Restore HL',DE',BC',AF'
	READ_PAIR_WITH_HALT L,H 
	READ_PAIR_WITH_HALT E,D 
	push de
	READ_PAIR_WITH_HALT c,b  
	READ_PAIR_WITH_HALT e,d  ; spare to read AF
	push de
	pop af
	pop de		
	ex	af,af'				; Alternate AF' restored
	exx						; Alternates registers restored

	ld c,$1f  
	; Restore HL,DE,BC,IY,IX	
	READ_PAIR_WITH_HALT l,h   ; restore HL
	READ_PAIR_WITH_HALT e,d;  ; read DE
;	ld ($5800+2),de			  ; store DE				 
	push de
	READ_PAIR_WITH_HALT e,d;  ; read BC
;	ld ($5800+4),de			  ; store BC
	push de
	READ_PAIR_WITH_HALT e,d;  ; read IY
	push de
	pop IY					  ; restore IY
	READ_PAIR_WITH_HALT e,d;  ; read IX
	push de
	pop IX					  ; restore IX

	; Restore interrupt enable flip-flop (IFF) 
	READ_ACC_WITH_HALT		 ; read IFF
	AND	%00000100   		 ; get bit 2
	jr	z,skip_EI
	; If bit 2 is 1, modify instruction NOP to EI (opcode $FB)
	ld a,$FB
	ld (SCREEN_START+(NOP_LABLE-relocate)),a  ; IM Opcode = $FB
skip_EI:

	; R REG ... TODO  
	READ_ACC_WITH_HALT ; currently goes to the void

	; restore AF
	READ_PAIR_WITH_HALT e,d;  ; using spare to restore AF
;	ld ($5800+6),de			  ; store AF
	push de

	; Store Stack Pointer
	READ_PAIR_WITH_HALT e,d;  ; get Stack
	ld ($5800),de 			  ; store sack

	; Restore IM0, IM1 or IM2  
	READ_ACC_WITH_HALT		  ; read IM (interrupt mode)
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

	READ_ACC_WITH_HALT   ; Border Colour
	AND %00000111		 
	out ($fe),a			 ; set border

;	ld de,($5800+6)	     ; Restore AF register pair
;	PUSH de
;	POP AF 

	POP af
	POP bc
	POP de

;	ld bc,($5800+4)	 	 ; Restore BC register pair
;	ld de,($5800+2)		 ; Restore DE register pair
	ld SP,($5800)		 ; Restore the program's stack pointer

	JP SCREEN_START
	
;-----------------------------------------------------------------------	

ClearScreen:
     xor a
     ld hl, 16384         ;first byte of pixel area
     ld c, 6              ;6 * (256 * 4) = 6144
loop2
     ld b, a              ;set B to zero it will cause 256 repeations of loop
loop1
     ld (hl), a           ;set byte to zero
     inc l                ;move to the next byte
     ld (hl), a
     inc l
     ld (hl), a
     inc l
     ld (hl), a
     inc hl               ;this time we are not sure that inc l will not cause overflow
     djnz loop1           ;repeat for next 4 bytes
     dec c
     jr nz, loop2         ;outer loop. repeat for next 1024 bytes.
	 ret;

;-----------------------------------------------------------------------	
relocate:
	HALT		 		; "last halt" notifies we need original rom
NOP_LABLE:
	NOP 				 ; This will be modified to EI if needed
	RETN				 ; Start program
relocateEnd:


;**************************************************************************************************

org $3ff0
debug:   			; debug trap
	SET_BORDER a
	inc a
	jr debug

last:
DS  16384 - last


;******************************************************
; todo
; rename "EX" to "SN" 
; "SN" - restore SNapshot regs, but not do the exe.
; "EX" now just EXecute JMP <LOCATION>
; "GO" needs renaming ... maybe, "GD" get data
;******************************************************

;******************************************************
;  SNA Header (27 bytes)
;  REG_I	00
;  REG_HL1	01	;HL'
;  REG_DE1	03	;DE'
;  REG_BC1	05	;BC'
;  REG_AF1	07	;AF'
;  REG_HL	09
;  REG_DE	11
;  REG_BC	13
;  REG_IY	15
;  REG_IX	17
;  REG_IFF  19 
;  REG_R	20 
;  REG_AF	21
;  REG_SP	23
;  REG_IM	25 
;  REG_BDR	26	 
;******************************************************