org 0x8040
;org 0x0000

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

start:   ; MAIN STARTING POINT
	DI
   	ld a,$ff     
    LD (NMI_SETTING),a   ; /NMI will now just return, we can now use NMI to un-halt z80
mainloop: 
	ld c,$1f  	; Initial setup for use with the IN command, i.e. "IN <REG>,(c)" 

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

command_EX:  ; SECOND STAGE - Restore snapshot states & execute stored jump point from the stack

	; Stack goes down from SCREEN-END. The code only needs 2 bytes or one stack depth.
	; However, the NMI call in the stock ROM will also add 2 more pushes, so 6 bytes in total are needed.
	ld SP,SCREEN_END 	; using screen attribute area works are a bonus debugging tool!

SET_BORDER 2 ; DEBUG

	; Restore HL',DE',AF',BC'
	READ_PAIR_WITH_HALT h,l
	READ_PAIR_WITH_HALT d,e  ; USED DE NOT BC FOR SCRATCH, SAVES RENEWING c
	READ_PAIR_WITH_HALT b,c
	push BC
	pop AF	
	ld c,$1f  ; just trashed BC - reload C as needed for more READ_PAIR IN's
	READ_PAIR_WITH_HALT b,c
	ex	af,af'	; Alternate AF' restored
	exx			; Alternates registers restored

	; Restore HL,DE,IY,IX
	ld c,$1f  ; EXX used - renew c
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
	im  2
	or  a
	jr  nz, not_IM0      ; If A is not 0, continue to check for IM 1
	im  0
	jr  IM_done_inline
not_IM0:
	dec a
	jr  nz, IM_done_inline     ; If A is not 1, IM 2 is already set
	im  1
IM_done_inline:

 	; Restore 'I'
	READ_ACC_WITH_HALT
	ld	i,a

	; Restore interrupt enable flip-flop (IFF) 
	READ_ACC_WITH_HALT
	AND	%00000100
	jr	z,skip_EI
	EI	 ; restore Interrupt (this code uses DI - so if needed we only need to enable)
skip_EI:

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

	RETN  ; PC is now ready on stack! Returning via 'RETN' will now start the snapshot code.

;**************************************************************************************************

command_GO:  ; FIRST STAGE - TRANSFER DATA
	
	; The actual maximum transfer size is smaller than 1 byte; it's really 250,
	; as 'amount', 'destination', and "GO" are included in each transfer chunk.
	; Transfers here are sequential, but any destination location can be targeted.
;	READ_ACC_WITH_HALT ; amount to transfer (1 byte)
;    ld b, a       

	in b,(c)  ; C was set earler on, infact it will remain ok until the second stage
	HALT

	; Read destination address in HL (2 bytes)

	READ_PAIR_WITH_HALT h,l

;	READ_ACC_WITH_HALT
 ;  ld H, a      
;	READ_ACC_WITH_HALT
 ;  ld L, a           

screenloop:
	; liking the boarder effect, so going with 'in' not 'ini' as need value in Acc
	in a,($1f)   ; Read a byte from the I/O port
	ld (hl),a	 ; write to screen
    inc hl
SET_BORDER a ; DEBUG
	halt 		 ; do this one after write!
    djnz screenloop



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