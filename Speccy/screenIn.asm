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
; Kempston  : ---- ---- 0001 1111  0x1F
;

org 0x8000

start:      
	DI
	
;hangaround:	
;	in a,($1f) 
;	LD B, 10101010b 
;	CP B         
;	JP Z, mainloop     
;	JR hangaround

	
mainloop:     
	ld hl,0x4000
	ld de,6912  ; 6144
	
screenloop:
	
	; ld BC,0xFF1F
	; in a,(c)
	
	; ($1f) place on address buss bottom half (a0 to a7)
	; Accumulator top half (a8 to a15)  - currently I don't care and acc is left with whatever value
	in a,($1f) 
;	CPL	
	ld (hl),a
    inc hl
    dec de
    ld a,d
    or e
    jp nz,screenloop
	
	
	 LD BC,$ffff  
    CALL DELAY
   CALL DELAY
    CALL DELAY
    CALL DELAY
    CALL DELAY


	
;	ld hl,0x4000
;	ld de,6912 
;screenloop2:
;	ld BC,0xFF1F
;	in a,(c)
;	ld (hl),a
;	inc hl
;	dec de
;	ld a,d
;	or e
;	jp nz,screenloop2
;   LD BC,$ffff  
;   CALL DELAY

    jr mainloop

	
 DELAY:
    NOP
	DEC BC
	LD A,B
	OR C
	RET Z
	JR DELAY
		
END start