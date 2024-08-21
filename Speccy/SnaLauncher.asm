; NOTES:-
;            rom: $0000
;        rom end: $3fff
;         screen: $4000  [size:6144] 
;     screen end: $57FF
;     attributes: $5800  [size:768]
; attributes end: $5AFF 
;
;******************************************************************************
;   CONSTANTS
;******************************************************************************
SCREEN_START				EQU $4000  ; [6144]
SCREEN_END					EQU $57FF
SCREEN_ATTRIBUTES_START   	EQU $5800  ; [768]
SCREEN_ATTRIBUTES_END   	EQU $5AFF  
WORKING_STACK				EQU  (SCREEN_START+(TempStack-relocate))
;WORKING_STACK				EQU SCREEN_ATTRIBUTES_END+1  ; DEBUG STACK

;******************************************************************************
;   MACRO SECTION
;******************************************************************************

;--------------------------------
MACRO READ_ACC_WITH_HALT 
	halt
    in a, ($1f)  
ENDM
;--------------------------------
; Note: With READ_PAIR_WITH_HALT don't used BC directly, do this...
;   READ_PAIR_WITH_HALT e, d   ; Get BC (temporarily in DE)
;   push de                    ; Save DE (holds BC)
;   pop bc                     ; BC restored
;
; DONT FORGET TO "ld C,$1F" BEFORE USING
MACRO READ_PAIR_WITH_HALT  ,reg1,reg2  
	halt
    in reg1,(c)  
	halt
    in reg2,(c)
ENDM
;--------------------------------
MACRO SET_BORDER ,colour 
 	;0 Black, 1 Blue, 2 Red, 3 Magenta, 4 Green, 5 Cyan, 6 Yellow, 7 White		
	ld a,colour
	AND %00000111
	out ($fe), a
ENDM
;--------------------------------

;******************************************************************************
;   CODE START
;******************************************************************************

ORG $0000
L0000:  
	DI                     
    ; Stack goes down
	; Note: The attributes area for the stack can also be a helpful debugging tool when needed.
	ld SP,WORKING_STACK
	jp ClearScreen
mainloop: 

;******************************************************************************
; For each incoming chunck of data, the first 2 bytes are 'action' indicators.
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
     ;;;;;   EI    ; LEAVING THIS 'EI' OUT, NEED TO GET IN/OUT AS QUICKLY AS POSSIBLE
	;;	RETI
		RET
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
    
	;AND %00000111  ; set border 0 to 7
	AND %00000011  ;
	out ($fe), a   ; 

    djnz readDataLoop

    jr mainloop

command_EX:  ; SECOND STAGE - Restore snapshot states & execute stored jump point from the stack
	
	;-------------------------------------------------------------------------------------------
 	; Setup - put copy of ROM routine 'relocate' into Screen memory		
	LD HL,relocate
	LD DE,SCREEN_START 
	LD BC, relocateEnd - relocate
	LDIR   		
	; UPDATE: This area is now mostly used as temporary memory and is destroyed; this block copy
	;         serves to illustrate the intended use. This memory is restored later; for now, it can be put
	;         to good use since it is not being used just yet.
	;-------------------------------------------------------------------------------------------
	
 	; Restore 'I'
	READ_ACC_WITH_HALT
	ld	i,a

	ld c,$1f  
	; Restore HL',DE',BC',AF'
	READ_PAIR_WITH_HALT L,H  ; store HL'
	READ_PAIR_WITH_HALT e,d  ; store DE'
	READ_PAIR_WITH_HALT e,d  ; Get BC' (temporarily in DE)
    push de                  ; Save DE (holds BC')
    pop bc                   ; BC' restored
	exx						 ; Alternates registers restored
	; registers are free again. just AF' left to restore
	ld c,$1f  
	READ_PAIR_WITH_HALT e,d  ; spare to read AF' - save flags last
	push de					 ; store AF'
	pop af					 ; Restore AF'
	ex	af,af'				 ; Alternate AF' restored


	ld c,$1f  
	; Restore HL,DE,BC,IY,IX	
	READ_PAIR_WITH_HALT l,h   ; restore HL
	
	READ_PAIR_WITH_HALT e,d;  ;  TO VOID -  read DE		  			 
	READ_PAIR_WITH_HALT e,d;  ;  TO VOID -  read BC	  

	READ_PAIR_WITH_HALT e,d;  ; read IY
	push de
	pop IY					  ; restore IY
	READ_PAIR_WITH_HALT e,d;  ; read IX
	push de
	pop IX					  ; restore IX

	jp RestoreInterruptEnableState
RestoreInterruptComplete:

	; R REG ... TODO  
	READ_ACC_WITH_HALT ; currently goes to the void

	READ_PAIR_WITH_HALT e,d;  ; TO VOID - using spare to restore AF

	;------------------------------------------------------------------------
	; Store Stack Pointer
	READ_PAIR_WITH_HALT e,d;  ; get Stack
	ld (SCREEN_START+(spare-relocate)),de 	
	ld SP,(SCREEN_START+(spare-relocate))
	pop de
	; Store programs start address, uses self-modifying code into screen memory
	ld (SCREEN_START + (JumpInstruction - relocate) +1),de  ; set jump to address
	dec sp
	dec sp
	;------------------------------------------------------------------------

	;------------------------------------------------------------------------
	READ_ACC_WITH_HALT  ; gets value of IM into reg-A
	JP RESTORE_INTERRUPT_MODE
IM_SETUP_COMPLETE:
	;------------------------------------------------------------------------

	;------------------------------------------------------------------------
	READ_ACC_WITH_HALT   ; Border Colour
	AND %00000111		 
	out ($fe),a			 ; set border
	;------------------------------------------------------------------------

	;------------------------------------------------------------------------
	; Restore DE & BC
    READ_PAIR_WITH_HALT e, d   ; Restore DE directly
    READ_PAIR_WITH_HALT e, d   ; Get BC (temporarily in DE)
    push de                    ; Save DE (holds BC)
    pop bc                     ; BC restored
	;------------------------------------------------------------------------

	;------------------------------------------------------------------------
	; Restore AF - With just register A left to use, this step needs to be creative
	READ_ACC_WITH_HALT     		; 'F' is now in A
	ld (SCREEN_START+(TempVar-relocate)),a 

	READ_ACC_WITH_HALT     		; 'A' is now in A (will cause PUSH/POP)
	ld (WORKING_STACK-1),a    	; Store A 

	ld a,(SCREEN_START+(TempVar-relocate))
	ld (WORKING_STACK-2),a     	; Store F 

	pop af                 		; restore AF
	dec sp
	dec sp						; need re-aim stack we just borrowed
	; NOTE: 'READ_PAIR_WITH_HALT' will alter the flags (uses 'IN'), so we restore AF last.
	;------------------------------------------------------------------------

	;------------------------------------------------------------------------
	; Arduio just waits for the halt line - masable used this time
	EI
	HALT 				 ; Maskable 50FPS Interupt routine ($0038) forgoes 'EI',
	EI					 ; Need to re-enable as our maskable does not.
	JP SCREEN_START		 ; jump to relocated code in screen memory
	;------------------------------------------------------------------------

	
;-----------------------------------------------------------------------	
; TOTAL STACK USAGE - 1 deep (2 bytes) - try avoiding anything that makes use of the stack.
; To help reduce deep, don't:-  (i) Push,  (ii)NMI , (iii) pop (results in a 4 deep!)
; Interrupts triggered by HALT use the stack, so push/pop need to planned around.
;-----------------------------------------------------------------------	
; MEMORY USAGE - The entire sequence 
; 'relocate:' to 'relocateEnd:' uses 5 bytes of memory.
; (HALT: 1 byte, NOP: 1 byte, jp 0000h: 3 bytes)
;-----------------------------------------------------------------------
; This means 11 bytes of screen memory need to tbe destored
;-----------------------------------------------------------------------

;-----------------------------------------------------------------------	
; *** Start-up restored program ***
;-----------------------------------------------------------------------	
;
; This gets placed in screen memory (called via JP SCREEN_START)
;
relocate:
  	HALT  ; Here we just wait for a maskable interrupt.
          ; The Arduino waits for this halt as a signal to swap over to 'Upper ROM'.
          ; Note: During HALT, the interrupt service routine will use the stack.
          ; At this point, we are using the real restored program's stack space.
IM_LABLE:
	IM 0				; Self-modifying code - 'IM <n>'
NOP_LABLE:
	NOP 				; Self-modifying code - To 'EI' if flagged from snapshot
	inc sp 						 
	inc sp	
JumpInstruction: 		
    jp 0000h            ; Self-modifying code - JP location will be modified 
spare:				; Used to keep real stacks starting pointer
	db $7f,$7f		; Double perpose, see top & bottom comments (both labels reflect usage intent)
TempStack:			; Used at startup until real stack is restored
TempVar:
	db $7f
relocateEnd:

; ABOVE: "76 ED 46 00 33 33 C3 00 00 7F 7F"  (11 bytes used)

;-----------------------------------------------------------------------	
ClearScreen:
	SET_BORDER 0
   	ld a, 0              
    ld hl, 16384       
    ld de, 16385        
    ld bc, 6144       
    ld (hl), a           ;setup
    ldir                
    ld bc, 768-1
    ld (hl), a  
    ldir        
    jp mainloop  		 ; avoiding stack based calls
;-----------------------------------------------------------------------	

;-----------------------------------------------------------------------	
; TAKES A-reg as input, must be 0,1 or 2
RESTORE_INTERRUPT_MODE:   
	; Restore IM0, IM1 or IM2  
	ld d,a
	or	a
	jr	nz,not_im0
	ld a,$46  ;im	0  (ED 46)
	ld (SCREEN_START+(IM_LABLE-relocate)+1),a    
	jr	IMset
not_im0:	
	ld a,d
	dec	a
	jr	nz,not_im1
	ld a,$56  ;im	1 (ED 56)
	ld (SCREEN_START+(IM_LABLE-relocate)+1),a  
	jr	IMset
not_im1:	
	ld a,$5E  ; im	2 (ED 5E)
	ld (SCREEN_START+(IM_LABLE-relocate)+1),a  
IMset:
	jp IM_SETUP_COMPLETE
;------------------------------------------------------------------------

;------------------------------------------------------------------------
RestoreInterruptEnableState: 
	; Restore interrupt enable flip-flop (IFF) 
	READ_ACC_WITH_HALT		 ; read IFF
	AND	%00000100   		 ; get bit 2
	jr	z,skip_EI
	; If bit 2 is 1, modify instruction NOP to EI (opcode $FB)
	ld a,$FB
	ld (SCREEN_START+(NOP_LABLE-relocate)),a  ; EI Opcode = $FB
	jp RestoreInterruptComplete
skip_EI:
	LD A,0
	ld (SCREEN_START+(NOP_LABLE-relocate)),a  ; NOP
	jp RestoreInterruptComplete
;------------------------------------------------------------------------

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
;  SNA Header (27 bytes) - read via IN commands
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