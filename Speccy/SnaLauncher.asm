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
; Note: Setting the stack to the attributes area allows a visual debugging of the stack
WORKING_STACK_DEBUG			EQU SCREEN_ATTRIBUTES_END+1  ; DEBUG STACK

LOADING_COLOUR_MASK			EQU %00000001 ; flashing boarder while loading data

;******************************************************************************
;   MACRO SECTION
;******************************************************************************

;--------------------------------
; READ_ACC_WITH_HALT - Reads an 8-bit value from Arduino into the Accumulator (A)
;
; **Usage Example:**
;   READ_ACC_WITH_HALT  ; Read a byte into A
;
MACRO READ_ACC_WITH_HALT 
	halt
    in a, ($1f)  
ENDM

;--------------------------------
; READ_PAIR_WITH_HALT - Reads a 16-bit value sent from Arduino in a register pair
; **DO NOT USE BC** - This macro internally uses reg-C for the port value
;
; **Usage Example (showing workaround for reading 'BC') :** 
;   ld C, $1F                 ; Set up input port
;   READ_PAIR_WITH_HALT e, d  ; Read 16-bit value (temporarily in DE)
;   push de                   ; Save DE (holds BC)
;   pop bc                    ; Restore BC from DE
;
MACRO READ_PAIR_WITH_HALT  ,reg1,reg2  
    halt
    in reg1, (c)  ; Read lower byte
    halt
    in reg2, (c)  ; Read higher byte
ENDM

;--------------------------------
; SET_BORDER - Sets the border color
;	0 - Black, 1 - Blue, 2 - Red,  	 3 - Magenta, 
;	4 - Green, 5 - Cyan, 6 - Yellow, 7 - White
; **Usage Example:**
;   SET_BORDER 4   ; Set border color to Green
;
MACRO SET_BORDER ,colour 
    ld a, colour 
    AND %00000111	;  only the lower 3 bits are used (0-7)
    out ($FE), a  
ENDM
;--------------------------------

;******************
;*** CODE START ***
;******************
ORG $0000
L0000:  
	DI                     
	;-----------------------------------------------------------------------------------	
    ; Stack Behavior / Code Limitations
    ; - The stack grows downward:
    ;   - PUSH:  sp -= 2, stores value at (sp)
    ;   - POP:   loads value from (sp), sp += 2
    ; **IMPORTANT WARNINGS:**
    ; - Total stack usage must be **only 1 level deep** (2 bytes, just **one** push).
    ; - Do NOT use PUSH/POP around interrupts! (interrupts automatically push PC onto the stack)
	ld SP,WORKING_STACK
	;-----------------------------------------------------------------------------------	
	jp ClearScreen
	jp mainloop
 
;----------------------------------------------------------------------------------
; MASKABLE INTERRUPT - 0x0038
; Note: Leaves out 'EI' to ensure minimal delay, and is simple for us to EI outside
ORG $0038
L0038:   
	RET 	; 10 t-states
;----------------------------------------------------------------------------------

;----------------------------------------------------------------------------------
; NON-MASKABLE INTERRUPT (NMI) - 0x0066
; This code relies on the NMI for synchronization with the Arduino.
; The Z80 enters a blocking state (`HALT`), pausing execution until the Arduino triggers an NMI.
; When the Arduino sends a request on the NMI line, it releases the Z80 from HALT, in turn the Arduino 
; can read the state of the Z80 halt line.
ORG $0066
L0066:
	RETN	; 14 t-states
;----------------------------------------------------------------------------------

;******************
; *** MAIN LOOP ***
;******************
mainloop:
; For each incoming data transfer the header's first byte provides the request command ["C,"F","G","E"].
; NOTE: Most of the snapshot 16-bit data later on is big-endian. The sending Arduino pre-chewes this
; byte pair header information as little-endian.
check_initial:   	 ; command checking loop
	READ_ACC_WITH_HALT
	; These compares are in ordered in frequency of use
    cp 'C'           
    jr z, command_CP ; Transfer/copy (things like drawing Text, displying .scr files)
	cp 'F'            
    jr z, command_FL ; Fill (clearing screen areas, selector bar)   
    cp 'G' 
    jr z, command_GO ; Transfer/copy with flashing boarder (loading .sna files)
    cp 'E'            
    jr z, command_EX ; execute program (includes restoring 27 byte header) 
    jr check_initial 

;------------------------------------------------------
command_FL:  ; "F" - FILL COMMAND
;------------------------------------------------------
	ld C,$1F  				 ; setup for pair read
	READ_PAIR_WITH_HALT d,e  ; DE = Fill amount 2 bytes
	READ_PAIR_WITH_HALT h,l  ; HL = 'destination' address: 2 bytes
	halt		 	; Synchronizes with Arduino (NMI to continue)
	in a,($1f)   	; Read fill value from the z80 I/O port
	ld c,a
fillLoop:
	ld (hl),c	 	; Write to memory
    inc hl		
    DEC DE      
    LD A, D      
    OR E         
    Jr NZ, fillLoop   
    jr mainloop 	; done - back for next transfer command

;---------------------------------------------------
command_GO:  ; "G" - TRANSFER DATA (flashes border)
;---------------------------------------------------
	; Transfers are typically sequential, but any destination location can be targeted.
    ; The data transfer is limited to 255 bytes this INCLUDES THE HEADER.
	; Due to the synchronizing 'halt' we can't use 'INI'
	ld C,$1F  					; setup for pair read
	halt
	in b,(c)  					; The transfer size: 1 byte
	READ_PAIR_WITH_HALT h,l   	; HL = 'destination' address: 2 bytes
readDataLoop:
	halt		 		; Synchronizes with Arduino (NMI to continue)
	in a,($1f)   		; Read a byte from the z80 I/O port
	ld (hl),a	 		; write to memory
    inc hl		
	AND LOADING_COLOUR_MASK  
	out ($fe), a   		; Flash border - from actual data while loading
	djnz readDataLoop  	; read loop
    jp mainloop			; done - back for next transfer command

;------------------------------------------------------
command_CP:  ; "C" - COPY DATA
;------------------------------------------------------
	; Same as TRANSFER DATA but without the flashing boarder.
	ld C,$1F  				; setup for pair read
	halt
	in b,(c)  
	READ_PAIR_WITH_HALT h,l	; HL = 'destination' address: 2 bytes
CopyLoop:
	halt		 	; Synchronizes with Arduino (NMI to continue)
	in a,($1f)   	; Read a byte from the z80 I/O port
	ld (hl),a	 	; write to memory
    inc hl		
	djnz CopyLoop  	; read loop
    jp mainloop 	; done - back for next transfer command

;-------------------------------------------------------------------------------------------
command_EX:  ; "E" - EXECUTE CODE, RESTORE & LAUNCH 
; Restore snapshot states & execute stored jump point from the stack
;-------------------------------------------------------------------------------------------

	;-------------------------------------------------------------------------------------------
 	; Setup - put copy of ROM routine 'relocate' (final launch code) into Screen memory	
	; We are preparing screen memory now while registers are still available.
	LD HL,relocate
	LD DE,SCREEN_START 
	LD BC, relocateEnd - relocate
	LDIR  
	;-------------------------------------------------------------------------------------------
	
	;-------------------------------------------------------------------------------------------
 	; Restore 'I'
	READ_ACC_WITH_HALT
	ld	i,a
	;-------------------------------------------------------------------------------------------
	
	;-------------------------------------------------------------------------------------------
	; Restore HL',DE',BC',AF'
	ld c,$1f  				 ; setup for pair read
	READ_PAIR_WITH_HALT L,H  ; future HL'
	READ_PAIR_WITH_HALT e,d  ; future DE'
	READ_ACC_WITH_HALT		 ; C
	ld c,a
	READ_ACC_WITH_HALT		 ; B
	ld b,a					 ; future BC'
	exx						 ; Alternates registers restored
	; registers are free again, just AF' left to restore
	ld c,$1f  				 ; setup for pair read
	READ_PAIR_WITH_HALT e,d  ; read AF' - saving flags last
	push de					 ; store future AF'
	pop af					 ; Restore future AF'
	ex	af,af'				 ; Alternate AF' restored
	;-------------------------------------------------------------------------------------------
	
	;-------------------------------------------------------------------------------------------
	; Restore HL,DE,BC,IY,IX	
	ld c,$1f  				  ; setup for pair read
	READ_PAIR_WITH_HALT l,h   ; restore HL
	READ_PAIR_WITH_HALT e,d;  ; TO VOID - reads DE, we get this again later
	READ_PAIR_WITH_HALT e,d;  ; TO VOID - reads BC, we get this again later
	READ_PAIR_WITH_HALT e,d;  ; read IY
	push de
	pop IY					  ; restore IY
	READ_PAIR_WITH_HALT e,d;  ; read IX
	push de
	pop IX					  ; restore IX
	;------------------------------------------------------------------------

	;------------------------------------------------------------------------
	; read IFF
	READ_ACC_WITH_HALT	; reads the IFF flag states
	jp RestoreIFFState	; jump used - avoiding stack use
RestoreIFFStateComplete:
	;------------------------------------------------------------------------

	;------------------------------------------------------------------------
	; R regester
	READ_ACC_WITH_HALT 	; reads R
	ld a, $5F-$2c		; take away instructions to get R's true startup value - DANDARE-3 HACK FOR NOW
	ld r,a
	;------------------------------------------------------------------------

	;------------------------------------------------------------------------
	; 'AF' pair 
	READ_PAIR_WITH_HALT e,d		; TO VOID - we get AF again later
	;------------------------------------------------------------------------

	;------------------------------------------------------------------------
	
	; ************* TODO - REWORK THE REGISTERS IN ARDUINO CODE, UPDATE THIS FILE AND REPLACE DE WITH HL HERE

	; Restore Program's Stack Pointer
	READ_PAIR_WITH_HALT e,d;  ; get Stack
	ld (SCREEN_START+(spare-relocate)),de 	
	ld SP,(SCREEN_START+(spare-relocate))
	pop de	; results in SP+=2 - SP is now ready to be used as a tiny stack.
	; Store programs start address, uses self-modifying code into screen memory
	ld (SCREEN_START + (JumpInstruction - relocate) +1),de  ; set jump to address

    ;-----------------------------------------------
	; Final Stack Usage:
	; Instead of using RETN to start the .SNA program, we jump directly to the 
	; starting address. This frees up 2 bytes of stack space, allowing for a 
	; minimal but useful 1-deep stack. The HALT instruction requires an active 
	; stack for synchronization. The 2 freed bytes, originally holding the 
	; snapshot’s return address, now serve as a temporary single-level 
	; stack - just enough for our needs.
    ;-----------------------------------------------

	;------------------------------------------------------------------------
	READ_ACC_WITH_HALT  		; gets value of IM into reg-A
	JP RestoreInterruptMode 	; jump used - avoiding stack use
RestoreInterruptModeComplete:
	;------------------------------------------------------------------------

	;------------------------------------------------------------------------
	READ_ACC_WITH_HALT   ; Border Colour
	AND %00000111		 ; Only the lower 3 bits are used (0-7)
	out ($fe),a			 ; set border
	;------------------------------------------------------------------------

	; Arduino will now resend DE,BC & AF we IGNORED earlier.

	;------------------------------------------------------------------------
	; Restore DE & BC (we are now resource-poor)
 	READ_PAIR_WITH_HALT e, d   ; Restore DE directly 
	; Restore BC - To restore BC we can only use 'READ_ACC_WITH_HALT'
	READ_ACC_WITH_HALT	; C
	ld c,a
	READ_ACC_WITH_HALT	; B
	ld b,a
	;------------------------------------------------------------------------

	;-----------------------------------------
	; Spare is now available - Clear 2nd spare to reduce screen impact.
;	ld a,0
;	ld (SCREEN_START+((spare+1)-relocate)),a
	;-----------------------------------------

	;------------------------------------------------------------------------
	;
	; *************************************************
	; *** We are now extremely resource-constrained ***
	; *************************************************
	; Restore AF using only register A and SP.
	READ_ACC_WITH_HALT       ; Load original F (flags) into A
	push af                  ; Save F_original (A) and current flags (F) to stack
	inc sp                   ; Skip the current_flags byte (SP now points to F_original)
	pop af                   ; F = F_original (from stack), A = garbage (ignored)
	dec sp                   ; Restore SP to its original position
	READ_ACC_WITH_HALT       ; Load original A (accumulator) – AF is now fully restored

	;------------------------------------------------------------------------
	; Arduio just waits for the halt line - masable used this time.
	; Note: During HALT, the interrupt service routine will use the stack.
    ;!!! At this point, we are using the real restored program's stack space !!!
	IM 1
	EI

	; The idea behind using HALT here is to give a 20ms gap, we really don't
	; want the speccy firing it's maskalbe interrupt while we are restoring the 
	; final step. (our Maskable routine forgoes 'EI')
	HALT 	; Released by Maskable 50FPS Interupt routine ($0038)

	JP SCREEN_START		 ; jump to relocated code in screen memory
	;------------------------------------------------------------------------
; END - "command_EX:"

;-----------------------------------------------------------------------	
; 'relocate': SCREEN MEMORY USAGE - "76 XX xx XX C3 00 00 FF FF"  (template values will chage)
; This means 9 bytes of screen memory will be corrupted.
; 'relocate' section in ROK gets copied into screen memory to run so the ROM can be swapped out.
;-----------------------------------------------------------------------	
; FINAL STEP - Start-up the restored program 
; STACK has already been moved/freed of the snapshot's 'RETN' value.
relocate:
	; 1/ Arduino detects this HALT and signals the 'NMI' line to resume z80.
	; 2/ Arduino will waits 7 Microseconds (as NMI will cause a 'PUSH' & 'RETN')
	; 3/ Arduino switches from loader rom to original rom kept in the 2nd half.
	HALT ; 	[opcode:76] - After halt the original ROM should be active again!
IM_LABLE: 	
	IM 0    ; Interrupt Mode: self-modifying code - 'IM <n>'
NOP_LABLE:  
	NOP    	; Placeholder for self-modifying code - 'EI' or stays as 'NOP'
	; note: The loaded Snapshot file provides RETN/jump, but we have already
	; saved it's value and stored it just below.
JumpInstruction: 	; Self-modifying code - JP location will be modified 	
    jp 0000h        ; [opcode:C3] !!! THIS IS IT - WE ARE ABOUT TO JUMP TO THE RESTORE PROGRAM !!!
spare:				; a) Keeps real stacks starting pointer
	db $FF,$FF		; 			(^ Double purpose v)
TempStack:			; b) Used at startup until real stack is restored
relocateEnd:

; TO DO - optimise: remove db x2 bytes and just use the jp address bytes. ?!!!?!?
; Will need to aim stack at a JumpInstruction+1 as the code uses TempStack at startup 
; switching to the final stack before launch. Might be easy, removing "spare: db $ff,$ff" 
; as the stack is restored in the same place as the JumpInstruction address.

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

; Need to wait around for half of 69888 (t-states per screen update)
; This will wait for 32,768+74 t-states, being over with extra logic is fine
;pause:
;    push bc            
;    ld c, 15
;outer_loop:
;    ld b, 0   
;inner_loop:
;    nop           ; 4 t-states     
;    nop               
;    nop
;    nop
;    djnz inner_loop   
;    dec c
;    jr nz, outer_loop   
;    pop bc             
;	jp pauseReturn


org $3ff0
debug:   			; debug trap
	SET_BORDER a
	inc a
	jr debug

last:
DS  16384 - last


;******************************************************
; todo - future extra support for 2-byte "header"
; 1/ Shorten commands i.e. 'GO' to 'G' [DONE] 
; 2/ Add SCR loading/viewing "SC" [DONE]
; 3/ Add PSG Files - AY Player  "PS" 
; 4/ Add GIF Files - Gif Viewer/Player "GI"
; 5/ TO VOID - reorder incoming snapshot header  
; 6/ Options to change loading boarder (off, colour choice)
; 7/ Options to slow down loading, mimic tapes
; 8/ load binary TAP files (add command "T")
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