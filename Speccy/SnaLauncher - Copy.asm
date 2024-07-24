;******************************************************************************
;   CONSTANTS
;******************************************************************************
SCREEN_START		EQU $4000	 
SCREEN_ATTRIBUTES   EQU $5800
SCREEN_END			EQU $5AFF

RELOCATED_NOP		EQU SCREEN_START+1  ; nop modifed
RELOCATED_CODE		EQU SCREEN_START 

STORE_SP			EQU SCREEN_ATTRIBUTES-2
STORE_DE			EQU SCREEN_ATTRIBUTES-4
STORE_BC			EQU SCREEN_ATTRIBUTES-6
STORE_AF			EQU SCREEN_ATTRIBUTES-8

TEMP_STACK			EQU SCREEN_ATTRIBUTES-32 ; next line up (helps debug)

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
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
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
    	ld SP,TEMP_STACK 	; using screen attribute area works are a bonus debugging tool!
		;SET_BORDER 4

mainloop: 
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
		;;;SET_BORDER 2
        EI 
		RET
;******************************************************************************
; NON MASKABLE INTERRUPT
ORG $0066
L0066:
		RETN
;******************************************************************************

command_GO:  ; FIRST STAGE - TRANSFER DATA
	
	ld C,$1F  	; Initial setup for use with the IN command, i.e. "IN <REG>,(c)" 

	; The actual maximum transfer size is smaller than 1 byte; it's really 250, as 'amount', 'destination' and "GO" 
	; are included in each transfer chunk. Transfers here are sequential, but any destination location can be targeted.
	in b,(c)  ; transfer size (1 byte)
	HALT

	; Set HL with the 'destination' address (reading 2 bytes)
	READ_PAIR_WITH_HALT h,l

readDataLoop:
	; liking the boarder effect, so going with 'in' not 'ini' as need value in Acc
	in a,($1f)   ; Read a byte from the I/O port
	ld (hl),a	 ; write to screen
    inc hl
    
	;;;;SET_BORDER L
	;;;;;AND %00000111  ; set border 0 to 7
	AND %00000101  ; 
	out ($fe), a   ; 

	halt 		 ; do this one after memory write (so final write does not miss out)
    djnz readDataLoop

    jp mainloop

;**************************************************************************************************
;  SNA Header (27 bytes)
;  REG_I	00
;  REG_HL'	01
;  REG_DE'	03
;  REG_BC'	05
;  REG_AF'	07
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

command_EX:  ; SECOND STAGE - Restore snapshot states & execute stored jump point from the stack
	
	; Relocate code to run in screen memory
	LD HL,relocate
	LD DE,RELOCATED_CODE 
	LD BC,relocateEnd - relocate
	LDIR
	
 	; Restore 'I'
	READ_ACC_WITH_HALT ; <1>
	ld	i,a

	ld c,$1f  
	; Restore HL',DE',BC',AF'
	READ_PAIR_WITH_HALT L,H  ; <3>
	READ_PAIR_WITH_HALT E,D  ; <5>
	push de
	READ_PAIR_WITH_HALT c,b  ; <7>
	READ_PAIR_WITH_HALT e,d  ; <9> spare to read AF
	push de
	pop af
	pop de		
	ex	af,af'				; Alternate AF' restored
	exx						; Alternates registers restored

	ld c,$1f  
	; Restore HL,DE,BC,IY,IX	
	READ_PAIR_WITH_HALT l,h   ; <11> restore HL
	READ_PAIR_WITH_HALT e,d;  ; <13> read DE
	ld (STORE_DE),de		  ; store DE				 
	READ_PAIR_WITH_HALT e,d;  ; <15> read BC
	ld (STORE_BC),de		  ; store BC
	READ_PAIR_WITH_HALT e,d;  ; <17> read IY
	push de
	pop IY					  ; restore IY
	READ_PAIR_WITH_HALT e,d;  ; <19> read IX
	push de
	pop IX					  ; restore IX

	; Restore interrupt enable flip-flop (IFF) 
	READ_ACC_WITH_HALT		  ; <20> read IFF
	AND	%00000100   		  ; get bit 2
	jr	z,skip_EI
	; If bit 2 is 1, modify instruction NOP to EI (opcode $FB)
	ld a,$FB
	ld (RELOCATED_NOP),a  	  ; IM Opcode = $FB
skip_EI:

	; R REG ... TODO  
	READ_ACC_WITH_HALT 		  ; <21>  currently goes to the void

	; restore AF
	READ_PAIR_WITH_HALT e,d;  ; <23>  using spare to restore AF
	ld (STORE_AF),de		  ; store AF

	; Store Stack Pointer
	READ_PAIR_WITH_HALT e,d;  ; <25> get Stack
	ld (STORE_SP),de 		  ; store sack

	; Restore IM0, IM1 or IM2  
	READ_ACC_WITH_HALT		  ; <26> read IM (interrupt mode)
	or	a
	jr	nz,not_im0
	im	0
	jr	doneIM
not_im0:	
	dec	a
	jr	nz,not_im1
	im	1
	jr	doneIM
not_im1:	
	im	2
doneIM:

	READ_ACC_WITH_HALT   	; <27> Border Colour
	AND %00000101		 
	out ($fe),a			 	; set border

	ld de,(STORE_AF)	 	; Restore AF register pair
	PUSH de
	POP AF 

	ld bc,(STORE_BC)	 ; Restore BC register pair
	ld de,(STORE_DE)	 ; Restore DE register pair

	JP RELOCATED_CODE

relocate:
	HALT
	NOP 				 ; RELOCATED_NOP - This will be modified to EI if needed
	ld SP,(STORE_SP)	 ; Restore the program's stack pointer
	RETN				 ; Start program
relocateEnd:


;**************************************************************************************************


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