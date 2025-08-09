command_FL:  ; "F" - FILL COMMAND
;------------------------------------------------------
    halt
    in d, (c)  ; Read lower byte
    halt
    in e, (c)  ; Read higher byte

    halt
    in h, (c)  ; Read lower byte
    halt
    in l, (c)  ; Read higher byte

    halt		 	; Synchronizes with Arduino (NMI to continue)
    in b,(c)		; Read fill value from the z80 I/O port
fillLoop:
    ld (hl),b	 	; Write to memory
    inc hl		
    DEC DE      
    LD A, D      
    OR E         
    Jp NZ, fillLoop ; JP, 2 cycles saved per iter (jr=12,jp=10 cycles when taken)
