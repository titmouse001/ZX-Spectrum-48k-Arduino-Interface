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
	
	ld SP,0xFFFF  ; !!! Stack is only for menu use !!!
	; Note: At game loading time, SP is reasigned with 'command_Stack' from the Arduino.

	call sendFunctionList
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
; After NMI line signals for the speccy to resume, the Arduino waits for 7microseconds to allow time for the ISR to complete.
; NMI Z80 timings -  Push PC onto Stack: ~10 or 11 T-states
;                    RET Instruction:    10 T-states.  (24x0.2857=6.857microsecond)  <1/3.5MHz = 0.2857microseconds>
;				     
ORG $0066  ; location needs to use up 14 bytes
L0066:
	; WARNING: NMI disables maskable interrupts (IFF1=0, old IFF1 saved into IFF2).
    ; If we return with RET, IFF1 will stay cleared and maskable interrupts will never come back.

	RETN 	; MIRROR 4 bytes to PUSH
	NOP			
	NOP

    NOP		; MIRROR 2 bytes to ".idle: jr .idle" 
    NOP

	NOP
	NOP
	NOP
	NOP
	NOP

	; jp L04AA
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
	ld hl,command_TransmitKeyAs8Bits
	call transmit16bitValue
	;SET_BORDER 0
	
	ld hl,command_Fill
	call transmit16bitValue
	;SET_BORDER 1

	ld hl,command_SmallFill
	call transmit16bitValue
	;SET_BORDER 2

	ld hl,command_Transfer
	call transmit16bitValue
	;SET_BORDER 3

	ld hl,command_Copy
	call transmit16bitValue
	;SET_BORDER 4

	ld hl,command_Copy32
	call transmit16bitValue
	;SET_BORDER 5

	ld hl,command_Wait
	call transmit16bitValue
	;SET_BORDER 6

	ld hl,command_Stack
	call transmit16bitValue
	;SET_BORDER 7

	ld hl,command_Execute
	call transmit16bitValue
	;SET_BORDER 0

	ld hl,command_FillVariableEven
	call transmit16bitValue
	;SET_BORDER 1

	ld hl,command_FillVariableOdd
	call transmit16bitValue
	;SET_BORDER 2

	ret


;------------------------------------------------------
command_FillVariableEven:  ; Handles even byte fills
;------------------------------------------------------
     LD IX, 0        
     ADD IX, SP     

    READ_PAIR_WITH_HALT h,l    ; HL = buffer END address
    halt                       ;
    in b,(c)                   ; B = number of PUSH operations (count/2)
    READ_ACC_WITH_HALT         ; A = fill byte

    LD SP, HL                
    LD H, A                  
    LD L, A                  
FillEvenLoop:
    PUSH HL                   
    DJNZ FillEvenLoop        
	ld SP,IX    
    jp mainloop                

;------------------------------------------------------
command_FillVariableOdd:  ; Handles odd byte fills
;------------------------------------------------------
     LD IX, 0        
     ADD IX, SP 

	READ_PAIR_WITH_HALT h,l    ; HL = buffer END address
	halt                       ;
	in b,(c)                   ; B = number of PUSH operations ((count-1)/2)
	halt                       ;
	in d,(c)                   ; D = fill byte

	dec hl                     ; handle the odd byte
	ld (hl), d

	ld sp, hl                  ; Set SP to buffer end - 1
	ld h, d                   
	ld l, d                   
	
FillOddLoop:
	push hl                 
	djnz FillOddLoop        
	ld SP,IX   
	jp mainloop             


;------------------------------------------------------
transmit16bitValue:  
; Encodes/transmits using HALT pulses.
; HL holds 16-bit value to transmit
; Sends 0 bit as 1 HALT, 1 bit as 2 HALTs
    ld d,16          ; Bit counter
.bitloop:
    bit 7,h
    HALT
    jr z,.bitEndMarkerDelay  ; If bit is 0, skip second HALT
    HALT     	             ; Second HALT for bit '1'
.bitEndMarkerDelay:
    ld e,40   	 	         ; Delay
.delayMarkerLoop:
    dec e
    jr nz,.delayMarkerLoop
    add hl,hl        ; Shift HL left
    dec d
    jr nz,.bitloop
    ret

;------------------------------------------------------
 transmit8bitValue:
; A holds 8-bit value to transmit
;------------------------------------------------------
     ld d,8              ; Bit counter
 .bitlooptx:
     bit 7,a             ; Test MSB of A
     HALT
     jr z,.bitEndMarkerDelaytx  ; If 0, skip second HALT
     HALT                 ; Second HALT for '1'
 .bitEndMarkerDelaytx:
     ld e,40              ; Inter-bit delay
 .delayMarkerLooptx:
     dec e
     jr nz,.delayMarkerLooptx
     add a,a              ; Shift A left
     dec d
     jr nz,.bitlooptx
     ret

;------------------------------------------------------
command_TransmitKeyAs8Bits:
;------------------------------------------------------
    READ_ACC_WITH_HALT			 

    JP GET_KEY_PULSES          ; RESULT A = key pulses (2 to N)
DONE_KEY:

	call transmit8bitValue	; A=input
 	jp mainloop

;     ld d,8              ; Bit counter
; .bitloop8:
;     bit 7,a             ; Test MSB of A
;     HALT
;     jr z,.bitEndMarkerDelay8  ; If 0, skip second HALT
;     HALT                 ; Second HALT for '1'
; .bitEndMarkerDelay8:
;     ld e,40              ; Inter-bit delay
; .delayMarkerLoop8:
;     dec e
;     jr nz,.delayMarkerLoop8
;     add a,a              ; Shift A left
;     dec d
;     jr nz,.bitloop8

;     ld c,$1F
;     jp mainloop

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

;------------------------------------------------------
command_SmallFill:  ; "f" - Small Fill 
;------------------------------------------------------
	halt 
	in b,(c)     			  ; Fill count (1 byte)
	READ_PAIR_WITH_HALT h,l   ; HL = Destination address
	halt                      ; Wait for Arduino
	in a,(c)                  ; Read fill value into A
smallfillLoop:
	ld (hl),a                 ; Store fill byte
	inc hl
	djnz smallfillLoop             ; Loop until B == 0
	JP mainloop

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

;	in a,($1f)   				; Read data byte [11]
;	ld (hl),a					; Store byte	 [7]
;  	inc hl						;				 [6]
;	djnz readDataLoop			;			 	 [13]

	ld a,R
	AND LOADING_COLOUR_MASK  	; Use data for border flash
	out ($fe), a

	jp mainloop


;	000	Black		
;	001	Blue		
;	010	Red		
;	011	Magenta		
;	100	Green		
;	101	Cyan		
;	110	Yellow		
;	111	White		


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
    jp mainloop          	; Done - jump back for next command

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
;;;;	push de
;;;;	pop IY					  ; restore IY
	LD IYL, e  
	LD IYH, d  

	READ_PAIR_WITH_HALT e,d;  ; read IX
;;;;	push de
;;;;	pop IX					  ; restore IX
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

	JP SCREEN_START		 ; jump to relocated code in screen memory
	
;-----------------------------------------------------------------------	



;-----------------------------------------------------------------------    
; RELOCATE SECTION - ROM swap code
; 
; NOTE: Games need original Spectrum ROM, but we can't execute code during ROM swap
; so we copy this small routine to screen memory before swapping ROMs.
; 
; This 4-byte section runs from screen memory (safe to overwrite) and switches to 
; the second half of our ROM (original 48K ROM copy). Switching back to internal 
; ROM caused issues, so using ROM's second half works perfectly.
;-----------------------------------------------------------------------    
relocate:
    HALT       					; Signal Arduino to switch ROM
JumpInstruction:     
    jp 0000h 					; Jump to restored program (address patched above)
TempStack:        				; Temporary stack space (2 bytes)
relocateEnd:

; IDEA :  COULD REPLACE LAST TWO BYTES IN STOCK ROM TO HALT (0x76) AND JP (0xC3)
;         JUMPING TO 0x3ffe WOULD ROLL FROM ROM INTO SCREEN MEMORY WHICH WOULD 
;         ONLY NEED TO SPOIL 2 SCREEN BYTES (FOR THE JMP ADDR)  !!!
;  ... hmmm, halt would need to be in SNA ROM and the final JMP IN STOCK to roll into RAM.

; SECTION NOTES: Could move this HALT in ROM.  However the Arduino would need to 
;				 acount for it. Since the Halt signals the ROM swap and the 
;				 "JP SCREEN_START" has yet to happen. So on seeing the halt the 
;				 Arduino would need to delay for around +10t states to allow 
;				 the JP to complete and the new rom swap in while in screen memory.
;				 The window size is small, but possible, even more so if the 
;				 ROM is not used soon after by the game.
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
; CLEAR RAM 48KB 
;-----------------------------------------------------------------------
ClearAllRam:  ; 273,725 t-states ÷ 3,500,000 = 0.0782 seconds ?!?
        LD IX, 0        
        ADD IX, SP               
        ld hl, 0                 
        ld sp, 16384 + (48*1024) ; top of RAM (65536)
        ld b, 0                  ; wraps so 256 times
clr:    REPT 96
        push hl           
        ENDM
        djnz clr                 
        ld sp, ix                

		jp ClearAllRamRet 

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


;------------------------------------------------------------------------
; KEYBOARD SCANNING - Converts keypress to pulse count for "HALT-FUDGE" transmission
;------------------------------------------------------------------------
GET_KEY_PULSES:    
    LD      D, $FE          	; First keyboard row
    LD      C, 2     		    ; Start pulse count (1=no key, 2+=key index)
rawcheckKeyLoop: 	
    LD      A, D
    IN      A, ($FE)      		; Read keyboard row
    LD      B, 5            	; 5 keys per row
    LD      E, 1            	; Bit mask
rawcheckBits:
    RRCA                    	; Test bit in carry
    JR      NC, rawKeyFound    	; Key pressed (bit=0)
    INC     C              	 	; Next pulse count
    SLA     E           	    ; Next bit mask
    DJNZ    rawcheckBits
    RLC     D               	; Next keyboard row
    JR      C, rawcheckKeyLoop
    ; No key found
	ld 		c,1
rawKeyFound:
    LD      A, C            	; Return pulse count
    JP      DONE_KEY
;------------------------------------------------------------------------

ORG $04AA  ; *** KEEP THIS 24 Bytes long to match stock ***
; This routine is technically still running from inside the NMI.
; However game code has been redirected to run from SNA rom.
L04AA:

; The CPU is fetching instructions from the ROM we are about to swap, so there’s a good chance
; it might grab bytes mid-instruction. If that happens, the displacement byte could come from the “wrong” ROM.
; However, since the new ROM has NOPs at that location, a JR -2 could become JR 0,
; which just jumps to the next instruction (i.e. basically a slow NOP and it does nothing but waste 2 cycles).

	NOP
	NOP
	NOP
	NOP  ;0x04AD - Arduino will exit mainloop here.
	NOP
	NOP

    ld  HL,16384
    bit 7,(HL)            	 ; check EI flag
    jr  z,.ContinueGameIdle  ; if clear, don’t re-enable
    ei    					 ; restore EI

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

ORG $0500  

.IngameHook:

		ld HL,16384  
        ld   a,i          	; set flags, P/V = IFF2
        jp   po,.WasOff    	; parity odd -> IFF2=0

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

	jp mainloop

.SkipEnable:  jp   L04AA  ; path back to stock ROM

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


; **** SCRATCH PAD ********



; ___UNUSED___GET_KEY:   
;             LD      HL, KEY_TABLE    
;             LD      D, $FE          ; 1st Port to read
; checkKeyLoop: 	
; 			LD      A, D            
; 	        IN      A, ($FE)        ; Read key state
;             LD      E, $01          ; Bitmask for key check
;             LD      B, $05          ; 5 bits (keys) in each row
; CheckKey:   RRCA                    ; 
;             JR      NC, KeyFound 	; key down (bit checked is zero)
;             INC     HL              ; next lookup
; 			SLA     E         		; move test mask
;             DJNZ    checkKeyLoop        
;             RLC     D               ; next port row to read ($FD,$FB...)
;             JR      C, checkKeyLoop
; KeyFound: 	LD      A, (HL)         ; get char value from table
; 			JP DONE_KEY

; KEY_TABLE:	; Key row mapping  		; $6649
; 			defb $01,"ZXCV"		 	; $01=shift	
; 			defb "ASDFG"
; 			defb "QWERT"
; 			defb "12345"
; 			defb "09876"
; 			defb "POIUY"  
; 			defb $0D,"LKJH"     	; $0D=enter
; 			defb $20,$02,"MNB"  	; $20=space, $02=sym shft
; 			defb $0					; tables end marker
