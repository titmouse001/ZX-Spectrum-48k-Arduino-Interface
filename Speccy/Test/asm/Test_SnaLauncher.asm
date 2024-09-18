; THIS IS A COPY OF THE MAIN CODE SETUP AS A TEST CASE
; THE INCOMING DATA FROM THE ARDUINO IS HARDCODED FROM "DAN DARE 3"
; Some transfer mode, copy/fill are not part of this test can have been removed

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

	ld bc,$FEB1
	ld ($618A) , bc
	ld bc,$4040
	ld ($6188) , bc
	ld bc,$FC0B
	ld ($6186) , bc

;	jp ClearScreen

	jp mainloop
 
;******************************************************************************
; MASKABLE INTERRUPT - (EI HAS TO BE CALLED BEFORE RETN - SO PLAYING IT SAFE)
ORG $0038
L0038:   
		; Leaving out 'EI' to ensure minimal delay, and is simple for us to EI outside
		RET
;******************************************************************************
; NON MASKABLE INTERRUPT - used to release z80 HALT state
ORG $0066
L0066:
		RETN
;******************************************************************************

mainloop:

;******************************************************************************
; For each incoming data transfer, the header (first 2 bytes) are 'action' indicators.
;  # Transfer data = "GO"   (normally this is screen/program code all-in-one )
;  # Execute code = "EX" 	(after reading and restoring z80 registers)
;
check_initial:   		; Main loop for checking two letter command

;;;;;;;;;;	READ_ACC_WITH_HALT
	ld a,'E' ;TEST CODE

    cp 'E'            
    jr z, command_EX ; execute program (includes restoring 27 byte header)
  
    jr check_initial 

;------------------------------------------------------
command_EX:  ; "E" EXECUTE CODE / RESTORE & LAUNCH 
; Restore snapshot states & execute stored jump point from the stack
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

; DanDare3 header ... looking into as main menu crashing back into basic!
; 3F,     	I
; 58 27, 	HL'
; 62 B4, 	DE'
; 62 3F, 	BC'
; A8 12, 	AF'
; 0B FC, 	HL
; B2 98, 	DE
; 12 00, 	BC
; 3A 5C,	IY
; 59 DB,	IX
; 00,		IFF
; 5F,		R
; 40 40,	AF
; 88 61,	SP
; 01,		IM
; 00		border colour

; RE-SENT PAIRS TO HELP WITH RESTORE ORDER
; B2 98, 	DE
; 12 00, 	BC
; 40 40,	AF

;------------------------------------------------------

	;-------------------------------------------------------------------------------------------
 	; Setup - put copy of ROM routine 'relocate' (final launch code) into Screen memory	
	LD HL,relocate
	LD DE,SCREEN_START 
	LD BC, relocateEnd - relocate
	LDIR  ; We are preparing this screen memory now while registers are still available.
	;-------------------------------------------------------------------------------------------
	
	;-------------------------------------------------------------------------------------------
 	; Restore 'I'
	ld a, $3f  ; Test line , replacing: READ_ACC_WITH_HALT
	ld	i,a
	;-------------------------------------------------------------------------------------------
	
	;-------------------------------------------------------------------------------------------
	; Restore HL',DE',BC',AF'
	ld c,$1f  				 ; setup for pair read
	ld hl, $2758;   ; Test line , replacing: EAD_PAIR_WITH_HALT L,H  ; store HL'
	ld de, $B462    ; Test line , replacing: READ_PAIR_WITH_HALT e,d  ; store DE'

;;; mistake here!!!!  CAN'T USE READ_PAIR_WITH_HALT WITH BC
;;;	ld de, $3F62	; Test line , replacing: READ_PAIR_WITH_HALT e,d  ; Get BC' (temporarily in DE)
;;;    push de                  ; Save DE (holds BC')
;;;    pop bc                   ; BC' restored

	LD A, $62   ;;;READ_ACC_WITH_HALT	; C     *** TEST
	ld c,a
	LD A, $3F   ;;;READ_ACC_WITH_HALT	; B    *** TEST
	ld b,a


	exx						 ; Alternates registers restored
	; registers are free again, just AF' left to restore
	ld c,$1f  				 ; setup for pair read
	ld de,$12A8    ; Test line , replacing: READ_PAIR_WITH_HALT e,d  ; spare to read AF' - save flags last
	push de					 ; store AF'
	pop af					 ; Restore AF'
	ex	af,af'				 ; Alternate AF' restored
	;-------------------------------------------------------------------------------------------
	
	;-------------------------------------------------------------------------------------------
	ld c,$1f  				  ; setup for pair read
	; Restore HL,DE,BC,IY,IX	
	ld hl,$FC0B  			  ;Test line , replacing:READ_PAIR_WITH_HALT l,h   ; restore HL
	;;;;READ_PAIR_WITH_HALT e,d;  ;TEST can skip this one,  TO VOID -  read DE		  			 
	;;;;READ_PAIR_WITH_HALT e,d;  ;TEST can skip this one,  TO VOID -  read BC	  
	ld DE, $5C3A 			  ;Test line , replacing: READ_PAIR_WITH_HALT e,d;  ; read IY
	push de
	pop IY					  ; restore IY
	ld DE, $DB59			  ;Test line , replacing: READ_PAIR_WITH_HALT e,d;  ; read IX
	push de
	pop IX					  ; restore IX
	;------------------------------------------------------------------------

	;------------------------------------------------------------------------
	; read IFF
	ld a,0  ;Test line , replacing: READ_ACC_WITH_HALT		 
	jp RestoreIFFState
RestoreIFFStateComplete:
	;------------------------------------------------------------------------

	;------------------------------------------------------------------------
	; R regester
	ld a, $5F-$2c		;Test line , replacing: READ_ACC_WITH_HALT ;  R REG ... TODO currently goes to the void
	ld r,a

	;------------------------------------------------------------------------

	;------------------------------------------------------------------------
	; SKIP AF here ... will read again later
	; TEST can skip this one ; READ_PAIR_WITH_HALT e,d;  ; TO VOID - using spare to restore AF
	;------------------------------------------------------------------------

	;------------------------------------------------------------------------
	; Restore Program's Stack Pointer
	ld de, $6188			;Test line , replacing: READ_PAIR_WITH_HALT e,d;  ; get Stack
	ld (SCREEN_START+(spare-relocate)),de 	
	ld SP,(SCREEN_START+(spare-relocate))
	
	pop de

	; Store programs start address, uses self-modifying code into screen memory
	ld (SCREEN_START + (JumpInstruction - relocate) +1),de  ; set jump to address
	dec sp
	dec sp
	;------------------------------------------------------------------------

	;------------------------------------------------------------------------
	ld a,1    ;Test line , replacing:  READ_ACC_WITH_HALT  ; gets value of IM into reg-A
	JP RestoreInterruptMode
RestoreInterruptModeComplete:
	;------------------------------------------------------------------------

	;------------------------------------------------------------------------
	ld a,6 ;  YELLOW FOR TEST ...Test line , replacing:  READ_ACC_WITH_HALT   ; Border Colour
	AND %00000111		 
	out ($fe),a			 ; set border
	;------------------------------------------------------------------------

	; Arduino will now resend DE,BC & AF we IGNORED earlier.

	;------------------------------------------------------------------------
	; Restore DE & BC

	ld de,$98B2 		;Test line , replacing: READ_PAIR_WITH_HALT e, d   ; Restore DE directly

;;;; MISTAKE HERE;;;;    cant pair read for BC
;; 	ld de,$0012    		;Test line , replacing: READ_PAIR_WITH_HALT e, d   ; Get BC (temporarily in DE)
;;    push de                    ; Save DE (holds BC)
;;    pop bc                     ; BC restored

;; TEST
;;; 	READ_ACC_WITH_HALT	; C
	ld c,$12
;;;;	READ_ACC_WITH_HALT	; B
	ld b,$00


	;------------------------------------------------------------------------

	;-----------------------------------------
	; Clear spares to reduce screen impact
	ld a,0
	ld (SCREEN_START+((spare+1)-relocate)),a
	ld (SCREEN_START+(spare-relocate)),a
	;-----------------------------------------

	;------------------------------------------------------------------------
	; Restore AF - but we can ONLY use register A, hence the odd looking code!

	ld a,$41			; F
	push af
	inc sp
	pop af
	dec sp
	ld a,$40			; A
	
	; Note: We need to continue using the stack because the 'HALT' instruction relies on it.
	; Fortunately, the program’s actual stack has exactly 2 bytes of spare space.
	; This conveniently holds the snapshot's return address, which we are now using 
	; as a temporary 1-level deep stack, since we have already saved it in the 
	; "JumpInstruction: jp 0000h " self-modifying code.

	;------------------------------------------------------------------------

	;------------------------------------------------------------------------
	; Arduio just waits for the halt line - masable used this time.
	; Note: During HALT, the interrupt service routine will use the stack.
    ;!!! At this point, we are using the real restored program's stack space !!!
;;;NOT FOR TEST;	IM 1
;;;NOT FOR TEST;	EI

	; The idea behind using HALT here is to give a 20ns gap, we really don't
	; want the speccy firing it's maskalbe interrupt while we are restoring the 
	; final step.
;;;NOT FOR TEST;	HALT 				 ; Maskable 50FPS Interupt routine ($0038) forgoes 'EI',


	JP SCREEN_START		 ; jump to relocated code in screen memory
	;------------------------------------------------------------------------
; END - "command_EX:"

;-----------------------------------------------------------------------	
; SCREEN MEMORY USAGE - "76 ED 46 00 33 33 C3 00 00 FF FF"
; This means 11 bytes of screen memory will be corrupted.

; This gets placed in screen memory (called via "JP SCREEN_START")
;-----------------------------------------------------------------------	
; FINAL STEP - Start-up the restored program 
relocate:

;;;NOT FOR TEST 	HALT    ; 1/ Arduino detects this HALT and signals the 'NMI' line to resume z80.
			; 2/ Arduino will wait 10+14 t-states (as NMI will cause a 'PUSH' & 'RETN')
		    ; 3/ Arduino switches from loader rom to original rom kept in the 2nd half.

      		; At this point, the original ROM should be active again!

    inc sp  ; Restore the stack to its original state, we used the 'RETN'
	inc sp  ; 			   location was used as a temporary 1-deep stack.

IM_LABLE: 	
	IM 0    ; Interrupt Mode: self-modifying code - 'IM <n>'
NOP_LABLE:  
	NOP    	; Placeholder for self-modifying code - 'EI' or stays as 'NOP'

;;;;;;;    inc sp  ; Restore the stack to its original state, we used the 'RETN'
;;;;;;;	inc sp  ; 			   location was used as a temporary 1-deep stack.

	; note: The loaded Snapshot file provides RETN/jump, but we have already
	; saved it's value and stored it just below.
JumpInstruction: 	; Self-modifying code - JP location will be modified 	
    jp 0000h        ; !!! THIS IS IT - WE ARE ABOUT TO JUMP TO THE RESTORE PROGRAM !!!

spare:				; Used to keep real stacks starting pointer
	db $FF,$FF		; Double purpose, see top & bottom comments
TempStack:			; Used at startup until real stack is restored

relocateEnd:
;-----------------------------------------------------------------------	

;-----------------------------------------------------------------------	
ClearScreen:
	SET_BORDER 0
   	ld a, 0              
    ld hl, 16384       
    ld de, 16385        
    ld bc, 4;   REDUCE FORE TEST  ; 6144       
    ld (hl), a           ;setup
    ldir                
    ld bc, 4;  REDUCE FOR TEST  ; 768-1
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