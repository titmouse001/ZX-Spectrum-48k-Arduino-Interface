;------------------
; SNA ROM
;------------------
;
; This Asm file is the source for the 16k sna rom image.
; The Make.bat script will assemble x2 roms:
;   (1) Upper 16k: SnaLauncher.asm (this sna rom - the remote command/launcher code)
;   (2) Lower 16k: A lightly modified version of the original 48K ROM (48KROM.asm)
; A final 32k ROM is built by joining the two 16k ROM images.
;
;-----------------------------------------------------------------------------------
;
; _Make.bat will build everything and put the build rom in output/EPROM_PAIR.bin
;
;-----------------------------------------------------------------------------------
;
; The modified stock ROM (48KROM.asm) has been altered in areas that are never used. 
; I've stayed away from anything used in Spectrum games. e.g. The 0xFF padding for 
; IM 2 vectors remains untouched, as most 48k games rely on it.
;
;
; Tested on both 48K and 128K machines (using '_ROMCS' & 'ROM1_OE').
; - 48K: _ROMCS (edge 25B) is NOT USED on +2a/b or +3 boards.
; - PCB now includes jumper labeled "BOOT128" which allows 128K machines to access 
;   the sna ROM on 48K machines. Infact it could just be solder blob/bridge and 
;   has no effect on 48k machines.
;   128k needs longer pause for a80 reset - see Arduino code 'Z80Bus::resetZ80()'
;
;******************************************************************************
;   CONSTANTS
;******************************************************************************
SCREEN_START                EQU $4000
SCREEN_END                  EQU $57FF
SCREEN_SIZE                 EQU SCREEN_END - SCREEN_START + 1 ; 6144 bytes

SCREEN_ATTRIBUTES_START     EQU $5800
SCREEN_ATTRIBUTES_END       EQU $5AFF
SCREEN_ATTRIBUTES_SIZE      EQU SCREEN_ATTRIBUTES_END - SCREEN_ATTRIBUTES_START + 1 ; 768 bytes

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
	; This stack is only for menu use (this does not include the in-game menu)
	; Note: At game loading time, SP is reasigned with 'command_SetStack' from the Arduino.
	;       We use screen memory for the temp stack (our stack usage is only 1 deep!) while restoring the game.
	ld SP,0xFFFF

	;Stack Behavior - 	The stack grows downward.
	;					PUSH: sp -= 2; stores value at (sp).
	;					POP: loads value from (sp); sp += 2.
	;*** IMPORTANT WARNINGS ***:
	; Code limitations when using "command_Execute":
	; Total stack usage must be only 1 level deep (2 bytes, just one push).
	; Do NOT use PUSH/POP around interrupts! (Interrupts automatically push PC onto the stack).
	; (While in the menu code, it's okay to use the stack normally.)

	SET_BORDER 0
	call ClearScreenAttributes

	;------------------------------------------
	; Disable 128k Spectrums from paging
	; 0x7FFD : Bit 5=1 (Lock), Bit 3=0 (Screen at 0x4000), Bits 0-2=0 (Bank 0)
	LD A, %00100000   
    LD BC, $7FFD      
    OUT (C), A        ; Lock and select Bank 0/Screen 5
	;------------------------------------------

	jp mainloop

; -------------------------------------------------------------------------
	IF $ > $0038
	.ERROR "CODE OVERLAPS"
	ENDIF
; -------------------------------------------------------------------------

;----------------------------------------------------------------------------------
; MASKABLE INTERRUPT (IM 1) - Vector: 0x0038  
; Note: 'EI' is intentionally omitted - we don't use IM1 in the usual way.  
ORG $0038
L0038:   
	RET 	; 10 t-states
;----------------------------------------------------------------------------------

; -------------------------------------------------------------------------
	IF $ > $0066
	.ERROR "CODE OVERLAPS"
	ENDIF
; -------------------------------------------------------------------------

; notes: NMI Z80 timings
; 		 Push PC onto Stack: 10/11 T-states
; 		 RETN Instruction:   14 T-states.
;----------------------------------------------------------------------------------
; NON-MASKABLE INTERRUPT (NMI) - Vector: 0x0066  
; The HALT instruction and this NMI are used for synchronization with the Arduino.  
; The Arduino monitors the HALT line and triggers the NMI to let the Z80 exit the HALT state. 
;			
ORG $0066   ; Must occupy exactly 14 bytes
L0066:
    ; -------------------------------------------------------------------------
    ; NMI handling: NMI copies IFF1 to IFF2 and clears IFF1, disabling maskable 
	; interrupts. Usually, a 'RETN' is required to restore IFF1 from IFF2.
    ;
    ; Note: We should aim to restore interrupts, this rule is intentionally 
    ; broken during in-game menu activation, where we perform bank switching 
    ; and trap the main loop in a 'JR -2' state.
    ; -------------------------------------------------------------------------
    RETN     ; In File Browser mode, we exit the NMI immediately.
    NOP
    NOP
    ; -------------------------------------------------------------------------
    ; The following NOPs provide a landing zone for the mirror ROM's idle loop.
    ; The stock ROM uses a "jr -2" infinite loop here. If NMI triggers while in the loop
	; it will safely break out regardless of whether it hit the loops opcode or the operand.
    ; -------------------------------------------------------------------------
    NOP   ; Pad for mirror ROM 'jr -2'
    NOP   

    ; Additional padding to maintain the 14-byte requirement
    NOP
    NOP
    NOP
    NOP
    NOP
    
    JP .IngameHook

;------------------------------------------------------
;******************
; *** MAIN LOOP ***
;******************
mainloop:
	ld c,$1F  
	READ_PAIR_WITH_HALT h,l  ; HL = jump address
 	JP (hl) ; 
;----------------------------------------------------------------------------------

;-------------------------------
; Jump Table
ORG $00D0
	jp command_NOP					; 1
	jp command_TransmitKey			
	jp command_Fill					
	jp command_Transfer				
	jp command_Copy					
	jp command_Copy32				
	jp command_VBL_Wait				
	jp command_SetStack				
	jp command_RestoreGameAndExecute				
	jp command_fill_mem_bytecount	
	jp command_SendData				; 11
	jp command_Poke					; 12

;------------------------------------------------------
	IF $ > $00D0+(13*3)
		.ERROR "CODE OVERLAPS"
	ENDIF
;------------------------------------------------------


;------------------------------------------------------
;------------------------------------------------------
; REMOTE COMMAND SECTION (routines called via main loop)
;------------------------------------------------------
;------------------------------------------------------

;-------------------------------
; Debugging support - NOP 
command_NOP:
	SET_BORDER 2
	jp mainloop

;------------------------------------------------------
; Transmit Data to Arduno 
command_SendData:	
	READ_PAIR_WITH_HALT d,e  ; DE = amount 
 	READ_PAIR_WITH_HALT h,l  ; HL = start address 

.transmitLoop:
	ld a,(hl)
	OUT ($1f), A    	  ; Latch value (Game cart uses ic: 74HC574PW)
	halt				  ; Sync with Arduino
    inc hl		
    DEC de
    LD A, d     
    OR e        
    JP NZ, .transmitLoop 
	jp mainloop

;------------------------------------------------------


;------------------------------------------------------
; Small Memory Fill (byte amount)
; Input: Buffer end address, total fill count (byte), fill byte value (byte)
;------------------------------------------------------
command_fill_mem_bytecount:  

    READ_PAIR_WITH_HALT h,l ; HL = buffer end address (fill backwards) 
    halt                     ; Synchronization 
    in b,(c)                 ; B = total fill amount/count
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
; Read Keyboard / Transmit key (0 none)
command_TransmitKey:
;------------------------------------------------------
	JP GET_KEY            ; Returns key in A
GET_KEY_RET:		  ; (jump back - avoid stack usage)
    OUT ($1F), A    		  ; Game cart latches value (latch ic: 74HC574PW)
	halt				  ; Halt line - Arduino knows is safe to #EO and read new value from latch
	jp mainloop

;------------------------------------------------------
; Fill Memory 
; Input: DE = Count, HL = Start Addr, A = Fill Byte
;------------------------------------------------------
command_Fill:
    READ_PAIR_WITH_HALT d,e    ; DE = Count
    READ_PAIR_WITH_HALT h,l    ; HL = Start
    halt
    in a,(c)                   ; A = Fill byte
    
    ; Save original SP
    ld ix, 0
    add ix, sp 
         
    ; Handle Odd Byte (if any)
    bit 0, e                   
    jr z, .EvenStart
    ld (hl), a                 
    inc hl                     
    dec de                     
    
.EvenStart:
    ; SP needs to point to (Start + Count) 
    add hl, de                 ; HL = End of buffer
    ld sp, hl                  ; SP points to the end
    
    ; Prepare fill pattern in HL (PUSH will write 2 bytes at a time)
    ld h, a                    ; HL = Fill Word
    ld l, a                    
    
    ; Divide count by 2 for word-based fill
    srl d                      ; A = high byte of count/2
    rr e                       ; E = low byte of count/2
    
    ; Process 256-word blocks (if A > 0)
    ld a, d                    ; A = high byte
    or a
    jr z, .Remainder           ; Skip if no full blocks
    
.LoopBlocks:
    ld b, 0                    ; 256 iterations
.LoopInner:
    push hl
    djnz .LoopInner            ; Loops 256 times
    dec a
    jr nz, .LoopBlocks         ; Continue until high byte is 0
    
.Remainder:
    ; Process remaining words (E)
    ld a, e
    or a
    jr z, .Done
    ld b, a
.LoopRem:
    push hl
    djnz .LoopRem              ; Process remainder (1 to 255)

.Done:
    ld sp, ix                  ; Restore SP
    jp mainloop


;---------------------------------------------------
; Receive Data Transfer from Arduino (flashes border)
command_Transfer:  
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
; Receive Data Transfer from Arduino
command_Copy: 
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
; Receive Data Transfer from Arduino (32 bytes)
command_Copy32: 
;  ini: [HL]:=port[BC], HL:=HL+1, B:=B-1, Z is set if BC == 0
;------------------------------------------------------
	READ_PAIR_WITH_HALT h,l	; HL = Destination address
	REPT 32					; transfer x32 bytes [total:640 t-states]
	halt					; [4]
	ini						; [16] 
	ENDM
  JP mainloop 			; done - back for next transfer command

;--------------------------------------------------------
; Wait for the 50Hz maskable interrupt
command_VBL_Wait:  
;--------------------------------------------------------
    ; Enable Interrupt Mode 1 (IM 1) to use the default Spectrum interrupt handler at $0038.  
    ; This will trigger an interrupt at 50Hz (every 20ms) at address $0038 (default ISR).
    ; NOTE: Our ISR does not re-enable interrupts.
    IM 1    ; MASKABLE INTERRUPT (IM 1) - Vector: 0x0038  
    EI 
    ; We use HALT here to pause execution until the next interrupt occurs. Giving us a 20ms delay 
	; before continuing. The goal is to prevent an interrupt from interfering while
	; restoring the final state.  Since the Arduino gave this wait command it monitors halt and 
	; blocks until the Z80 resumes.
    HALT 			; enables the Z80 halt line
    jp mainloop 	; done - back for next transfer command

;-----------------------------------------------------
; Read/Write to the Stack Pointer
command_SetStack: 
;-----------------------------------------------------

	READ_PAIR_WITH_HALT h,l 
;  	READ_ACC_WITH_HALT       ; A = Save:0 or Restore:1

; 	or a    
;     jr nz, .restore 
	
; .store:
	; ld HL,0
	; add HL,SP
	; OUT (C), L
	; halt				
	; OUT (C), H    		  
	; halt
 	; jp mainloop    
	
; .restore 	
 	ld sp,hl
    halt  					; synchronization with Arduino
    jp mainloop          	
	

;------------------------------------------------------
; Command: Upload Code and Execute
; Input from Arduino: 
;   1 Byte  : Transfer size (B) 
;   2 Bytes : Target execution address (DE)
;   N Bytes : Payload (256 bytes)
;------------------------------------------------------
command_UploadAndExec:
    halt
    in b,(c)                    ; B = Transfer size (0 means 256 bytes)
    READ_PAIR_WITH_HALT d,e  
    
    ld h, d  
    ld l, e             
    
.uploadCodeLoop:
    halt                        ; Synch Arduino
    ini                         ; (HL) <- (C), B <- B-1, HL <- HL+1
    jp nz, .uploadCodeLoop 
    
    ex de, hl                   ; start address
    ; Payload code must use 'RET' to return back to mainloop
   	ld de, mainloop        		; return address
    push de

    jp (hl) ; Execute the payload!


;------------------------------------------------------
; Command: Poke Single Byte
; Input from Arduino: 
;   2 Bytes : Target RAM address (HL)
;   1 Byte  : Value to write (A)
;------------------------------------------------------
command_Poke:
    READ_PAIR_WITH_HALT h,l
    halt
    in a,(c)                    ; Read the value to POKE from Arduino
    ld (hl), a                  ; Perform the POKE: Write A to RAM address (HL)
    jp mainloop                 ; Return to the primary command loop


;-------------------------------------------------------------------------------------------
; Restore snapshot states & execute stored jump point
command_RestoreGameAndExecute: 
;-------------------------------------------------------------------------------------------

	;-------------------------------------------------------------------------------------------
	; Setup - Copy code to screen memory, as we'll be switching ROMs later.
	; After the ROM swap, we lose this sna ROM code in exchange for the stock ROM,
	; so the launch code must now run (JP <GAME_ADDRESS>) from screen RAM.
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
	; (Data from file, byte order is now in reverse)
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
	; Restore R (Refresh) register.
    ; NOTE: The R register cannot be perfectly restored because the act of 
    ; executing the restoration code advances the internal counter. 
    ; While the drift could be calculated by accounting for the CPU cycles 
    ; consumed until the game starts, this is unnecessary as most software 
    ; is unaffected. However, be aware that some games use R as a source 
    ; of semi-random numbers, which may result in minor behavioral variations.
    READ_ACC_WITH_HALT      ; Read saved R value from snapshot
    ld r, a                 ; Update R register
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
; This 3-byte section runs from screen memory after switching over to stock rom 
;-----------------------------------------------------------------------    
relocate:  			; I've found ways to reduce this - now at 3 bytes.
JumpInstruction:     
    jp 0000h 		; Jump to restored program (address patched above)
TempStack:        	; Temporary stack space (2 bytes)
relocateEnd:
;-----------------------------------------------------------------------	

;-----------------------------------------------------------------------
; CLEAR SCREEN ATTRIBUTES - 768 bytes of attribute memory.
;-----------------------------------------------------------------------
ClearScreenAttributes:
    LD IX, 0        
    ADD IX, SP     
        
    ld hl, 0    
    ld sp, SCREEN_ATTRIBUTES_START + SCREEN_ATTRIBUTES_SIZE  
    ld b, 6   ; 768 / 2 (push) / 6 = 64
        
clr_attr_loop:
    REPT 64
    push hl
    ENDM        
    djnz clr_attr_loop   

 	ld sp, ix     
    ret
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
    jp RestoreInterruptModeComplete  ; jp used to avoid stack
setIM0:
    IM 0
    jp RestoreInterruptModeComplete  ; jp used to avoid stack
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

; -------------------------------------------------------------------------
	IF $ > $04AA
	.ERROR "CODE OVERLAPS"
	ENDIF
; -------------------------------------------------------------------------

ORG $04AA  ; *** KEEP THIS 24 bytes long to match the stock ROM ***

; Exit mechanism – the Arduino signals a jump to L04AA in SNA ROM,
; initiating the path back to the game.

; When the this stock ROM swaps in, it overrides the idle loop, restores the 
; registers, and returns execution to the game.
; (Technically, from the game's perspective we are still inside the first NMI)

; Safeguards: The CPU may fetch in the middle of an instruction during the ROM
; swap. If the displacement byte comes from the new ROM (which contains NOPs),
; a "JR -2" becomes "JR 0" (effectively a two-cycle slow NOP).

; unused stock rom location - ROM SWAP LOCATION
L04AA: 
	;----------------------------------------------------------------
	JR .restoreInGameState
	;----------------------------------------------------------------
.restoreInGameStateCompletedWithEI:  ; jp return point - avoid stack usage
.ContinueGameIdle: jr .ContinueGameIdle   ; ROM sync, swapping back to stock
	NOP ; 2nd rom -> 'EI'
	NOP ; 2nd rom -> 'RET'
	;----------------------------------------------------------------
; stock rom location with 18 bytes of unused space - ROM SWAP LOCATION
L04B0:  
	;----------------------------------------------------------------
.restoreInGameStateCompletedWithDI: 
.ContinueGameIdle2: jr .ContinueGameIdle2   ; ROM sync, swapping back to stock
	NOP ; 2nd rom -> 'RET'
	;----------------------------------------------------------------
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop

;------------------------------------------------------------------------
; Ingame NMI hook - save game state
.IngameHook:

	; Our Arduino sync requires HALT + NMI. Since the pause menu uses recursive
	; NMIs, the CPU's IFF2 is overwritten (losing the game's interrupt state). 
	; We must keep IFF2 and when done manually restore IFF (call EI or DI) to resume the game.

	; Here we use OUTs to save registers and minimize stack usage.
	out  (0x1F), a			; A
	halt					; Nano triggers a NMI to release halt (this processs will use stack)
	ld   a,b				; B
	out  (0x1F), a
	halt
	ld   a,c				; C
	out  (0x1F), a
	halt
	; OK to grow/Strink stack in one go - we just need to avoid multiple pushes
	push af		
	pop bc
	ld   a,c				; F
	out  (0x1F), a
	halt

	ld   a,i                ; IFF2 state is copied to the parity flag
	; capture IFF2 
	jp   po, .WasOff        ; IFF2 OFF (interrupt enable flip-flop)
.WasOn:
	or  %10000000 ; 128                ; set bit7
	out  (0x1F), a          ;
	halt					; Sync with Arduino
	di                      ; Disable interrupts (Explicit intent, NMI does handle it)
	jr   .SendRegs
.WasOff:
	and  %01111111 ; 127                ; clear bit7
	out  (0x1F), a          ;
	halt					; Sync

.SendRegs:
	ld   a,d				; D
	out  (0x1F), a
	halt
	ld   a,e				; E
	out  (0x1F), a
	halt
	ld   a,h				; H
	out  (0x1F), a
	halt
	ld   a,l				; L
	out  (0x1F), a
	halt

	ld   a,ixh				; IXH
	out  (0x1F), a
	halt
	ld   a,ixl				; IXL
	out  (0x1F), a
	halt

	; We've already been forced to used the GAMES STACK above
	; using the luxury of a call/return here will not matter now!
	call command_MuteAY; // Prevent audio looping (machines with AY chip)

    jp mainloop         

;------------------------------------------------------------------------

.restoreInGameState: 
 
 	halt 
    in a, ($1f) 
	ld d,a			; D
	halt 
    in a, ($1f) 
	ld e,a			; E
	halt 
    in a, ($1f) 
	ld b,a			; B
	halt 
    in a, ($1f) 
	ld c,a			; C
	halt 
    in a, ($1f) 	
	ld h,a			; H
	halt 
    in a, ($1f) 
	ld l,a			; L

	halt 
    in a, ($1f) 	
	ld ixh,a			; IXH
	halt 
    in a, ($1f) 
	ld ixl,a			; IXL

	halt 
    in a, ($1f)   	
	bit 7,a			; IFF2

	jr  z,.disablePath
	;;;.ExitEnabled:

	; -------------------------------------------------------
	; Enable Maskable Interrupts Path
	halt 
    in a, ($1f)   ; F

	push af                  ; get F onto stack!
	inc sp                   ; Align SP to F
	pop af                   ; F now good (ignoring A holding junk)
	dec sp                   ; re-align SP 

	halt 
    in a, ($1f)   			 ; A - now we have 'AF'
	jp .restoreInGameStateCompletedWithEI
	; -------------------------------------------------------

.disablePath:
	; -------------------------------------------------------
	; Disable Maskable Interrupts Path
	halt 
    in a, ($1f)   ; F

	push af                  ; get F onto stack!
	inc sp                   ; Align SP to F
	pop af                   ; F now good (ignoring A holding junk)
	dec sp                   ; re-align SP 

	halt 
    in a, ($1f)   			 ; A - now we have 'AF'

	jp .restoreInGameStateCompletedWithDI
	; -------------------------------------------------------


;------------------------------------------------------------------------

;--------------------------------------------------------
; Mute the AY-3-8912 sound chip (128K Spectrum)
command_MuteAY:
;--------------------------------------------------------
	; register select - port $FFFD 
    LD   BC, $FFFD         
    LD   A, 7             ; Register 7 = mixer/enable
    OUT  (C), A           
	; register write - port $BFFD
    LD   B, $BF           
    LD   A, $FF           ; disable tone + noise on A/B/C
    OUT  (C), A            
    RET		; return OK as using MuteAY inside game menu's stack
;--------------------------------------------------------

	
; =======================================================================
; PORT 0x7FFD - ZX SPECTRUM 128K PAGING REGISTER
; =======================================================================
; Bits 0-2 : RAM Bank Select (0-7) mapped to Slot 3 (0xC000-0xFFFF)
; Bit  3   : Screen Select   (0 = RAM 5 Normal, 1 = RAM 7 Shadow)
; Bit  4   : ROM Select      (0 = ROM 0 128k Editor, 1 = ROM 1 48k BASIC)
; Bit  5   : Paging Lock     (1 = Lock machine into 48k mode until RESET)
; Bits 6-7 : Unused          (Should always be 0)
;
; System Variable copy: 0x5B5C <- we can ignore not using system
; =======================================================================

;--------------------------------------------------------
; Detect 128K memory paging (port $7FFD)
; Returns: A = 0   -> no paging (48K)
;          A = $10 -> paging works (128K)
;
; Port 0x7FFD: [7:Unused][6:Unused][5:Lock][4:ROM][3:Screen][2:RAM_B2][1:RAM_B1][0:RAM_B0]
;--------------------------------------------------------
Detect128K:
    LD   HL, $C000        ; Base of the paging window ($C000–$FFFF)
    LD   BC, $7FFD        ; BC = 128K paging port
    LD   A, $10           ; Bank 0, normal screen, ROM 1
    OUT  (C), A           ; Select bank 0 at $C000

    LD   E, (HL)          ; Save original byte from bank 0
    LD   (HL), $63        ; Write test value $63 into bank 0

    LD   A, %00010111     ; ROM:48k, Bank:7 (SCR:Norm, Lock:off)
    OUT  (C), A           ; Switch to bank 7 at $C000

    LD   A, $63
    CP   (HL)             ; Is $C000 (now bank 7) still $63?
    JR   Z, no_paging     ; If equal no paging found

    LD   D, $10           ; Result: paging works (128K)
    JR   cleanup

no_paging:
    LD   D, 0             ; Result: no paging (48K)

cleanup:
    LD   A, $10           ; Switch back to bank 0
    OUT  (C), A
    LD   (HL), E          ; Restore original byte in bank 0
    LD   A, D             ; Move result into A for return

    RET  ; return OK as using Detect128K inside game menu's stack



;-----------------------------------------------------------------------
; SaveZ80State – Save FULL Z80 state (entered via NMI)
; The original NMI return address stays on the game's stack 
;-----------------------------------------------------------------------
.SaveZ80State:

    out  ($1F),a	; A
    halt

    ld   a,i		; I
    out  ($1F),a
    halt

    ld   a,b		; B
    out  ($1F),a
    halt
    ld   a,c		; C
    out  ($1F),a
    halt

    push af
    pop  bc
    ld   a,c        ; F
    out  ($1F),a
    halt

    ld   a,d
    out  ($1F),a  	; D
    halt
    ld   a,e
    out  ($1F),a	; E
    halt
    ld   a,h
    out  ($1F),a	; H
    halt
    ld   a,l
    out  ($1F),a	; L
    halt
    ;---- Save SP ----
    ld   hl,0
    add  hl,sp
    ld   a,l
    out  ($1F),a
    halt
    ld   a,h
    out  ($1F),a
    halt
    ;---- Save IX ----
    ld   a,ixh
    out  ($1F),a
    halt
    ld   a,ixl
    out  ($1F),a
    halt
    ;---- Save IY ----
    ld   a,iyh
    out  ($1F),a
    halt
    ld   a,iyl
    out  ($1F),a
    halt

    ;---- Save alternate registers ----
    ex   af,af'
    exx
    out  ($1F),a	; A'
    halt
    push af
    pop  bc
    ld   a,c      	; F'
    out  ($1F),a
    halt
    ld   a,b
    out  ($1F),a  	; B'
    halt
    ld   a,c
    out  ($1F),a	; C'
    halt
    ld   a,d
    out  ($1F),a	; D'
    halt
    ld   a,e
    out  ($1F),a	; E'
    halt
    ld   a,h
    out  ($1F),a	; H'
    halt
    ld   a,l
    out  ($1F),a	; L'
    halt

    exx
    ex   af,af'

    ;---- Save I + IFF2 (bit 7 encodes IFF2) ----
    ld   a,i
    jp   po,.iffOff
.iffOn:
    or   %10000000
    out  ($1F),a
    halt
    jr   .saveR
.iffOff:
    and  %01111111
    out  ($1F),a
    halt

.saveR:
    ld   a,r		; R
    out  ($1F),a
    halt

    jp   mainloop


; -------------------------------------------------------------------------
	IF $ > $16D4
	.ERROR "CODE OVERLAPS"
	ENDIF
; -------------------------------------------------------------------------	

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
; The other stock ROM patches address L16D4 to execute "JP $3FFF".
; ROM byte ($3FFF) which is 0x3C (INC A). After INC A the CPU continues into 
; RAM at $4000 where our game launch code (JP <GAME_START_ADDR>) is.
; -------------------------------------------------------------------------

ORG $16D4       ; Must match stock ROM's 7 bytes here (this code will be swapped out)
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

	IF $ > $3d00
	.ERROR "CODE OVERLAPS"
	ENDIF

;------------------------------------------------------------------------
; If we see border lines, we probably hit this debug trap.
org $3d00	  		
debug_trap:  		
	SET_BORDER 2

    ld bc, 0xFFFF      ;  about 1/2 second delay
.pause_loop1: 
    dec bc          
    ld a, b       
    or c
    jr nz, .pause_loop1 

	SET_BORDER 0

    ld bc, 0xFFFF    
.pause_loop2:
    dec bc          
    ld a, b       
    or c
    jr nz, .pause_loop2 

	jr debug_trap
;------------------------------------------------------------------------

;------------------------------------------------------------------------
last:
DS  16384 - last	; leave rest of rom blank
;------------------------------------------------------------------------


;******************************************************
; todo 
; 1/ Shorten commands i.e. 'GO' to 'G' [DONE] 
; 2/ Add SCR loading/viewing "SC" [DONE]
; 3/ Add PSG Files - AY Player  "PS" 
; 4/ Add GIF Files - Gif Viewer/Player "GI"
; 5/ reorder incoming snapshot header [DONE]
; 6/ Options to change loading boarder (off, colour choice)
; 7/ Options to slow down loading, mimic tapes
; 8/ load binary TAP files (add command "T")
; 9/ load TZX files
; 10/ Load Z80 Files  [DONE]
; 11/ Load PNG file
; 12/ Support poke cheats
; 13/ Analyse games at load time, add lives + cheats
; 14/ Analyse music players extract, enable for AY on 48k.
; 15/ Compress data/unpack - with the aim of faster load times [DONE]
; 16/ Very simple run-length encoding [DONE]
; 17/ View game screens/scroll and pick one by picutre view (load just sna screen part)
; 18/ Move stack for menu stuff (currently it's using screen mem) [DONE]
; 19/ Relocate stack anywhere in memory [DONE]
; 20/ Launcher code can be relocated depending on game [MAYBE, HOWEVER IT'S NOW JUST 3BYTES]
; 21/ Command to upload code and execute 
; 22/ Joystick remapping including 2nd button support
;*****************************


; 27-byte SNA Header Example (Reordered by Arduino to aid Z80 register restoration)

;  Arduino's data transmit code loaded by z80
;   sendBytes(&header[SNA_I],  1 + 2 + 2 + 2 + 2);  // I,HL',DE',BC',AF'
;   sendBytes(&header[SNA_IY_LOW], 2 + 2 + 1 + 1);  // IY,IX,IFF2,R
;   sendBytes(&header[SNA_SP_LOW], 2);              // The rest aren't in SNA header sequence...
;   sendBytes(&header[SNA_HL_LOW], 2);
;   sendBytes(&header[SNA_IM_MODE], 1);
;   sendBytes(&header[SNA_BORDER_COLOUR], 1);
;   sendBytes(&header[SNA_DE_LOW], 2);
;   sendBytes(&header[SNA_BC_LOW], 2);
;   sendBytes(&header[SNA_AF_LOW], 2);

; [0 ] I            = 0x3F     	 [1st loaded]   
; [1 ] HL_          = 0x2758   	 [2nd]  
; [3 ] DE_          = 0xB462     [3rd]  
; [5 ] BC_          = 0x3F62     [4th]  
; [7 ] AF_          = 0x12A8     [5th]  
; [15] IY           = 0x5C3A  	 [6th]  
; [17] IX           = 0xDB59     [7th]  
; [19] IFF2         = 0x00       [8th]  
; [20] R            = 0x5F       [9th]  
; [23] SP           = 0x6188     [10th]  
; [9 ] HL           = 0xFC0B     [11th]  
; [25] IM           = 0x01       [12th]  
; [26] BorderColour = 0x00       [13th]  
; [11] DE           = 0x98B2     [14th]  
; [13] BC           = 0x0012     [15th]  
; [21] AF           = 0x4040  	 [16th is loaded last]

; ----------------------------------------------------------------------------------


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




;-----------------------------------------------------------------------
; SCREEN CLEAR - Initialize display memory
;-----------------------------------------------------------------------
; ClearScreen:
; 	SET_BORDER 0
;    	ld a, 0
;     ld hl, SCREEN_START
;     ld de, SCREEN_START+1
;     ld bc, 6144					; Screen bitmap
;     ld (hl), a
;     ldir
;     ld bc, 768-1				; Screen attributes
;     ld (hl), a
;     ldir
;     ret
;-----------------------------------------------------------------------	

;-----------------------------------------------------------------------
; CLEAR RAM 48KB - 273,725 T-states / 3,500,000 = 0.0782
; +0.08 seconds (will be a bit more due to contended memory)
;-----------------------------------------------------------------------
; ClearAllRam: 
;         LD IX, 0        
;         ADD IX, SP               
;         ld hl, 0                 
;         ld sp, 16384 + (48*1024) ; top of RAM (65536)
;         ld b, 0                  ; wraps so 256 times
; clr:    REPT 96
;         push hl           		 ; 11 * 96 * 256
;         ENDM
;         djnz clr          		 ; (256 * 13) - (13-8)
;         ld sp, ix                

; 		jp ClearAllRamRet 
;-----------------------------------------------------------------------	
