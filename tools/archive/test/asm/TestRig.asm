org 0x8000    

start:
	DI

main:
	ld b,191
	ld c,255    
lp:
	push bc
    ; call plot_pixel    

	ld a,c
    ld (x),a       
	ld a,b
	ld (y),a       
    call plot        


	pop bc
	djnz lp
	ld b,191
	dec c
	jp lp





	jp main

; Pixel Plot Routine for ZX Spectrum
; Input: B = Y (0-191), C = X (0-255)
; Clobbers: AF, BC, DE, HL

plot_pixel:
    ; Calculate base address components
    ld a,b
    ; 1. Handle (y >> 6) * 0x0800 (screen third)
    and %11000000       ; Isolate bits 7-6
    rrca                ; Rotate bits into position
    rrca
    rrca                ; A now contains 0/0x08/0x10
    add a,0x40          ; Add screen base address
    ld h,a              ; Store in H
    ld l,0              ; HL = 0x4000/0x4800/0x5000

    ; 2. Add ((y & 0x07) << 8) (scanline within char)
    ld a,b
    and 0x07            ; Get bottom 3 bits of Y
    add a,h             ; Add to high byte
    ld h,a              ; HL += (Y%8)*256

    ; 3. Add ((y & 0x38) << 2) (block row)
    ld a,b
    and 0x38            ; Get middle bits of Y
    add a,a             ; Multiply by 4
    add a,a
    ld e,a
    ld d,0
    add hl,de           ; HL += (Y&0x38)*4

    ; 4. Add (x >> 3) (column byte)
    ld a,c
    rra                 ; Divide X by 8
    rra
    rra
    and 0x1f            ; Mask to 0-31
    ld e,a
    add hl,de           ; HL += X/8

    ; Generate pixel mask
    ld a,c
    and 0x07            ; Get X position within byte
    ld b,a
    ld a,0x80           ; Start with leftmost pixel
mask_loop:
    rrca                ; Rotate mask right
    djnz mask_loop
    ld c,a              ; C now contains pixel mask

    ; Set the pixel
    or (hl)             ; Combine with existing bits
    ld (hl),a           ; Update screen memory

    ret

 ;; Point (x,y) routine
 ;;   org     50998
x:  defb    0       ; coordinates
y:  defb    0

plot:
 ;   push    af
 ;   push    bc
 ;   push    de
 ;   push    hl      ; keep the registers

    ld      hl,tabpow2
    ld      a,(x)
    and     7       ; x mod 8
    ld      b,0
    ld      c,a
    add     hl,bc
    ld      a,(hl)
    ld      e,a     ; e contains one bit set

    ld      hl,tablinidx
    ld      a,(y)
    ld      b,0
    ld      c,a
    add     hl,bc
    ld      a,(hl)      ; table lookup

    ld      h,0
    ld      l,a
    add     hl,hl
    add     hl,hl
    add     hl,hl
    add     hl,hl
    add     hl,hl       ; x32 (16 bits)

    set     6,h         ; adds the screen start address (16384)

    ld      a,(x)
    srl     a
    srl     a
    srl     a           ; x/8.

    or      l
    ld      l,a         ; + x/8.

    ld      a,(hl)
    or      e           ; or = superposition mode.
    ld      (hl),a      ; set the pixel.

  ;  pop     hl
  ;  pop     de
  ;  pop     bc
  ;  pop     af          ; recovers registers.
    ret

tabpow2:
    ;; lookup table with powers of 2
    defb    128,64,32,16,8,4,2,1

    ;; screen lines lookup table
tablinidx:
    defb    0,8,16,24,32,40,48,56,1,9,17,25,33,41,49,57
    defb    2,10,18,26,34,42,50,58,3,11,19,27,35,43,51,59
    defb    4,12,20,28,36,44,52,60,5,13,21,29,37,45,53,61
    defb    6,14,22,30,38,46,54,62,7,15,23,31,39,47,55,63

    defb    64,72,80,88,96,104,112,120,65,73,81,89,97,105,113,121
    defb    66,74,82,90,98,106,114,122,67,75,83,91,99,107,115,123
    defb    68,76,84,92,100,108,116,124,69,77,85,93,101,109,117,125
    defb    70,78,86,94,102,110,118,126,71,79,87,95,103,111,119,127

    defb    128,136,144,152,160,168,176,184,129,137,145,153,161,169,177,185
    defb    130,138,146,154,162,170,178,186,131,139,147,155,163,171,179,187
    defb    132,140,148,156,164,172,180,188,133,141,149,157,165,173,181,189
    defb    134,142,150,158,166,174,182,190,135,143,151,159,167,175,183,191


picture:   
INCBIN "Judge-Dredd-Featured.png.scr"
last:
END start
