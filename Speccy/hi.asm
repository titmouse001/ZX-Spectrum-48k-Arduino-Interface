    org $8000
    
    ; Define some ROM routines
    cls     EQU $0D6B
    opench  EQU $1601
    print   EQU $203C
    
    ; Define our string to print
    string:
    db 'Hello world!',13
    
    start:
     ; Clear screen
     call cls
    
     ; Open upper screen channel
     ld a,2
     call opench
    
     ; Print string
     ld de,string
     ld bc,13
     call print
	 
	 

zap:
	ld b,0	;overall length counter
	di	
blp0:
	; and 248		;keep border colour the same
	
    CALL rnd   ;;  very good !!!!
;	Out (254),a	
	
 ;  CALL rand_8
	;Out (254),a	


;	LD A,R			; Load the A register with the refresh register
;	LD L,A			; Copy register A into register L
;	AND %00111111		; This masking prevents the address we are forming from accessing RAM
;	LD H,A			; Copy register A into register H
;	LD A,(HL)		; Load the pseudo-random value into A		
;	out (254),a	;move the speaker in or out depending on bit 4
	
;	CALL FastRND
;	out (254),a	;move the speaker in or out depending on bit 4
	
	
	
	;in a,($fe)
	Out (254),a	
	
	djnz blp0
	djnz blp0


     ret
	 

; https://chuntey.wordpress.com/2012/06/07/random-number-generators/



	

rnd     ld  hl,0xA280   ; xz -> yw
        ld  de,0xC0DE   ; yw -> zt
        ld  (rnd+1),de  ; x = y, z = w
        ld  a,e         ; w = w ^ ( w << 3 )
        add a,a
        add a,a
        add a,a
        xor e
        ld  e,a
        ld  a,h         ; t = x ^ (x << 1)
        add a,a
        xor h
        ld  d,a
        rra             ; t = t ^ (t >> 1) ^ w
        xor d
        xor e
        ld  h,l         ; y = z
        ld  l,a         ; w = t
        ld  (rnd+4),hl
        ret
	 

; returns pseudo random 8 bit number in A. Only affects A.
; (r_seed) is the byte from which the number is generated and MUST be
; initialised to a non zero value or this function will always return
; zero. Also r_seed must be in RAM, you can see why......

rand_8:
	LD	A,(r_seed)	; get seed
	AND	$B8		; mask non feedback bits
	SCF			; set carry
	JP	PO,no_clr	; skip clear if odd
	CCF			; complement carry (clear it)
no_clr:
	LD	A,(r_seed)	; get seed back
	RLA			; rotate carry into byte
	LD	(r_seed),A	; save back for next prn
	RET			; done

r_seed:
	DB	1		; prng seed byte (must not be zero)
	
   
; Fast RND
;
; An 8-bit pseudo-random number generator,
; using a similar method to the Spectrum ROM,
; - without the overhead of the Spectrum ROM.
;
; R = random number seed
; an integer in the range [1, 256]
;
; R -> (33*R) mod 257
;
; S = R - 1
; an 8-bit unsigned integer
FastRND:
 ld a, (seed)
 ld b, a 

 rrca ; multiply by 32
 rrca
 rrca
 xor 0x1f

 add a, b
 sbc a, 255 ; carry

 ld (seed), a
 ret   
	
seed:
	DB	1		; prng seed byte (must not be zero)
	
    end start