org 0x8040

SCREEN_END			EQU $5AFF
FUTURE_STACK_BASE  	EQU $5AFF-2
NMI_SETTING			EQU $5CB0

MACRO READ_ACC_WITH_HALT 
    in a, ($1f)  
    halt     
ENDM

MACRO READ_PAIR_WITH_HALT  ,reg1,reg2
    in reg1,(c)
    halt
    in reg2,(c)
    halt
ENDM

MACRO SET_BORDER ,colour  ; DEBUG
	;0 Black, 1 Blue, 2 Red, 3 Magenta, 4 Green, 5 Cyan, 6 Yellow, 7 White		
	ld a,colour
	AND %00000111
	out ($fe), a
ENDM

;**************************************************************************************************

start: 
	DI
   	ld a,$ff     
    LD (NMI_SETTING),a
mainloop: 

;**************************************************************************************************
; At start of each chunck transfer, the first 2 bytes are checked for 'action' indicators.
; ACTIONS ARE:-
;  - Transfer data = "GO"
;  - Execute code = "EX"
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

;**************************************************************************************************

command_EX:  ; "Execute snapshot Stage"
	; Stack goes down from SCREEN-END. The code only needs 2 bytes or one stack depth.
	; However, the NMI call in the stock ROM will also add 2 more pushes, so 6 bytes in total are needed.
	ld SP,SCREEN_END 	; using screen attribute area works are a bonus debugging tool!

SET_BORDER 2 ; DEBUG

	; Restore HL',DE',AF',BC'
	ld c,$1f  			; setup for use with the IN command, i.e. "IN <REG>,(c)"
	READ_PAIR_WITH_HALT h,l
	READ_PAIR_WITH_HALT d,e
	READ_PAIR_WITH_HALT b,c
	push BC
	pop AF	
	ld c,$1f
	READ_PAIR_WITH_HALT b,c
	ex	af,af'	; Alternate AF' restored
	exx			; Alternates registers restored

	; Restore HL,DE,IY,IX
	ld c,$1f
	READ_PAIR_WITH_HALT h,l
	READ_PAIR_WITH_HALT d,e
	READ_PAIR_WITH_HALT b,c
	push bc
	pop IY
	ld c,$1f
	READ_PAIR_WITH_HALT b,c
	push bc
	pop IX

	; Restore interrupt mode; IM0, IM1 or IM2
	READ_ACC_WITH_HALT
	or	a
	jr	nz,not_IM0
	im	0
	jr	IM_done
not_IM0:	
	dec	a
	jr	nz,not_IM1
	im	1
	jr	IM_done
not_IM1: 
	im	2
IM_done: 

 	; Restore 'I'
	READ_ACC_WITH_HALT
	ld	i,a

	; Restore interrupt enable flip-flop (IFF) 
	READ_ACC_WITH_HALT
	AND	%00000100
	jr	z,skipEI
	EI	 ; restore Interrupt (this code uses DI - so if needed we only need to enable)
skipEI:

	; R REG ... TODO  
	READ_ACC_WITH_HALT ; currently goes to the void

	; Restore AF
	ld c,$1f
	READ_PAIR_WITH_HALT b,c
	push BC
	pop AF

	; Restore SP
	ld c,$1f
	READ_PAIR_WITH_HALT b,c
	push BC
	pop BC
	ld SP,(FUTURE_STACK_BASE)

	; Restore BC register pair
	ld c,$1f
	READ_PAIR_WITH_HALT b,c

	; NOTE: Reading the border colour is ignored for now - header byte 26 is not sent by the Arduino

	RETN  ; PC ready on stack! return to start snapshot code

;**************************************************************************************************

command_GO:

	; The actual maximum transfer size is smaller than 1 byte; it's really 250,
	; as 'amount', 'destination', and "GO" are included in each transfer chunk.
	; Transfers here are sequential, but any destination location can be targeted.
	READ_ACC_WITH_HALT ; amount to transfer, 1byte
    ld d, a       

	; dest (read 2 bytes) - Read the high byte
	READ_ACC_WITH_HALT
    ld H, a      
	READ_ACC_WITH_HALT
    ld L, a           

screenloop:
	in a,($1f)   ; Read a byte from the I/O port
	ld (hl),a	 ; write to screen
    inc hl
SET_BORDER a ; DEBUG
	halt 		 ; do this one after write!

    dec d 	
    jp nz,screenloop

    jp mainloop

;**************************************************************************************************	

END start




; http://www.robeesworld.com/blog/33/loading_zx_spectrum_snapshots_off_microdrives_part_one
; https://www.seasip.info/ZX/spectrum.html
; https://worldofspectrum.org/faq/reference/formats.htm
; https://github.com/oxidaan/zqloader/blob/master/z80snapshot_loader.cpp
; http://cd.textfiles.com/230/EMULATOR/SINCLAIR/JPP/JPP.TXT

; Use end connectors to fudge/repair speccy keyboard
; GOOGLE THIS: "4x3/4x5/1x6/1x4 Keys Matrix Keyboard Array Membrane Switch Keypad Keyboard UK"