; NOTES:-
;            rom: $0000 - $3FFF (16 KB ROM)
;         screen: $4000 - $57FF (6144 bytes - screen display data)
;     attributes: $5800 - $5AFF (768 bytes - screen color attributes)
;
;******************************************************************************
;   CONSTANTS
;******************************************************************************
SCREEN_START				EQU $4000  ; [6144]
SCREEN_END					EQU $57FF
SCREEN_ATTRIBUTES_START   	EQU $5800  ; [768]
SCREEN_ATTRIBUTES_END   	EQU $5AFF  
WORKING_STACK				EQU  (SCREEN_START+(TempStack-relocate))
; Setting the stack to the attributes area allows a visual debugging of the stack
;WORKING_STACK				EQU SCREEN_ATTRIBUTES_END+1  ; DEBUG STACK

LOADING_COLOUR_MASK			EQU %00000001 ; flashing boarder while loading data

;******************************************************************************
;   MACRO SECTION
;******************************************************************************

;--------------------------------
MACRO READ_ACC_WITH_HALT 
	halt
    in a, ($1f)  
ENDM
;--------------------------------
; Note: When using READ_PAIR_WITH_HALT, avoid BC as it’s implicitly used in IN (reg), (C) commands.
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

    ; Stack grows down, for example:- PUSH="sp-=2, xx->(sp)" , POP="(sp)->xx, sp+=2"
	; Note: The attributes area for the stack can also be a helpful debugging tool when needed.
	;-----------------------------------------------------------------------	
	; TOTAL STACK USAGE MUST BE - 1 deep (2 bytes) - try avoiding anything that makes use of the stack.
	; Avoid PUSH, POP, and HALT during operations that are sensitive to stack depth.
	; To help reduce deep, don't:-  (i) Push,  (ii)NMI, (iii) pop (results in a 4 deep!)
	; Interrupts triggered by HALT use the stack, so push/pop need to planned around.

	ld SP,WORKING_STACK
	jp ClearScreen
mainloop: 

;******************************************************************************
; For each incoming data transfer, the header (first 2 bytes) are 'action' indicators.
;  # Transfer data = "GO"   (normally this is screen/program code all-in-one )
;  # Execute code = "EX" 	(after reading and restoring z80 registers)
;
check_initial:   		; Main loop for checking 'GO' or 'EX' (or 'SN' in future)
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
		; Leaving out 'EI' to ensure minimal delay, and is simple for us to EI outside
		RET
;******************************************************************************
; NON MASKABLE INTERRUPT - USED TO UN-HALT 
ORG $0066
L0066:
		RETN
;******************************************************************************

command_GO:  ; FIRST STAGE - TRANSFER DATA
	
	ld C,$1F  	; Initial setup for use with the IN command, i.e. "IN <REG>,(c)" 

	; Transfers are typically sequential, but any destination location can be targeted.
    ; The data transfer size is limited to around 250 bytes due to the inclusion of metadata like 'GO'.
    ; (Border color changes during transfers for useful visual feedback)

	halt
	in b,(c)  ; The transfer size is guaranteed to fit within a single byte.

	; Set HL with the 'destination' address (reading 2 bytes)
	READ_PAIR_WITH_HALT h,l  ; Arduino has formatted this address as little-endian, HOWEVER...
	; NOTE: Most of the snapshot 16bit data processed later on is big-endian, 
	;		so you will see things like "READ_PAIR_WITH_HALT l,h" (so l,h not h,l)

readDataLoop:
	halt		 ; As we need to halt each time around we can't use 'ini' we have to use 'in'
	in a,($1f)   ; Read a byte from the z80 I/O port
	ld (hl),a	 ; write to memory (technically onward from $4000 on a 48k speccy)
    inc hl		

	AND LOADING_COLOUR_MASK  
	out ($fe), a   ; Flash border - fed from actual data while loading

	djnz readDataLoop  ; read loop

    jr mainloop ; done - go back and wait for the next transfer action

command_EX:  ; SECOND STAGE - Restore snapshot states & execute stored jump point from the stack
	
	;-------------------------------------------------------------------------------------------
 	; Setup - put copy of ROM routine 'relocate' (final launch code) into Screen memory	
	LD HL,relocate
	LD DE,SCREEN_START 
	LD BC, relocateEnd - relocate
	LDIR  ; We are preparing this screen memory now while registers are still available.
	;-------------------------------------------------------------------------------------------
	
	;-------------------------------------------------------------------------------------------
 	; Restore 'I'
	READ_ACC_WITH_HALT
	ld	i,a
	;-------------------------------------------------------------------------------------------
	
	;-------------------------------------------------------------------------------------------
	; Restore HL',DE',BC',AF'
	ld c,$1f  
	READ_PAIR_WITH_HALT L,H  ; store HL'
	READ_PAIR_WITH_HALT e,d  ; store DE'
	READ_PAIR_WITH_HALT e,d  ; Get BC' (temporarily in DE)
    push de                  ; Save DE (holds BC')
    pop bc                   ; BC' restored
	exx						 ; Alternates registers restored
	; registers are free again, just AF' left to restore
	ld c,$1f  
	READ_PAIR_WITH_HALT e,d  ; spare to read AF' - save flags last
	push de					 ; store AF'
	pop af					 ; Restore AF'
	ex	af,af'				 ; Alternate AF' restored
	;-------------------------------------------------------------------------------------------
	
	;-------------------------------------------------------------------------------------------
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
	;-------------------------------------------------------------------------------------------

	READ_ACC_WITH_HALT		 ; read IFF
	jp RestoreIFFState
RestoreIFFStateComplete:

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
	JP RestoreInterruptMode
RestoreInterruptModeComplete:
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
	; Restore AF creatively using only register A.
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
; SCREEN MEMORY USAGE - "76 ED 46 00 33 33 C3 00 00 7F 7F 7F"
; This means 12 bytes of screen memory will be destored

; This gets placed in screen memory (called via JP SCREEN_START)
;-----------------------------------------------------------------------	
; FINAL STEP - Start-up the restored program 
relocate:
  	HALT  ; Here we just wait for a maskable interrupt.
		  ; Arduino switches to 'Upper ROM' upon detecting this halt.
          ; Note: During HALT, the interrupt service routine will use the stack.
          ; At this point, we are using the real restored program's stack space.
IM_LABLE:
	; "IM_LABLE" here self-modifying code is used here to dynamically adjust the 
	; interrupt mode and jump location based on runtime data.
	IM 0				; Self-modifying code - 'IM <n>'
NOP_LABLE:
	NOP 				; Self-modifying code - To 'EI' if flagged from snapshot
	
	inc sp  ; Snapshot file provides RETN/jump location here, but we have used/destroyed this location
	inc sp  ; as a temporary 1-deep stack. We previously saved its value and stored it just below.

JumpInstruction: 	; Self-modifying code - JP location will be modified 	
    jp 0000h        ; !!! THIS IS IT - WE ARE ABOUT TO JUMP TO THE RESTORE PROGRAM !!!
spare:				; Used to keep real stacks starting pointer
	db $7f,$7f		; Double perpose, see top & bottom comments (both labels reflect usage intent)
TempStack:			; Used at startup until real stack is restored
TempVar:
	db $7f
relocateEnd:
;-----------------------------------------------------------------------	

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
; IN: Acc must be 0,1 or 2
RestoreInterruptMode:   
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
	jp RestoreInterruptModeComplete
;------------------------------------------------------------------------

;------------------------------------------------------------------------
; IN: Acc this holds byte-19 of the SNA header
RestoreIFFState:  
	; Restore interrupt enable flip-flop (IFF) 
	AND	%00000100   		 ; get bit 2
	jr	z,skip_EI
	; If bit 2 is 1, modify instruction NOP to EI (opcode $FB)
	ld a,$FB
	ld (SCREEN_START+(NOP_LABLE-relocate)),a  ; EI Opcode = $FB
	jp RestoreIFFStateComplete
skip_EI:
	LD A,0
	ld (SCREEN_START+(NOP_LABLE-relocate)),a  ; NOP
	jp RestoreIFFStateComplete
;------------------------------------------------------------------------

org $3ff0
debug:   			; debug trap
	SET_BORDER a
	inc a
	jr debug


last:
DS  16384 - last


;******************************************************
; todo - future extra support for 2-byte "header"
; 1/ renaming 'GO' to 'GD' (Get Data).
; 2/ Add SCR loading/viewing "SC"
; 3/ Add PSG Files - AY Player  "PS" 
; 4/ Add GIF Files - Gif Viewer/Player "GI"
; 5/ TO VOID - reorder incoming snapshot header
; 6/ Options to change loading boarder (off, colour choice)
; 7/ Options to slow down loading, mimic tapes
; 8/ load binary TAP files "TP"
; 9/ load TZX files
; 10/ Load Z80 Files
; 11/ Load PNG file
; 12/ Support poke cheats
; 13/ Analyse games at load time, add lives + cheats
; 14/ Analyse music players extract, enable for AY on 48k.
; 15/ Compress data/unpack - result must be faster (so maybe not)
; 16/ Very simple run-length encoding - could pay off - find most frequent/clear all mem to that/skip those on loading ? 
; 17/ View game screens/scroll and pick one by picutre view (load just sna screen part)

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