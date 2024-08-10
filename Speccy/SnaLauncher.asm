;******************************************************************************
;   CONSTANTS
;******************************************************************************
SCREEN_START			EQU $4000	  ; USE ME FOR FINAL BUILD LOCATION	
;SCREEN_ATTRIBUTES   	EQU $5800+32  ; DEBUG LOCATION
;SCREEN_END				EQU $5AFF
WORKING_STACK			EQU $57FF

TEMP_STORAGE			EQU SCREEN_START

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
  
    ; Stack goes down
	ld SP,WORKING_STACK 	; using screen attribute area works are a bonus debugging tool!
	
	jp ClearScreen
	
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
     ;;;;;   EI  
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
	
 	; Setup - Later on the code will using a tiny footprint in screen memory for 
	;	      running code & temporary storage during the restoration process.
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
	push de					  ; store DE	
	READ_PAIR_WITH_HALT e,d;  ; read BC	  
	push de  			      ; store BC
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
	push de 				  ; store AF

	; Store Stack Pointer
	READ_PAIR_WITH_HALT e,d;  ; get Stack
	ld (TEMP_STORAGE),de 	  ; store sack

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

	; Self-modifying code
	ld SP,(TEMP_STORAGE)		
	pop de
	ld (SCREEN_START + (JumpInstruction - relocate) +1),de

	; restore final registers - we know how mutch is on the stack
	ld SP,WORKING_STACK-(2+2+2)
	POP af   			 ; Restore AF register pair
	POP bc   			 ; Restore BC register pair
	POP de   			 ; Restore DE register pair	

	ld SP,(TEMP_STORAGE)		 ; Restore the program's stack pointer
	inc sp 						 ; skip PC kept here as part of snapshot protocol
	inc sp						 ; (PC skipped as we have it place in the 'JumpInstruction')

	; Note: WORKING_STACK would have gone 4 deep over its lifetime,
	;       with 3 push/pop operations, including the NMI 'RETN'.

	; Self-modifying code - restore memory used as temp storage
	; (note : at this point I would happly use up 1k of rom just to save a single byte in memory!!!!)
	ld (WORKING_STACK),A
	ld a,$76				;	HALT
	ld (SCREEN_START),a
	ld a,$0					;   NOP
	ld (SCREEN_START+1),a
	ld A,(WORKING_STACK)

	EI
	HALT 				 ; Maskable - Interupt routine forgoes a 'EI',
	EI					 ; so we need to re-enable ask maskable does not do this
	JP SCREEN_START		 ; jump to relocated code
	
;-----------------------------------------------------------------------	
; *** Start-up restored program ***
; This gets placed in screen memory (called via JP SCREEN_START)
relocate:
;;;;	EI  ; 'EI' TO BE MOVED INTO ROM, BUT WILL ANOTHER HALT TO MAKE SURE MASKABLE IS NOT HIT DURING ROM EXECUTION
	HALT  ; Uses maskable - after there should be ample time if 'EI' is restored.
		  ; Arduino waits for last halt to signal in 'Upper Rom'
NOP_LABLE:
	NOP 				; This will be modified to EI if needed
JumpInstruction: 		; Self-modifying code
    jp 0000h            ; This jump location will be modified 
relocateEnd:
;-----------------------------------------------------------------------	
;$4000
;$5800
;$5B00

;768 attribute space

ClearScreen:
   	ld a, 0              ;attributte
    ld hl, 16384         ;pixels
    ld de, 16385         ;pixels + 1
    ld bc, 6144          ;pixel area
    ld (hl), a           ;set first byte to '0' as HL = 16384 = $4000  therefore L = 0
    ldir                 ;copy bytes
    ld bc, 767           ;attribute area length - 1
    ld (hl), a           ;set first byte to attribute value
    ldir                 ;copy bytes
    jp mainloop  		 ; don't use call/ret ... !!!NEED TO AVOID USING STACK!!!

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