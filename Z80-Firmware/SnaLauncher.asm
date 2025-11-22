; ZX SPECTRUM 48K MEMORY MAP:-
;        rom: $0000 - $3FFF (16 KB ROM)
;     screen: $4000 - $57FF (6144 bytes - screen display data)
; attributes: $5800 - $5AFF (768 bytes - screen color attributes)
;
;******************************************************************************
;   CONSTANTS
;******************************************************************************
SCREEN_START				EQU $4000  ; [6144 bytes]
SCREEN_END					EQU $57FF
SCREEN_ATTRIBUTES_START		EQU $5800  ; [768 bytes]
SCREEN_ATTRIBUTES_END		EQU $5AFF  
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
	;------------------------------------------
	; For a test on both 48/128k machines (now using '_ROMCS' & 'ROM1_OE')
	;       48k: _ROMCS (edge 25B) is NOT USED on +2a/b, +3 boards.
    ; +2a/b, +3: Added test jumper wire from 'ROM1 _OE' (edge 4A) to 5v.
	; 			 (loading games works fine on 128k machines (minor reset timing tweek))
	;------------------------------------------
	; Disable 128k Spectrums from paging
	LD A, %00100000  ; bit 5, paging disabled until the computer is reset
    LD BC, $7FFD     ; 7FFD = paging port
    OUT (C), A    
	;------------------------------------------

	;-----------------------------------------------------------------------------------	
	;Stack Behavior - 	The stack grows downward.
	;					PUSH: sp -= 2; stores value at (sp).
	;					POP: loads value from (sp); sp += 2.
	;*** IMPORTANT WARNINGS ***:
	; Code limitations when using "command_Execute":
	; Total stack usage must be only 1 level deep (2 bytes, just one push).
	; Do NOT use PUSH/POP around interrupts! (Interrupts automatically push PC onto the stack).
	; (While in the menu code, it's okay to use the stack normally.)

	;-----------------------------------------------------------------------------------	
	SET_BORDER 0
	jp ClearAllRam 
ClearAllRamRet:
	; Now we know all RAM is zeroed
	; This means we can now assume loading can skip large chunks of zero-length data
	
	ld SP,0xFFFF  ; !!! This stack location is only for menu use !!!
	; Note: At game loading time, SP is reasigned with 'command_Stack' from the Arduino.
	;       We use screen memory for the temp stack (our stack usage is only 1 deep!) while restoring the game.

	call sendFunctionList
	jp mainloop
 
;----------------------------------------------------------------------------------
; MASKABLE INTERRUPT (IM 1) - Vector: 0x0038  
; Note: 'EI' is intentionally omitted - we don't use IM1 in the usual way.  
ORG $0038
L0038:   
	RET 	; 10 t-states
;----------------------------------------------------------------------------------

; notes: NMI Z80 timings
; 		 Push PC onto Stack: 10/11 T-states
; 		 RETN Instruction:   14 T-states.
;----------------------------------------------------------------------------------
; NON-MASKABLE INTERRUPT (NMI) - Vector: 0x0066  
; The HALT instruction and this NMI are used for synchronization with the Arduino.  
; The Arduino monitors the HALT line and triggers the NMI to let the Z80 exit the HALT state. 
;				     
ORG $0066  ; location needs to use up 14 bytes
L0066:
	; WARNING: NMI disables maskable interrupts (IFF1=0, old IFF1 saved into IFF2).
	; If we return with 'RET' and not 'RETN', IFF1 will stay cleared and maskable interrupts will never come back.
	; UPDATE: Above warning still applies - however this rule must be broken when the game is
	;         running and the NMI fires to bring up the in-game menu, this time using bank switching
	;         with control via idle JR -2 loops and NOPs in the mirror to swap into.

	; -------------------------------------------
	; The Sna ROM will use this routine 'RETN' while in the file browser menu.
	; The idea is to get in and out from this NMI as quickly as possible!
	;
	; The mirror ROM continues here
	RETN 	; 2 bytes		MIRROR STOCK ROM - Pushes x4 registers here
	NOP		; 1 byte			 	  	   	   
	NOP		; 1 byte			 	            
	;
	; Stock ROM has instructions to ".idle: jr .idle" here
	; These NOPs will release the mirror's idle loop and turn JR -2 into JR 0 (same as a slow NOP)
    NOP		; 1 byte 				MIRROR ROM - Uses jr -2
    NOP		; 1 byte 							
	;
	NOP
	NOP
	NOP
	NOP
	NOP
	jp .IngameHook


;------------------------------------------------------
;******************
; *** MAIN LOOP ***
;******************
mainloop:
	ld c,$1F  	; setup for 'READ_PAIR_WITH_HALT'
check_initial:   
	READ_PAIR_WITH_HALT h,l  ; HL = jump address
	JP (hl) ; 
	jp check_initial	
;----------------------------------------------------------------------------------

sendFunctionList:
	; Hardware Info: The game cartridge latches these OUT values using a 74HC574PW latch IC.
	; The hardware performs a lazy check on address lines - A7=0 enables latching (requires #IORQ + #RD).
	; Each HALT synchronizes the Z80 with the Arduino. The Arduino enables the latch outputs for reading,
	; then disables them (tri-state) and signals the z80 to continue (un-halts).

 	LD C, $1F			  
	;--------------------------------
	ld hl,command_TransmitKey
   	OUT (C), l    		  
	halt				
	OUT (C), h    		  
	halt				
	;--------------------------------
	ld hl,command_Fill
	OUT (C), l    		  
	halt				
	OUT (C), h    		  
	halt
	;--------------------------------
	; ld hl,command_SmallFill
	; OUT (C), l    		  
	; halt				
	; OUT (C), h    		  
	; halt
	;--------------------------------
	ld hl,command_Transfer
	OUT (C), l    		  
	halt				
	OUT (C), h    		  
	halt
	;--------------------------------
	ld hl,command_Copy
	OUT (C), l    		  
	halt				
	OUT (C), h    		  
	halt
	;--------------------------------
	ld hl,command_Copy32
	OUT (C), l    		  
	halt				
	OUT (C), h    		  
	halt
	;--------------------------------
	ld hl,command_VBL_Wait
	OUT (C), l    		  
	halt				
	OUT (C), h    		  
	halt
	;--------------------------------
	ld hl,command_Stack
	OUT (C), l    		  
	halt				
	OUT (C), h    		  
	halt
	;--------------------------------
	ld hl,command_Execute
	OUT (C), l    		  
	halt				
	OUT (C), h    		  
	halt
	;--------------------------------
	ld hl,command_fill_mem_bytecount
	OUT (C), l    		  
	halt				
	OUT (C), h    		  
	halt
	;--------------------------------
	ld hl,command_SendData
	OUT (C), l    		  
	halt				
	OUT (C), h    		  
	halt
	;--------------------------------
	ret



;------------------------------------------------------;
command_SendData:	

	READ_PAIR_WITH_HALT d,e  ; DE = amount
	;ld de, 6144+768  ; DE = Fill amount 2 bytes
 	READ_PAIR_WITH_HALT h,l  ; HL = start address 
	;ld hl, 16384  ; HL = Destination address

	LD C, $1F
.transmitLoop:
	ld a,(hl)	
   	
	OUT (C), A    		  ; Game cart automatically latches value (with latch ic: 74HC574PW)
	halt				  ; Halt line - Arduino knows is safe to #EO and read new value from latch

    inc hl		
    DEC DE      
    LD A, D     
    OR E        
    JP NZ, .transmitLoop 

	jp mainloop    
;------------------------------------------------------;


;------------------------------------------------------
; Fast memory fill 
; Input: Buffer end address, total fill count (byte), fill byte value (byte)
;------------------------------------------------------
command_fill_mem_bytecount:  

    READ_PAIR_WITH_HALT h,l ; HL = buffer end address (fill backwards) 
    halt                     ; Synchronization 
    in b,(c)                 ; B = total fill count
    READ_ACC_WITH_HALT       ; A = fill byte value

    LD IX, 0
    ADD IX, SP               ; save SP

CheckParity:
    BIT 0, B                 ; odd or even count?
    JR Z, StartEvenFill      

HandleOddByte:
	DEC HL
	LD (HL),a
    DEC B                    ; B is now even
    JR Z, RestoreSP    		 ; zero (only when initial count of 1)

StartEvenFill:
    SRL B                    ; count/2 to give no# PUSH operations
	LD SP, HL                ; Point SP to the end address for the PUSH trick

	LD H, A                  ; fill byte
    LD L, A                  ;  "
FillLoopOptimized:
    PUSH HL                  ; x2 byte fill
    DJNZ FillLoopOptimized   
        
RestoreSP:
	LD SP,IX               
    jp mainloop             


;------------------------------------------------------
command_TransmitKey:
;------------------------------------------------------
	JP GET_KEY            ; Returns key in A
	GET_KEY_RET:		  ; (jump back - avoid stack usage)

;;;;;;;;;;;;;;;;	halt

    LD C, $1F			  ; #IORQ + A7 + #RD
    OUT (C), A    		  ; Game cart latches value (latch ic: 74HC574PW)
	halt				  ; Halt line - Arduino knows is safe to #EO and read new value from latch
	jp mainloop

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
    JP mainloop 	; Return back for next transfer command

; ;------------------------------------------------------
; command_SmallFill:  ; "f" - Small Fill 
; ;------------------------------------------------------
; 	halt 
; 	in b,(c)     			  ; Fill count (1 byte)
; 	READ_PAIR_WITH_HALT h,l   ; HL = Destination address
; 	halt                      ; Wait for Arduino
; 	in a,(c)                  ; Read fill value into A
; smallfillLoop:
; 	ld (hl),a                 ; Store fill byte
; 	inc hl
; 	djnz smallfillLoop             ; Loop until B == 0
; 	JP mainloop

;---------------------------------------------------
command_Transfer:  ; "G" - TRANSFER DATA (flashes border)
;---------------------------------------------------
	halt
	in b,(c)  					; B = transfer size
	READ_PAIR_WITH_HALT h,l 	; HL = destination

readDataLoop:
	halt
	ini   						; (HL)<-(C), B<-B-1, HL<-HL+1	[16]
	JP nz,readDataLoop			;								[10]
	ld a,R
	AND LOADING_COLOUR_MASK  	; Use data for border flash
	out ($fe), a
	jp mainloop

;------------------------------------------------------
command_Copy:  ; "C" - COPY DATA
;------------------------------------------------------
	; Same as TRANSFER DATA but without the flashing boarder.
	halt							; Synchronizes with Arduino (NMI to continue)
	in b,(c)  				; The transfer size: 1 byte
	READ_PAIR_WITH_HALT h,l	; HL = Destination address
CopyLoop:
	halt		 			; Synchronizes with Arduino (NMI to continue)
	; was...	
	; in a,($1f)   			; Read a byte from the z80 I/O port [11]
	;	ld (hl),a	 		;	 write to memory				[7]
	;   inc hl				;									[6]	
	;	djnz CopyLoop  		; read loop							[13] TOTAL: [37 t-states]
	ini   				; (HL)<-(C), B<-B-1, HL<-HL+1		[16]
	JP nz,CopyLoop		;									[10]

  jp mainloop 			; done - back for next transfer command

;------------------------------------------------------
command_Copy32:  ; 'Z' - Copy Block of Data (32 bytes)
;  ini: [HL]:=port[BC], HL:=HL+1, B:=B-1, Z is set if BC == 0
;------------------------------------------------------
	READ_PAIR_WITH_HALT h,l	; HL = Destination address
	REPT 32					; transfer x32 bytes [total:640 t-states]
	halt					; [4]
	ini						; [16] 
	ENDM
  JP mainloop 			; done - back for next transfer command

;--------------------------------------------------------
command_VBL_Wait:  ; "W" - Wait for the 50Hz maskable interrupt
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
command_Stack:  ; 'S'  - store or restores snapshots stack 
;-----------------------------------------------------

	READ_PAIR_WITH_HALT h,l 
  	READ_ACC_WITH_HALT       ; A = Save:0 or Resotre:1

	or a    
    jr nz, .restore 
	
.store:
	ld HL,0
	add HL,SP
	OUT (C), L
	halt				
	OUT (C), H    		  
	halt
 	jp mainloop    
.restore 	
	ld sp,hl
    halt  					; synchronization with Arduino
    jp mainloop          	
	
;-------------------------------------------------------------------------------------------
command_Execute:  ; "E" - EXECUTE CODE, RESTORE & LAUNCH 
; Restore snapshot states & execute stored jump point from the stack
;-------------------------------------------------------------------------------------------

	;-------------------------------------------------------------------------------------------
	; Setup - Copy code to screen memory, as we'll be switching ROMs later.
	; After the ROM swap, we lose this sna ROM code in exchange for the stock ROM,
	; so the launch code will run from screen RAM.

	; TODO - THIS RELOCATED CODE USED TO DO FAR MORE - NOW THIS COPY IS OVERKILL)

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
	; -- Restore Alternate Registers HL',DE',BC',AF' --
	ld c,$1f  				 ; Just use C above for LDIR copy, setup again for pair read 
	READ_PAIR_WITH_HALT L,H  ; future HL'
	READ_PAIR_WITH_HALT e,d  ; future DE'
	READ_ACC_WITH_HALT		 ; C'
	ld c,a
	READ_ACC_WITH_HALT		 ; B'
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
	LD IYL, e  
	LD IYH, d  

	READ_PAIR_WITH_HALT e,d;  ; read IX
	LD IXL, e  
	LD IXH, d  

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
	; Note: The 'SNA' format does not store the program counter in the header, it's put in the stack.
	; Final Stack Usage: 
	; Instead of using RETN to start the .SNA program, we jump directly to the starting address. 
	; This frees up 2 bytes of stack space that would normally be used to hold the return address.
	; NOTE: The HALT instruction requires an active stack for synchronization. The 2 freed bytes is used 
	; as a temporary single-level stack, just enough for the final "resource-critical" restore stages.
	;
	; Restore Program's Stack Pointer
	READ_PAIR_WITH_HALT l, h      ; stack pointer
	ld sp, hl   				  
	pop hl                        ; SP+=2, move past RET location (now we can use this area as a tiny stack!)
	ld (SCREEN_START + (JumpInstruction - relocate) + 1), hl  ; Patch JP xxxx with games startup address
	;-----------------------------------------------

	;------------------------------------------------------------------------
	READ_PAIR_WITH_HALT l,h  	; restore HL
	;------------------------------------------------------------------------

	;------------------------------------------------------------------------
	READ_ACC_WITH_HALT  		; gets value of IM into reg-A
	JP RestoreInterruptMode 
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
	; NOTE: BC restored late as possible to avoid trashing register C
	READ_ACC_WITH_HALT			; C
	ld c,a
	READ_ACC_WITH_HALT			; B
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
	READ_ACC_WITH_HALT       ; Load original A (accumulator) - AF is now fully restored

	JP L16D4	; path to start game!
;-----------------------------------------------------------------------	

;-----------------------------------------------------------------------    
; RELOCATE SECTION - ROM swap code
; Games need the original Spectrum ROM, but we can't execute ROM code during ROM swap
; so we copy this small routine to screen memory before swapping ROMs.
; This 3-byte section runs from screen memory directly after we switch back to stack rom 
;-----------------------------------------------------------------------    
relocate:  			; I've found ways to reduce this - now at 3 bytes.
JumpInstruction:     
    jp 0000h 		; Jump to restored program (address patched above)
TempStack:        	; Temporary stack space (2 bytes)
relocateEnd:
;-----------------------------------------------------------------------	

;-----------------------------------------------------------------------
; SCREEN CLEAR - Initialize display memory
;-----------------------------------------------------------------------
ClearScreen:
	SET_BORDER 0
   	ld a, 0
    ld hl, SCREEN_START
    ld de, SCREEN_START+1
    ld bc, 6144					; Screen bitmap
    ld (hl), a
    ldir
    ld bc, 768-1				; Screen attributes
    ld (hl), a
    ldir
    ret
;-----------------------------------------------------------------------	

;-----------------------------------------------------------------------
; CLEAR RAM 48KB - 273,725 T-states / 3,500,000 = 0.0782
; +0.08 seconds (will be a bit more due to contended memory)
;-----------------------------------------------------------------------
ClearAllRam: 
        LD IX, 0        
        ADD IX, SP               
        ld hl, 0                 
        ld sp, 16384 + (48*1024) ; top of RAM (65536)
        ld b, 0                  ; wraps so 256 times
clr:    REPT 96
        push hl           		 ; 11 * 96 * 256
        ENDM
        djnz clr          		 ; (256 * 13) - (13-8)
        ld sp, ix                

		jp ClearAllRamRet 
;-----------------------------------------------------------------------	

;-----------------------------------------------------------------------	
; RESTORE INTERRUPT MODE - Helper for command_Execute
;-----------------------------------------------------------------------	
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
;	NOTE: JP back not RET as subroutine calls due to the one-level stack constraint
RestoreEI_IFFState:
	AND	%00000100   		 			; get bit 2
	jp	z,RestoreEI_IFFStateComplete  	; disabled - skip 'EI'
	EI
	jp RestoreEI_IFFStateComplete		 ; avoiding stack based calls
;------------------------------------------------------------------------

; GET_KEY - Reading Keyboard Ports
; A zero in one of the five lowest bits means that the corresponding key is pressed.
; Returns A with Key, 0 for none
GET_KEY:    LD      HL, KEY_TABLE    
            LD      D, $FE          ; 1st Port to read
KEY_LOOP: 	
			LD      A, D            
	        IN      A, ($FE)        ; Read key state
            LD      E, $01          ; Bitmask for key check
            LD      B, $05          ; 5 bits (keys) in each row
CheckKey:   RRCA                    ; 
            JR      NC, KEY_PRESSED 	; key down (bit checked is zero)
            INC     HL              ; next lookup
			SLA     E         		; move test mask
            DJNZ    CheckKey        
            RLC     D               ; next port row to read ($FD,$FB...)
            JR      C, KEY_LOOP
KEY_PRESSED: 
			LD      A, (HL)         ; get char value from table          
            JP 		GET_KEY_RET                  
; ----------------------------------------------
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

ORG $04AA  ; *** KEEP THIS 24 bytes long to match the stock ROM ***

; Exit mechanism – the Arduino signals a jump to 0x04AD in SNA ROM (chosen arbitrarily),
; initiating the path back to the game.

; Path back to the game - when the stock ROM is swapped back in, it overrides the
; idle loop, restores the registers, and returns execution to the game.
; (Technically, from the game's perspective we are still inside the first NMI)

; Safeguards: The CPU may fetch in the middle of an instruction during the ROM
; swap. If the displacement byte comes from the new ROM (which contains NOPs),
; a "JR -2" becomes "JR 0" (effectively a two-cycle slow NOP).

L04AA:
	NOP
	NOP
	NOP
	NOP  ; 0x04AD: <<< path back to stock rom 
	NOP  ; 0x04AE:
	NOP
    ld  HL,16384
    bit 7,(HL)            	 			  ; check EI flag
    jr  z,.ContinueGameIdle  			  ; if clear, don’t re-enable
    ei    					 			  ; restore EI
.ContinueGameIdle: jr .ContinueGameIdle   ; ROM sync, swapping back to stock
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP

;------------------------------------------------------------------------

ORG $04AA+24  ; 0x04C2

.IngameHook:

		ld HL,16384  
        ld a,i          	; set flags, P/V = IFF2
        jp po,.WasOff    	; parity odd -> IFF2=0
.WasOn:
		set 7,(HL)            ; mark EI needed (bit7=1)
	;;;	SET_BORDER 3  		; Magenta DEBUG
	    ; We're running inside an NMI handler.  To avoid surprises,
        ; set interrupt state to disabled and restore later.
		di
        jr   .StoreDone
.WasOff: 
 		res 7,(HL)            ; clear EI flag (bit7=0)
	;;;	SET_BORDER 4  		; green DEBUG
.StoreDone:

; ; ; ; ------------------------------------------
; ; ; ; ------------------------------------------
; ; ; ; ------------------------------------------
; ; ; ; Send the screen to the Arduino to store
; ; ; ; We wish do modify the games screen by adding a ingame menu for
; ; ; ; things like activating pokes

; ; ; 	ld de, 6144+768  ; DE = Fill amount 2 bytes
; ; ; 	ld hl, 16384  ; HL = Destination address
	
; ; ; 	LD C, $1F
; ; ; transmitScreenDataLoop:
; ; ; 	ld a,(hl)	

; ; ; ;;;;;;;;	ld a, %00011000  ; TEST ONLY

; ; ;    	OUT (C), A    		  ; Game cart automatically latches value (with latch ic: 74HC574PW)
; ; ; 	halt				  ; Halt line - Arduino knows is safe to #EO and read new value from latch

; ; ;     inc hl		
; ; ;     DEC DE      
; ; ;     LD A, D     
; ; ;     OR E        
; ; ;     JP NZ, transmitScreenDataLoop 

; ; ; ; ------------------------------------------
; ; ; ; ------------------------------------------
; ; ; ; ------------------------------------------

	jp mainloop


;------------------------------------------------------------------------

; -------------------------------------------------------------------------
; *** ROM SWAP - GAME STARTUP ***
;
; The stock ROM ends just before the screen memory at 0x4000.
; L16D4: This area contains 7 unused bytes in the stock ROM that we mirror here.
; We use it to safely return to the game code.
; (Earlier versions without this technique required a precise wait 
;  on the Arduino side to synchronize after HALT/NMI.)
;
; Arduino process:
; - The Arduino sends a "JP 0x04AD" command. 
; - The Arduino then waits briefly to allow the Z80 enter the idle loop.
; - The Arduino switches back to the STOCK ROM.
;   The stock ROM does not contain the idle loop: it will POP the game registers and use
;   RET (not RETN) to exit the NMI so we control interrupt restoration.
;
; Note:
; In the stock ROM patches address L16D4 to execute "JP $3FFF".
; ROM byte ($3FFF) which is 0x3C (INC A). After INC A the CPU continues into 
; RAM at $4000 where our game launch code (JP <GAME_START_ADDR>) is.
; -------------------------------------------------------------------------

ORG $16D4       ; Must use all 7 bytes here (this code will be swapped out)
L16D4:  
	; --------------------------------------------------------------------
	; **** idel loop - mirrow rom will break us out of this with nops ****
	dec a  								; cancel out the mirrored ROM’s INC A
	.idleGameStart: jr .idleGameStart   ; Idle loop
	; -------------------------------------------------------------------- 
	nop
	nop
	nop  ; mirror rom does a "JP $3fff" here
	nop
;-------------------------------------------------------------------------

;------------------------------------------------------------------------
org $3ff0	  		; Locate near the end of ROM
debug_trap:  		; If we see border lines, we probably hit this trap.
	SET_BORDER a
	inc a
	jr debug_trap
;------------------------------------------------------------------------

org $3fff
;START_GAME: halt ; this will coninue into ram (screen memory)
;START_GAME: 
;	dec a  ; stock rom has 0xffff:inc a and we need that path back.
;.idleGameStart: jr .idleGameStart  ; the stock will continue with "inc a" 
; note: Luckly the last byte is useable as it's part of the charater set, the (c) symbol.
; Doing this idle loop allows the stock rom to take control and continue
; without needed to wait a precise number of Z80 t-states over on the Arduino, as 
; this othjer wise need a HALT and NMI soforcing the Arduino to delayMicroseconds(7).

; note: L16D4 get this path ready for the stock rom

	NOP  ; REF ONLY: mirror rom runs this - this is never called

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
; 22/ Joystick remapping including 2nd button support
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


; usefull ASM tips
; https://github.com/rejunity/zx-racing-the-beam/blob/main/screen_timing.asm

; (64+192+56)*224 = 69888 T-states (One scanline takes 224 T-states)
	; Total per frame: 312 PAL lines*224 = 69888 T-states (48k Speccy) 
	; Vertical Blank: 14336+11637 = 25972 T-states (Uncontended)




; ======================
; ARCHIVED CODE SECTION:
; ======================

; The 1-bit HALT-pulse protocol has been replaced as I've added additional supporting hardware
; to the curcuit design allowing the OUT instruction to be used instead.
;
; The HALT-pulse method is actually quite impressive - it's surprising how well it works.
; It’s probably a bulletproof poor mans out method, since the pulse timing
; makes it far less likely to be affected by interference - as HALT stays active
; long enough to ride through most real-world noise.

; ;------------------------------------------------------
; ; transmit8bitValue - Encodes/transmits using HALT pulses.
; ; A holds 8-bit value to transmit.  It is used to send things
; ; like key presses and byte data to the Arduino.
; ; transmit8bitValue:
; ; ;------------------------------------------------------
; ; 	ld d,8              ; Bit counter
; ;  .bitlooptx:
; ;     bit 7,a             ; Test MSB of A
; ;     HALT
; ;     jr z,.bitEndMarkerDelaytx  ; If 0, skip second HALT
; ;     HALT                 ; Second HALT for '1'
; ;  .bitEndMarkerDelaytx:
; ;     ld e,40              ; Inter-bit delay
; ;  .delayMarkerLooptx:
; ;     dec e
; ;     jr nz,.delayMarkerLooptx
; ;     add a,a              ; Shift left
; ;     dec d
; ;     jr nz,.bitlooptx
; ; 	jp mainloop

;---------------------------------------------------------------------------------
; transmit16bitValue - Encodes/transmits using HALT pulses. It is used to send
; command function addresses to the Arduino.
; 
; HL holds 16-bit value to transmit, x1 HALT = '0' bit, x2 HALTs = '1' bit
; ; transmit16bitValue:  
; ;     ld d,16          		 ; Bit counter
; ; .bitloop:
; ;     bit 7,h
; ;     HALT
; ;     jr z,.bitEndMarkerDelay  ; If bit is '0', skip second HALT
; ;     HALT     	             ; Second HALT for bit '1'
; ; .bitEndMarkerDelay:
; ;     ld e,40   	 	         ; Delay
; ; .delayMarkerLoop:
; ;     dec e
; ;     jr nz,.delayMarkerLoop
; ;     add hl,hl       		 ; Shift HL left
; ;     dec d
; ;     jr nz,.bitloop
; ;     ret


;



; ;------------------------------------------------------
; command_FillVariableEven:  ; Fast memory fill for even byte counts
; ; Input: Buffer end address, fill count/2, fill byte value
; ;------------------------------------------------------
;     LD IX, 0        
;     ADD IX, SP     
;     READ_PAIR_WITH_HALT h,l ; HL = buffer end (fill backwards) 
;     halt                    ; Synchronization 
;     in b,(c)                ; B = number of PUSH operations (count/2)
;     READ_ACC_WITH_HALT      ; A = fill byte
;     LD SP, HL          		; buffer
;     LD H, A                 ; 
;     LD L, A                 ; HL = fill value/pattern
; FillEvenLoop:
;     PUSH HL                   
;     DJNZ FillEvenLoop        
; 	ld SP,IX    
;     jp mainloop                

; ;------------------------------------------------------
; command_FillVariableOdd:  ;  Memory fill for odd byte counts
; ; Input: Buffer end address, (count-1)/2, fill byte value
; ;------------------------------------------------------
;     LD IX, 0        
;     ADD IX, SP 
; 	READ_PAIR_WITH_HALT h,l    ; HL = buffer END address
; 	halt                       ;
; 	in b,(c)                   ; B = number of PUSH operations ((count-1)/2)
; 	halt                       ;
; 	in d,(c)                   ; D = fill byte
; 	dec hl                     ; handle the odd byte
; 	ld (hl), d
; 	ld sp, hl                  ; Set SP to buffer end - 1
; 	ld h, d                   
; 	ld l, d                    ; HL = fill value/pattern
; FillOddLoop:
; 	push hl                 
; 	djnz FillOddLoop        
; 	ld SP,IX   
; 	jp mainloop             

