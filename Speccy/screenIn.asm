;https://www.retronator.com/png-to-scr

; OPTIMISED CODE TIPS HERE:-
; https://www.smspower.org/Development/Z80ProgrammingTechniques

;https://github.com/konkotgit/ZX-external-ROM/blob/master/docs/zx_ext_eprom_512.pdf

; Contended Screen Ram
; On 48K Spectrums, the block of RAM between &4000 and &7FFF is contented, 
; that is access to the RAM is shared between the processor and the ULA. 

; ====================
; PORTS USED BY SYSTEM
; ====================
; ULA 		: ---- ---- ---- ---0  
;			  All even I/O  uses the ULA, officialy 0xFE is used.
; Keyboard 	: 0x7FFE to 0xFEFE
; Kempston  : ---- ---- 0001 1111  = 0x1F
;

org 0x8000
start:      
	DI

mainloop: 
	LD BC,$fff    ; give nano time to cache 
    CALL DELAY

	check_loop:  ; x2 bytes, header "GO"
	in a,($1f) 
	CP 'G'
	JR NZ, check_loop
	in a,($1f) 
	CP 'O'  
	JR NZ, check_loop
	; if we get here then we have found "GO" header 

	LD BC,$ff    ; give nano time after header, not really needed
    CALL DELAY

	ld hl,0x4000
	ld de,6912  ; data remaining is 6144 (bitmap) + 768 (Colour attributes) 

screenloop:

	; ($1f) place on address buss bottom half (a0 to a7)
	; Accumulator top half (a8 to a15) - currently I don't care
	in a,($1f) 
	ld (hl),a
    inc hl
    dec de


	LD BC,$ff  
    CALL DELAY

;	REPT 64  ; delay
;	NOP
;	ENDM 

	ld a,d
    or e
    jp nz,screenloop

    jr mainloop
		
 DELAY:
    NOP
	DEC BC
	LD A,B
	OR C
	RET Z
	JR DELAY
		
END start

;ld BC,14
;loop14:
;
;
;   ld a,B
;   or C
;  jp nz,loop14
