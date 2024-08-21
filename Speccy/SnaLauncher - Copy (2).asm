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

WORKING_STACK				EQU SCREEN_START+5
;;;;TEMP_STORAGE			EQU SCREEN_START

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
    
	AND %00000111  ; set border 0 to 7
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

	; Restore interrupt enable flip-flop (IFF) 
	LD A,0
	ld (SCREEN_START+(JumpInstruction-relocate)),A
	READ_ACC_WITH_HALT		 ; read IFF
	AND	%00000100   		 ; get bit 2
	jr	z,skip_EI
	; If bit 2 is 1, modify instruction NOP to EI (opcode $FB)
	ld a,$FB
	ld (SCREEN_START+(JumpInstruction-relocate)),a  ; EI Opcode = $FB
skip_EI:

	; R REG ... TODO  
	READ_ACC_WITH_HALT ; currently goes to the void

	READ_PAIR_WITH_HALT e,d;  ; TO VOID - using spare to restore AF

	; Store Stack Pointer
	READ_PAIR_WITH_HALT e,d;  ; get Stack
	ld (SCREEN_START+(Space-relocate)),de 	  ; store sack

	;------------------------------------------------------------------------
	; Restore IM0, IM1 or IM2  
	READ_ACC_WITH_HALT		  ; read IM (interrupt mode)
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
	;------------------------------------------------------------------------


	READ_ACC_WITH_HALT   ; Border Colour
	AND %00000111		 
	out ($fe),a			 ; set border

	;------------------------------------------------------------------------
	; Store programs start address, uses self-modifying code into screen memory
	ld SP,(SCREEN_START+(Space-relocate))	
	pop de
	ld (SCREEN_START + (JumpInstruction - relocate) +1),de
	;------------------------------------------------------------------------

	;------------------------------------------------------------------------
	; Restore DE & BC
	ld SP,WORKING_STACK        ; nothing left on temp stack, can set without harming anything
    READ_PAIR_WITH_HALT e, d   ; Restore DE directly
    READ_PAIR_WITH_HALT e, d   ; Get BC (temporarily in DE)
    push de                    ; Save DE (holds BC)
    pop bc                     ; BC restored
	;------------------------------------------------------------------------

	;------------------------------------------------------------------------
	; Restore AF - This process involves additional steps as we a running out 
	;              of free registers, and need to minimize screen memory use. 
	; !!!Abusing stack's memory - WE NOW HAVE NO FREE REG PAIRS - A REG ONLY!!!!
	READ_ACC_WITH_HALT     		; 'F' is now in A
	ld (WORKING_STACK),a      	; Store F - (STACK START LOCATION SO SAFE FROM PUSH/POP)
	READ_ACC_WITH_HALT     		; 'A' is now in A (will cause PUSH/POP)
	ld (WORKING_STACK+1),a    	; Store A - FAKING THE STACK, SO WE CAN POP WITH NO PUSH
	pop af                 		; restore AF
	; NOTE: 'READ_PAIR_WITH_HALT' will alter the flags (uses 'IN'), so we restore AF last.
	;------------------------------------------------------------------------

	;------------------------------------------------------------------------
	; Restore real programs stack
	ld SP,(SCREEN_START+(Space-relocate))		 ; Restore the program's stack pointer
;	inc sp 						 ; 
;	inc sp						 ; (PC+2 as we already have it in the 'JumpInstruction')
	;------------------------------------------------------------------------

	;------------------------------------------------------------------------
	ld (SCREEN_END),A		; store A 
	;------------------------------------------------------------------------
  	; Restore opcodes in screen memory:- 
	;  	1/'HALT', 2/'IM <n>', 3/'EI' or 'NOP', 4/'inc sp', 5/'inc sp x2' 
	; Self-modifying code follows:-

	ld a,$76				; set 'HALT'
	ld (SCREEN_START),a
	;
	;IM_LABLE:
	;IM 0

	ld a, (SCREEN_START+(JumpInstruction-relocate))	; this location holds EI opcode or NOP
	ld (SCREEN_START+(EI_NOP_LABLE-relocate)),a   ; set $FB or NOP depending on sna we read earlier
	
	ld a,$C3				; set 'JP'  - we used this for IM storage above!!!
	ld (SCREEN_START+(JumpInstruction-relocate)),a   ; 
	; Note: jp $0000, placeholder for programs start address is already set
	
	ld a,$33				; set 'inc sp' opcode
	ld (SCREEN_START+(INC_SP-relocate)+0),a   ;  'inc sp'
	ld (SCREEN_START+(INC_SP-relocate)+1),a   ;  'inc sp'
	
	;------------------------------------------------------------------------
	ld A,(SCREEN_END)	 ; Restore A
	;------------------------------------------------------------------------

	;------------------------------------------------------------------------
	; Arduio just waits for the halt line - masable used this time
	IM 1
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
          ; At this point, we are using the real restored program's stack space,
          ; so technically the real state might have a full stack. If this is the case, bad things may happen.
		  ; (I sould really do the 'inc sp' x2 after the halt, but the would eat more into screen memory,
		  ;  maybe add a safe-mode that eats more screen memory)
IM_LABLE:
	IM 0
EI_NOP_LABLE:
	NOP 				; Self-modifying code - To 'EI' if flagged from snapshot
INC_SP:
	inc sp 			    ; $33
	inc sp	

TempStack:
JumpInstruction: 		; 
    jp 0000h            ; Self-modifying code - JP location will be modified 
Space:
	NOP
	NOP
relocateEnd:
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
    jp mainloop  		 ; don't use call/ret ... !!!NEED TO AVOID USING STACK!!!

;-----------------------------------------------------------------------	

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