; NOTES:-
;            rom: $0000 - $3FFF (16 KB ROM)
;         screen: $4000 - $57FF (6144 bytes - screen display data)
;     attributes: $5800 - $5AFF (768 bytes - screen color attributes)
;
;******************************************************************************
;   CONSTANTS
;******************************************************************************
SCREEN_START				EQU $4000  ; [6144 bytes]
SCREEN_END					EQU $57FF
SCREEN_ATTRIBUTES_START		EQU $5800  ; [768 bytes]
SCREEN_ATTRIBUTES_END		EQU $5AFF  

; Final stack for restoring sna files is place at location 0x4004 
; see label 'relocate' for more info.
WORKING_STACK				EQU  (SCREEN_START+(TempStack-relocate))
; Note: Setting the stack to the attributes area allows a visual debugging of the stack
;WORKING_STACK_DEBUG			EQU SCREEN_ATTRIBUTES_END+1  ; DEBUG STACK
LOADING_COLOUR_MASK			EQU %00000001 ; flashing boarder while loading data

;******************************************************************************
;   MACRO SECTION
;******************************************************************************

;--------------------------------
; READ_ACC_WITH_HALT - Reads an 8-bit value from Arduino into the Accumulator (A)
;
; Usage Example:
;   READ_ACC_WITH_HALT  ; Read a byte into A
;
; Note: We use halting like this to give us a synchronization point with the Arduino.
; Once HALT is reached, the Arduino takes over via NMI. 
MACRO READ_ACC_WITH_HALT 
	halt  				;   HALT pauses Z80 until NMI (from Arduino) resumes execution.
    in a, ($1f)  
ENDM

;--------------------------------

; READ_PAIR_WITH_HALT - Reads a 16-bit value sent from Arduino into a register pair
; **DO NOT USE BC AS DESTINATION** - Macro uses C for I/O port ($1F)
;   - If you need BC, use workaround:
;       ld C, $1F				  ; Setup needed for this macro
;       READ_PAIR_WITH_HALT e, d  ; Read into DE first
;       push de                   ; 
;       pop bc                    ; Now in BC
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
	;Stack Behavior - 	The stack grows downward.
	;					PUSH: sp -= 2; stores value at (sp).
	;					POP: loads value from (sp); sp += 2.
	;*** IMPORTANT WARNINGS ***:
	; Code limitations when using "command_Execute":
	; Total stack usage must be only 1 level deep (2 bytes, just one push).
	; Do NOT use PUSH/POP around interrupts! (Interrupts automatically push PC onto the stack).
	; (While in the menu code, it's okay to use the stack normally.)

	; 0xffff for menu usem, this SP will be reasigned later with a "command_Stack"
 	ld SP,0xFFFF  

;;	HALT ; syncronise with Arduino

	;-----------------------------------------------------------------------------------	
	jp ClearScreen
	ld c,$1F  	; setup for 'READ_PAIR_WITH_HALT'

;;;;;	call CHECK_STARTUP_STRING

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
	cp 'Z'
	jp z, command_Copy32 ; Copy 32+(4 header), 256pixels (32bytes)
	cp 'F'            
    jp z, command_Fill ; (clearing screen areas, selector bar)   
    cp 'G' 
    jp z, command_Transfer ; Transfer with flashing boarder (loading .sna files)
    cp 'C'           
    jp z, command_Copy ; Copy, use for Text (error messages)
    cp 'W'            
    jp z, command_Wait ; Wait for 50Hz maskable interrupt
	cp 'S'
	jp z, command_Stack ; Use screen memory for stack (for sna loading)
    cp 'E'            
    jp z, command_Execute ; execute program (includes restoring 27 byte header) 
    cp 'T'            
    jp z, command_Transmit ; send data to Arduino USING A pulse-count protocol

    jr check_initial 


;------------------------------------------------------
; --- Transmit key press using pulse-count protocol ---
; The Arduino watches the Z80's HALT line.
; We issue N HALT instructions, where N=key index value (1–40).
; Arduino counts the HALT pulses to determine which key was pressed.
;
command_Transmit:  ; "T" - TRANSMIT KEY PRESS
;------------------------------------------------------
	READ_ACC_WITH_HALT			 
	ld b,a
	push bc

 	; Get key pulses to send; 1:no key (2–41 keys)
	JP GET_KEY_PULSES           
DONE_KEY:
	LD B, A                 
	; send pulses  
TX: HALT                    
	DJNZ TX                  

	; --- End marker delay ---
	; Add a short delay to signal the end of pulse group.
	; Note: Next HALT in the main loop will happen *after* this delay.
	pop bc     
DELAY_LOOP:
	NOP                    
	DJNZ DELAY_LOOP		; ball park T-states: N * (4 + 13)
	ld c,$1F  			; re-setup for 'READ_PAIR_WITH_HALT'
	JR mainloop         ; Return to main loop

;------------------------------------------------------
command_Fill:  ; "F" - FILL
;------------------------------------------------------
	READ_PAIR_WITH_HALT d,e  ; DE = Fill amount 2 bytes
	READ_PAIR_WITH_HALT h,l  ; HL = Destination address
	halt		 	; Synchronizes with Arduino (NMI to continue)
	in b,(c)		; Read fill value from the z80 I/O port
fillLoop:
	ld (hl),b	 	; Write to memory   [7]
    inc hl			;					[6]
    DEC DE      	;					[6]
    LD A, D      	;					[4]
    OR E         	;					[4]
    JP NZ, fillLoop ; JP, 2 cycles saved per iter (jr=12,jp=10 cycles when taken)  [10]
    JR mainloop 	; Return back for next transfer command

;---------------------------------------------------
command_Transfer:  ; "G" - TRANSFER DATA (flashes border)
;---------------------------------------------------
	; Transfers are typically sequential, but any destination location can be targeted.
    ; The data transfer is limited to 255 bytes this INCLUDES THE HEADER.
	; Due to the synchronizing 'halt' we can't use 'INI'
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
command_Copy:  ; "C" - COPY DATA
;------------------------------------------------------
	; Same as TRANSFER DATA but without the flashing boarder.
	halt					; Synchronizes with Arduino (NMI to continue)
	in b,(c)  				; The transfer size: 1 byte (size must be >=1)
	READ_PAIR_WITH_HALT h,l	; HL = Destination address
CopyLoop:
	halt		 	; Synchronizes with Arduino (NMI to continue)

;	in a,($1f)   	; Read a byte from the z80 I/O port [11]
;	ld (hl),a	 	; write to memory					[7]
;   inc hl			;									[6]	
;	djnz CopyLoop  	; read loop							[13]
; TOTAL: [37 t-states]
	
	ini   			; (HL)<-(C), B<-B–1, HL<-HL+1		[16]
	JP nz,CopyLoop	;									[10]

    jp mainloop 	; done - back for next transfer command

;------------------------------------------------------
command_Copy32:  ; 'Z' - Copy Block of Data (32 bytes)
;------------------------------------------------------
	halt					; Synchronizes with Arduino (NMI to continue)
	in b,(c)  				; The transfer size: 1 byte (size must be >=1)
	READ_PAIR_WITH_HALT h,l	; HL = Destination address

	; (64+192+56)*224 = 69888 T-states (One scanline takes 224 T-states)
	; Total per frame: 312 PAL lines*224 = 69888 T-states (48k Speccy) 
	; Vertical Blank: 14336+11637 = 25972 T-states (Uncontended)

; https://github.com/rejunity/zx-racing-the-beam/blob/main/screen_timing.asm

	REPT 32					; transfer x32 bytes [total:640 t-states]
	halt					; [4]
	ini						; [16]
	ENDM

    JP mainloop 			; done - back for next transfer command

;--------------------------------------------------------
command_Wait:  ; "W" - Wait for the 50Hz maskable interrupt
;--------------------------------------------------------
    ; Enable Interrupt Mode 1 (IM 1) to use the default Spectrum interrupt handler at $0038.  
    ; This will trigger an interrupt at 50Hz (every 20ms). 
	; Once an interrupt occurs, execution resumes at address $0038 (default ISR).
    ; Our ISR does not re-enable interrupts, so no further interrupts will occur after this.
    IM 1    ; MASKABLE INTERRUPT (IM 1) - Vector: 0x0038  
    EI 
    ; We use HALT here to pause execution until the next interrupt occurs. Giving us a 20ms delay 
	; before continuing. The goal is to prevent an interrupt from interfering while
	; restoring the final state.  Since the Arduino gave this "W" command it monitors halt and 
	; blocks until the Z80 resumes.
    HALT 			; enables the Z80 halt line
    jp mainloop 	; done - back for next transfer command

;-----------------------------------------------------
command_Stack:  ; 'S'  - restores snapshots stack 
;-----------------------------------------------------
	READ_PAIR_WITH_HALT h,l 
	ld sp,hl
    halt  					; synchronization with Arduino
    jp   mainloop          	; Done – jump back for next command

;-------------------------------------------------------------------------------------------
command_Execute:  ; "E" - EXECUTE CODE, RESTORE & LAUNCH 
; Restore snapshot states & execute stored jump point from the stack
;-------------------------------------------------------------------------------------------

	;-------------------------------------------------------------------------------------------
	; Setup - Copy code to screen memory, as we'll be switching ROMs later.
	; After the ROM swap, we lose this sna ROM code in exchange for the stock ROM,
	; so the launch code will run from screen RAM.
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
	ld c,$1f  				 ; Just use C above for LDIR copy, setup again for pair read 
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
	ld c,$1f  				  ; setup again for pair read 
	READ_PAIR_WITH_HALT e,d;  ; read IY
	push de
	pop IY					  ; restore IY
	READ_PAIR_WITH_HALT e,d;  ; read IX
	push de
	pop IX					  ; restore IX
	;------------------------------------------------------------------------

	;------------------------------------------------------------------------
	; Restore IFF
	READ_ACC_WITH_HALT		; reads the IFF flag states
	jp RestoreEI_IFFState	; jump used - avoiding stack use in general
RestoreEI_IFFStateComplete:
	;------------------------------------------------------------------------

	;------------------------------------------------------------------------
	; Restore R (refresh) register
	READ_ACC_WITH_HALT      ; read R
	ld r, a                 ; Sets R, but this won't match the original start value,
	                        ; since we're restoring the state now, not at true game start.
	                        ; Note: this could cause issues if a game uses R as part of a protection check.
	                        ; So far, no known games are confirmed to use R this way???
	;------------------------------------------------------------------------

	;-----------------------------------------------
	; Final Stack Usage:
	; Instead of using RETN to start the .SNA program, we jump directly to the starting address. 
	; This frees up 2 bytes of stack space that would normally be used to hold the return address 
	; required by the RETN instruction.
	;
	; This gives us a minimal but useful 1-deep stack.
	; The HALT instruction requires an active stack for synchronization. The 2 freed bytes, 
	; which originally held the snapshot's return address, now serve as a temporary single-level 
	; stack — just enough for the final "resource-critical" restore stages.
	;
	; Restore Program's Stack Pointer — the SNA format allows some magic here (see above)
	READ_PAIR_WITH_HALT l, h      ; Read stack
	ld sp, hl                     ; Restore Snapshots_SP (the normal RET location)
	pop hl                        ; SP += 2, move past RET location — ready as a tiny stack!
	ld (SCREEN_START + (JumpInstruction - relocate) + 1), hl  ; Patch JP xxxx startup address
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
	READ_ACC_WITH_HALT   		; Border Colour
	AND %00000111		 		; Only the lower 3 bits are used (0-7)
	out ($fe),a			 		; set border
	;------------------------------------------------------------------------

	;------------------------------------------------------------------------
	; Restore DE 
 	READ_PAIR_WITH_HALT e, d   	; Restore DE directly 

	; Restore BC - !!! we are now resource-poor !!! 
	; To restore BC we can only use 'READ_ACC_WITH_HALT'
	READ_ACC_WITH_HALT			; C
	ld c,a
	READ_ACC_WITH_HALT			; B
	ld b,a
	; NOTE: BC is intentionally restored 2nd last to 
	; keep C=$1F usable for as long as possible! (i.e. READ_PAIR_WITH_HALT)
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
    ld hl, SCREEN_START       
    ld de, SCREEN_START+1        
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
	; IM1 uses the Spectrum's default interrupt handler at $0038 (simple RET).
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



; 2-42 pulse count represent keys, 1:no key, 0:invalid
GET_KEY_PULSES:    
            LD      D, $FE          	; first port row ($FE, $FD, ..., $7F)
            LD      C, 2     		    ; reserve 1 for no key
rawcheckKeyLoop: 	
            LD      A, D            
			; NOTE: IN uses A as port number
            IN      A, ($FE)      		; row state
            LD      B, 5            	; 5 keys per row (bits 0–4)
            LD      E, 1            	; Bitmask to check/move on
rawcheckBits:
            RRCA                    	
            JR      NC, rawKeyFound    	; key down (bit=0)
            INC     C              	 	; add another pulse
            SLA     E           	    ; move bitmask
            DJNZ    rawcheckBits
            RLC     D               	; Next row (D=$FD,$FB,...)
            JR      C, rawcheckKeyLoop
            ; No key found 
			ld 		c,1
rawKeyFound:
            LD      A, C            	; key pulses (2–41)
            JP      DONE_KEY




; CHANEGE THE 'NO KEY' TO USE 1 , KEY INDEX WILL BEGIN AT 2 

; GET_RAW_KEY:    
;             LD      D, $FE          ; Start with first port row ($FE, $FD, ..., $7F)
;             LD      C, 1            ; Start key index at 1
; rawcheckKeyLoop: 	
;             LD      A, D            
; 			; NOTE: IN uses A as port number
;             IN      A, ($FE)        ; Read key row state
;             LD      B, 5            ; 5 keys per row (bits 0–4)
;             LD      E, 1            ; Bitmask for bit check
; rawcheckBits:
;             RRCA                    ; Rotate bit into carry
;             JR      NC, rawKeyFound    ; If key down (bit = 0), return index in C
;             INC     C               ; Next key index
;             SLA     E               ; Shift bitmask
;             DJNZ    rawcheckBits
;             RLC     D               ; Next row (D = $FD, $FB, ...)
;             JR      C, rawcheckKeyLoop
;             ; No key found (C=41)
; rawKeyFound:   
;             LD      A, C            ; A = key index (1–40) or 0 if none
;             JP      DONE_KEY


;------------------------------------------------------------------------
GET_KEY:   ;; PUSH    BC            
           ;; PUSH    HL              
            LD      HL, KEY_TABLE    
            LD      D, $FE          ; 1st Port to read
checkKeyLoop: 	
			LD      A, D            
	        IN      A, ($FE)        ; Read key state
            LD      E, $01          ; Bitmask for key check
            LD      B, $05          ; 5 bits (keys) in each row
CheckKey:   RRCA                    ; 
            JR      NC, KeyFound 	; key down (bit checked is zero)
            INC     HL              ; next lookup
			SLA     E         		; move test mask
            DJNZ    checkKeyLoop        
            RLC     D               ; next port row to read ($FD,$FB...)
            JR      C, checkKeyLoop
KeyFound: 	LD      A, (HL)         ; get char value from table
           ;; POP     HL            
        ;;   POP     BC              
			JP DONE_KEY
          ;  RET    					; A holds result (UPPER CASE)          

KEY_TABLE:	; Key row mapping  		; $6649
			defb $01,"ZXCV"		 	; $01=shift	
			defb "ASDFG"
			defb "QWERT"
			defb "12345"
			defb "09876"
			defb "POIUY"  
			defb $0D,"LKJH"     	; $0D=enter
			defb $20,$02,"MNB"  	; $20=space, $02=sym shft
			defb $0					; tables end marker
;------------------------------------------------------------------------

;TARGET:  DB 'S', 'N', 'A', '!', 0
;
;CHECK_STARTUP_STRING:
;LD HL, TARGET
;
;CHECK_LOOP:
;    READ_ACC_WITH_HALT 
;	cp (HL)
;    JP NZ, not_match   
;    INC HL        
;    LD A, (HL)    
;    CP 0          
;    JP Z, match_found 
;    JP CHECK_LOOP     
;not_match:
;    LD A, 0             ; failure
;    JP check_end        
;match_found:
;    LD A, 1             ; success
;check_end:
;	ret

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
; 5/ reorder incoming snapshot header [DONE]
; 6/ Options to change loading boarder (off, colour choice)
; 7/ Options to slow down loading, mimic tapes
; 8/ load binary TAP files (add command "T")
; 9/ load TZX files
; 10/ Load Z80 Files
; 11/ Load PNG file
; 12/ Support poke cheats
; 13/ Analyse games at load time, add lives + cheats
; 14/ Analyse music players extract, enable for AY on 48k.
; 15/ Compress data/unpack - with the aim of faster load times (so maybe not)
; 16/ Very simple run-length encoding - could pay off - find most frequent/clear all mem to that/skip those on loading ? 
; 17/ View game screens/scroll and pick one by picutre view (load just sna screen part)
; 18/ Move stack for menu stuff (currently it's using screen mem) [DONE]
; 19/ Relocate stack anywhere in memory [DONE]
; 20/ Launcher code can be relocated depending on game (maybe final unused ram outside screen?!?!). This will need to be done Arduino side
; 21/ Command to upload code and execute (this will do the 20/ relocate)
;*****************************
; 27-byte SNA Header Example (Reordered by Arduino to aid Z80 register restoration)
; [0 ] I            = 0x3F       
; [1 ] HL_          = 0x2758     
; [3 ] DE_          = 0xB462     
; [5 ] BC_          = 0x3F62     
; [7 ] AF_          = 0x12A8     
; [9 ] HL           = 0xFC0B    
; [11] DE           = 0x98B2    
; [13] BC           = 0x0012     
; [15] IY           = 0x5C3A  [i.e. received 1st]
; [17] IX           = 0xDB59     
; [19] IFF2         = 0x00       
; [20] R            = 0x5F       
; [21] AF           = 0x4040  [i.e. received last]
; [23] SP           = 0x6188     
; [25] IM           = 0x01       
; [26] BorderColour = 0x00       

; The Arduino C code reorders the SNA header before sending.
; This rejig make things better suited for restoring Z80 states.
;  sendBytes(head27_1, (1/*"E"*/) + 1+2+2+2+2 );  	// Send command "E" then I,HL',DE',BC',AF'
;  sendBytes(&head27_1[1 + 15], 2+2+1+1 );   		// Send IY,IX,IFF2,R (packet data continued)
;  sendBytes(&head27_1[1 + 23], 2);          		// Send SP                     "
;  sendBytes(&head27_1[1 +  9], 2);         		// Send HL                     "
;  sendBytes(&head27_1[1 + 25], 1);       		   	// Send IM                     "
;  sendBytes(&head27_1[1 + 26], 1);     		   	// Send BorderColour           "
;  sendBytes(&head27_1[1 + 11], 2);        		  	// Send DE                     "
;  sendBytes(&head27_1[1 + 13], 2);       		   	// Send BC                     "
;  sendBytes(&head27_1[1 + 21], 2);         	 	// Send AF                     "








; **** SCRATCH PAD ********

; ;----------------------------------------
; ; fill32: Fills 32 bytes 
; ;----------------------------------------
; ; Inputs:
; ;   HL = target SP (caller precomputes start_address +32)
; ;   A  = fill value
; ;----------------------------------------
; fill32:
;     ld   (saved_sp), sp    ; 20t
;     ld   sp, hl            ; 6t
;     ld   h, a              ; 4t
;     ld   l, a              ; 4t
;     push hl                ; 16×11t = 176t
;     push hl
;     push hl
;     push hl
;     push hl
;     push hl
;     push hl
;     push hl
;     push hl
;     push hl
;     push hl
;     push hl
;     push hl
;     push hl
;     push hl
;     push hl
;     ld   sp, (saved_sp)    ; 20c
;     JR mainloop                      ; 10c

; saved_sp:
; dw 0                  ; SP storage

; ; Z80 fill routine (BC must be >=2)
; ; Inputs: DE=destination, BC=size (>=2), A=fill value
; ; Clobbers: AF, BC, DE, HL
; 	READ_PAIR_WITH_HALT b,c  ; DE = Fill amount
; 	READ_PAIR_WITH_HALT d,e  ; HL = Destination address
; 	halt				 	 ; Synchronizes with Arduino (NMI to continue)
; 	in a,(c)				 ; Read fill value from the z80 I/O port
; Fill:
;     LD HL, DE       ; 
;     LD (HL), A      ; 1st byte        
;     INC HL          ; HL = dest+1             
;     EX DE, HL       ; DE = dest+1, HL=dest    
;     DEC BC          ; BC = size-1             
;     LDIR            ; Fill
;    JR mainloop     ;   



; Fill using stack
; Inputs: DE=dest, BC=size/2, A=value
; Clobbers: AF, BC, DE, HL, SP
; Preserves: Original SP, re-enables interrupts

; Fill:   
;     ; Prepare 16-bit fill pattern
;     LD H, A           ; H = fill value          [4]
;     LD L, A           ; L = fill value          [4]
       
;     ; Position stack at buffer end
;     LD SP, DE         ; SP = start address      [6]
;     ADD SP, BC        ; SP += word count        [10]
;  	  ADD SP, BC 

;     ;;;;;;;;;; Convert byte count to PUSH count (BC /= 2)
;     ;;;;;;;;SRL B             ;                         [8]
;     ;;;;;;;;RR C              ; BC = size in words      [8]
    
;     ; Fast fill loop
; FillLoop:
;     PUSH HL           ; Write 2 bytes           [11]
;     DEC BC            ; Decrement word counter  [6]
;     LD A, B           ; Check if counter = 0    [4]
;     OR C              ;                         [4]
;     JR NZ, FillLoop   ;                         [12/7]
    
;	  ld SP,WORKING_STACK
;     JR mainloop 




; 	LD HL,0 
;	ADD HL,SP 
;
;	...
;
;	LD SP,HL 
;
;saved_sp: 
;dw 0