


org 0x8000    ; COMPILE TO RAM
;org 0x0000   ; COMPILE TO ROM (also add back in the DS line at end)

start:

	DI


; stack doing a forward copy:

; 	; screen, everything is in 6,912 bytes total 
; 	ld SP,data 
; 	ld HL,16384  
; 	ld C,16   ; 6912/2 , 16 = 3456/216 

; lp_outerA:
; 	ld B,216
; lp_innerA:
; 	pop de   ; image data -> reg
; 	ld (hl),e
; 	inc HL
; 	ld (hl),d
; 	inc HL
;   djnz lp_innerA
; 	dec c
; 	jr	nz,lp_outerA



; Stack copy in reverse order, screen attrubutes first
; Pushes data to screen:-

	; screen, everything is in 6,912 bytes total 
	ld HL,data + 6912 -1
	ld SP,16384 + 6912
	ld C,16   ; 6912/2 , 16 = 3456/216 

lp_outerA:
	ld B,216
lp_innerA:

	ld d,(hl)
	dec HL
	ld e,(hl)
	dec HL
	push de			; screen <- image data
    djnz lp_innerA
	dec c
  	jr	nz,lp_outerA

jr start

data:   


INCBIN "Judge-Dredd-Featured.png.scr"


last:
;DS  16384 - last  ; blank rest for a ROM compile


END start
