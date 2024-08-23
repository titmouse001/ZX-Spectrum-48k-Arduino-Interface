ORG $8000

;MACRO SET_BORDER ,colour   ; pasmo compiled used
;	;0 Black, 1 Blue, 2 Red, 3 Magenta, 4 Green, 5 Cyan, 6 Yellow, 7 White		
;	ld a,colour
;	AND %00000111
;	out ($fe), a
;ENDM

ClearScreen:

   ;SET_BORDER 0
	ld a,0
	AND %00000111
	out ($fe), a
   	ld a, 0
    ld hl, 16384       
    ld de, 16385        
    ld bc, 6144       
    ld (hl), a           ;setup
    ldir                
    ld bc, 768-1
    ld (hl), a  
    ldir        
    jp mainloop  		 ; avoiding stack based calls

mainloop :
	nop