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
MACRO READ_PAIR_WITH_HALT ,reg_low,reg_high  
    halt
    in reg_low, (c)  ; Read lower byte
    halt
    in reg_high, (c)  ; Read higher byte
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
    AND %00000111	;  only lower 3 bits are used
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
	ld c,$1F  				 ; setup for pair reads
	jp mainloop
 
;----------------------------------------------------------------------------------
; MASKABLE INTERRUPT (IM 1) - Vector: 0x0038  
; Note: 'EI' is intentionally omitted - we don't use IM1 in the usual way.  
ORG $0038
L0038:   
	RET 	; 10 t-states
;----------------------------------------------------------------------------------

;----------------------------------------------------------------------------------
; NON-MASKABLE INTERRUPT (NMI) - Vector: 0x0066  
; The HALT instruction and this NMI are used for synchronization with the Arduino.  
; The Arduino monitors the HALT line and triggers the NMI to let the Z80 exit the HALT state. 
; After NMI line signals for the speccy to resume, the Arduino waits for 7μs to allow time for the ISR to complete.
; NMI Z80 timings -  Push PC onto Stack: ~10 or 11 T-states
;                    RETN Instruction:    14 T-states.
; 					 24×0.2857=6.857μs (1/3.5MHz = 0.2857μs)
ORG $0066
L0066:
	RETN
;----------------------------------------------------------------------------------

;******************
; *** MAIN LOOP ***
;******************
mainloop:
; For each incoming data transfer the header's first byte provides the request command ["C,"F","G","E","W"].
; NOTE: Most of the snapshot 16-bit data later on is big-endian. The sending Arduino pre-chewes this
; byte pair 'command header' information as little-endian.
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
    cp 'W'            
    jr z, command_WT ; Wait for 50Hz maskable interrupt
    jr check_initial 

;------------------------------------------------------
command_FL:  ; "F" - FILL COMMAND
;------------------------------------------------------
;;;	ld C,$1F  				 ; setup for pair read
	READ_PAIR_WITH_HALT d,e  ; DE = Fill amount 2 bytes
	READ_PAIR_WITH_HALT h,l  ; HL = Destination address
	halt		 	; Synchronizes with Arduino (NMI to continue)
;;	in a,($1f)   	; Read fill value from the z80 I/O port
;;	ld c,a
	in b,(c)
fillLoop:
;;	ld (hl),c	 	; Write to memory
	ld (hl),b	 	; Write to memory
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
;;;	ld C,$1F  				; setup for pair read
	halt					; Synchronizes with Arduino (NMI to continue)
	in b,(c)  				; The transfer size: 1 byte
	READ_PAIR_WITH_HALT h,l ; HL = Destination address
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
;;;	ld C,$1F  				; setup for pair read
	halt					; Synchronizes with Arduino (NMI to continue)
	in b,(c)  				; The transfer size: 1 byte
	READ_PAIR_WITH_HALT h,l	; HL = Destination address
CopyLoop:
	halt		 	; Synchronizes with Arduino (NMI to continue)
	in a,($1f)   	; Read a byte from the z80 I/O port
	ld (hl),a	 	; write to memory
    inc hl		
	djnz CopyLoop  	; read loop
    jp mainloop 	; done - back for next transfer command

;--------------------------------------------------------
command_WT:  ; "W" - Wait for the 50Hz maskable interrupt
;--------------------------------------------------------
    ; Enable Interrupt Mode 1 (IM 1) to use the default Spectrum interrupt handler at $0038.  
    ; This will trigger an interrupt at 50Hz (every 20ms). 
	; Once an interrupt occurs, execution resumes at address $0038 (default ISR).
    ; Our ISR does not re-enable interrupts, so no further interrupts will occur after this.
    IM 1    ; MASKABLE INTERRUPT (IM 1) - Vector: 0x0038  
    EI 
    ; We use HALT to pause execution until the next interrupt occurs. Giving us a 20ms delay 
	; before continuing. The goal is to prevent an interrupt from interfering while
	; restoring the final state.  The Arduino monitors this HALT as special case and will wait 
	; for the z80 to resume.  The Arduino give the "W" command and so knows when this code resumes.
    HALT 			; enables z80's halt line
    jp mainloop 	; done - back for next transfer command
;-----------------------------------------------------

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
	; -- Restore Alternate Registers HL',DE',BC',AF --'
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
	; Restore IY,IX	
	ld c,$1f  				  ; setup for pair read
	READ_PAIR_WITH_HALT e,d;  ; read IY
	push de
	pop IY					  ; restore IY
	READ_PAIR_WITH_HALT e,d;  ; read IX
	push de
	pop IX					  ; restore IX
	;------------------------------------------------------------------------

	;------------------------------------------------------------------------
	; Read IFF
	READ_ACC_WITH_HALT		; reads the IFF flag states
	jp RestoreEI_IFFState	; jump used - avoiding stack use
RestoreEI_IFFStateComplete:
	;------------------------------------------------------------------------

	;------------------------------------------------------------------------
	; Read R (refresh) register
	READ_ACC_WITH_HALT      ; read R
	ld r, a                 ; Sets R, but this won't match the original start value,
	                        ; since we're restoring the state now, not at true game start.
	                        ; Note: this could cause issues if a game uses R as part of a protection check.
	                        ; So far, no known games are confirmed to use R this way???
	;------------------------------------------------------------------------

	;-----------------------------------------------
	; Final Stack Usage:
	; Instead of using RETN to start the .SNA program, we jump directly to the starting address. 
	; This frees up 2 bytes of stack space, allowing for a minimal but useful 1-deep stack. 
	; The HALT instruction requires an active stack for synchronization. The 2 freed bytes, 
	; originally holding the snapshot's return address, now serve as a temporary single-level 
	; stack—just enough for our needs in the final "resource-critical" restore stages.
	;
	; Restore Program's Stack Pointer - the SNA format allows some magic here
	READ_PAIR_WITH_HALT l, h  ; Read stack
	ld sp, hl                 ; 'Snapshots_SP' restored (the normal 'RET' location)
	pop hl                    ; SP += 2, move past 'RET' location - ready as a tiny stack!
	ld (SCREEN_START + (JumpInstruction - relocate) + 1), hl  ; patching "JP xxxx" starup-up address
	;-----------------------------------------------

	;------------------------------------------------------------------------
	READ_PAIR_WITH_HALT l,h  	; restore HL
	;------------------------------------------------------------------------

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

	;------------------------------------------------------------------------
	; Restore DE & BC (we are now resource-poor)
 	READ_PAIR_WITH_HALT e, d   ; Restore DE directly 
	; Restore BC - To restore BC we can only use 'READ_ACC_WITH_HALT'
	READ_ACC_WITH_HALT	; C
	ld c,a
	READ_ACC_WITH_HALT	; B
	ld b,a
	;------------------------------------------------------------------------

	; *************************************************
	; *** We are now extremely resource-constrained ***
	; *************************************************

	;------------------------------------------------------------------------
	; Restore AF using only register A and SP.
	READ_ACC_WITH_HALT       ; Load original F (flags) into A
	push af                  ; Save F_original (A) and current flags (F) to stack
	inc sp                   ; Skip the current_flags byte (SP now points to F_original)
	pop af                   ; F = F_original (from stack), A = garbage (ignored)
	dec sp                   ; Restore SP to its original position
	READ_ACC_WITH_HALT       ; Load original A (accumulator) – AF is now fully restored

	;------------------------------------------------------------------------
	JP SCREEN_START		 ; jump to relocated code in screen memory
	;------------------------------------------------------------------------
; END - "command_EX:"

;-----------------------------------------------------------------------    
; 'relocate' - The interface ROM has to give way to the original ROM for most games to work.
; For this to happen, a small 4-byte section of screen memory must be overwritten.
; At the "Execute Code Stage," this 'relocate' section is copied into screen memory and executed.
; Since we can't execute code during a ROM swap, it must run from screen memory (only memory we can spoil)
; allowing the second half of the ROM (a copy of the original Speccy 48K ROM) to be swapped in.
; (I found switching back to the internal ROM just gave me issues :(, so I used the second
;  half of the ROM which worked perfectly.) 
;-----------------------------------------------------------------------    
; FINAL STEP - Start the restored program. 
relocate:
    ; 1/ The Arduino detects this HALT and triggers the 'NMI' line to resume the Z80.
    ; 2/ The Arduino waits 7 microseconds (since NMI will cause a 'PUSH' & 'RETN').
    ; 3/ The Arduino switches from the loader ROM to the original ROM 
    ;    (same external ROM, but using its second half).
    HALT       		; [opcode: 76] - After HALT, the original ROM should be active again!

JumpInstruction:     
    jp 0000h 		; [opcode: C3] !!! THIS IS IT - WE ARE ABOUT TO JUMP TO THE RESTORED PROGRAM !!!

TempStack:        	; Address of above "jp" acts as temporary stack until the SNAPSHOT_SP is restored.
relocateEnd: 
;-----------------------------------------------------------------------	

;-----------------------------------------------------------------------
; Not speed critical (used at startup)	
; NOT optimising away 17 t-states, sepuration for future colour changes.
ClearScreen:
	SET_BORDER 0
   	ld a, 0              
    ld hl, 16384       
    ld de, 16385        
    ld bc, 6144       
    ld (hl), a    	; Screen bitmap locations
    ldir                
	; continue HL into attributes 
    ld bc, 768-1
    ld (hl), a  	; Screen attribute locations
    ldir        
    jp mainloop  	; avoiding stack based calls
;-----------------------------------------------------------------------	

;-----------------------------------------------------------------------	
; RestoreInterruptMode
;	IN: A, IM0=0, IM1=1, IM2=2
; 	Sets: IM0, IM1 or IM2  
; 	Destroys D register
;	NOTE: JP back not RET as subroutine calls due to the one‐level stack constraint
RestoreInterruptMode:
    cp 0
    jr z, setIM0
    cp 1
    jr z, setIM1
setIM2:
    IM 2
    jp RestoreInterruptModeComplete  ; used only to avoid stack use
setIM1:
    IM 1
    jp RestoreInterruptModeComplete  ; used only to avoid stack use
setIM0:
    IM 0
    jp RestoreInterruptModeComplete  ; used only to avoid stack use
;------------------------------------------------------------------------

;------------------------------------------------------------------------
; RestoreEI_IFFState
; 	Sets: 'EI' - Enable Interrupt, flip-flop (IFF) 
; 	IN: A, enable = bit 2 set
;	NOTE: JP back not RET as subroutine calls due to the one‐level stack constraint
RestoreEI_IFFState:
	AND	%00000100   		 			; get bit 2
	jp	z,RestoreEI_IFFStateComplete  	; disabled - skip 'EI'
	EI
	jp RestoreEI_IFFStateComplete		 ; avoiding stack based calls
;------------------------------------------------------------------------

;------------------------------------------------------------------------
org $3ff0	  		; Locate near the end of ROM
debug_trap:  		; If we see border lines, we probably hit this trap.
	SET_BORDER a
	inc a
	jr debug_trap
;------------------------------------------------------------------------

;------------------------------------------------------------------------
last:
DS  16384 - last	; leave rest of rom blank
;------------------------------------------------------------------------


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

;*****************************
; 27 bytes SNA Header Example
; [0 ] I            = 0x3F       
; [1 ] HL_          = 0x2758     
; [3 ] DE_          = 0xB462     
; [5 ] BC_          = 0x3F62     
; [7 ] AF_          = 0x12A8     
; [9 ] HL           = 0xFC0B   // READ LAST   
; [11] DE           = 0x98B2   // READ 3rd LAST  
; [13] BC           = 0x0012   // READ 2nd LAST  
; [15] IY           = 0x5C3A     
; [17] IX           = 0xDB59     
; [19] IFF2         = 0x00       
; [20] R            = 0x5F       
; [21] AF           = 0x4040     
; [23] SP           = 0x6188     
; [25] IM           = 0x01       
; [26] BorderColour = 0x00       

; This order is used Arduino end - it's better suited for restoring Z80 states.
;  sendBytes(head27_1, (1/*"E"*/) + 1+2+2+2+2 );  // Send command "E" then I,HL',DE',BC',AF'
;  sendBytes(&head27_1[1 + 15], 2+2+1+1 );   // Send IY,IX,IFF2,R (packet data continued)
;  sendBytes(&head27_1[1 + 23], 2);          // Send SP                     "
;  sendBytes(&head27_1[1 +  9], 2);          // Send HL                     "
;  sendBytes(&head27_1[1 + 25], 1);          // Send IM                     "
;  sendBytes(&head27_1[1 + 26], 1);          // Send BorderColour           "
;  sendBytes(&head27_1[1 + 11], 2);          // Send DE                     "
;  sendBytes(&head27_1[1 + 13], 2);          // Send BC                     "
;  sendBytes(&head27_1[1 + 21], 2);          // Send AF                     "
