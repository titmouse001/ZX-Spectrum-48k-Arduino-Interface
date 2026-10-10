; ==============================================================================
; ZX SPECTRUM 48K IN-DEPTH STACKLESS MEMORY DIAGNOSTIC + AUDIO ALERTS!
; Target: Diagnostic ROM (Origin 0x0000)
;
; DIAGNOSTIC BORDER OUTPUT READOUT:
; ------------------------------------------------------------------------------
;
; >>> DATA BITS / CHIP ISOLATION (8 Border Bars, Top-to-Bottom, MSB D7 -> D0):
;     - RED BAR   = Faulty Bit
;     - GREEN BAR = Good Bit
;
; >>> BOTTOM-OF-SCREEN FLASH (Region + Phase ID):
;     The strip flashes N times = failed test phase number.
;     Flash colour shows which region/component failed:
;         - YELLOW  = Lower RAM data failure
;         - WHITE   = Upper RAM data failure
;         - BLUE    = Lower address mux/decoder fault (IC3/4 or IC23-26)
;         - MAGENTA = Upper address mux/decoder fault
;
;     Phase-to-flash-count lookup table: (all tests cover 48K RAM range 0x4000–0xFFFF)
;     +-------+------------------------------------------+--------------------+
;     | Flash | Test Phase                               | RAM Region Checked |
;     +-------+------------------------------------------+--------------------+
;     |   1   | Write 0x00                               | Lower              |
;     |   2   | Verify 0x00, Write 0xFF                  | Lower              |
;     |   3   | Verify 0xFF, Write 0x55                  | Upper              |
;     |   4   | Verify 0x55, Write 0xAA                  | Lower              |
;     |   5   | Verify 0xAA, Write Low Address Byte  (L) | Upper              |
;     |   6   | Verify L,    Write High Address Byte (H) | Lower              |
;     |   7   | Verify H                                 | Upper              |
;     |   8   | Checkerboard          (Even=55, Odd=AA)  | Lower              |
;     |   9   | Inverse Checkerboard  (Even=AA, Odd=55)  | Lower              |
;     |  10   | Walking 1s Data Bus                      | Lower              |
;     |  11   | Walking 0s Data Bus                      | Lower              |
;     |  12   | DRAM Refresh Retention Decay             | Lower              |
;     +-------+------------------------------------------+--------------------+
;
;     Note: Phases alternate lower/upper where noted; the region colour
;     (YELLOW/WHITE) always tells you which half of RAM the fault was in,
;     and the flash count tells you which test caught it.
;
; HARDWARE DATA BIT TO IC CHIP MATRIX:
; ------------------------------------------------------------------------------
; Data Bit | Lower RAM (16K Contended) | Upper RAM (32K Uncontended)
; ---------+---------------------------+----------------------------
;    D7    |           IC13            |            IC22
;    D6    |           IC12            |            IC21
;    D5    |           IC11            |            IC20
;    D4    |           IC10            |            IC19
;    D3    |           IC9             |            IC18
;    D2    |           IC8             |            IC17
;    D1    |           IC7             |            IC16
;    D0    |           IC6             |            IC15
; ==============================================================================

; Example: Test 3 failed with bad Upper RAM (3 white flashes),
;          showing bits D7 and D0 faulty (red, so upper D7=IC22 & D0=IC15 )
; +------------------------------------------------------------------+
; |                        TOP BORDER  (BLACK)                       |
; +----------------+--------------------------------+----------------+
; |                |                                |                |
; |   D7  [RED]    |                                |   D7  [RED]    |
; |   D6  [GREEN]  |                                |   D6  [GREEN]  |
; |   D5  [GREEN]  |                                |   D5  [GREEN]  |
; |   D4  [GREEN]  |         SCREEN                 |   D4  [GREEN]  |
; |   D3  [GREEN]  |                                |   D3  [GREEN]  |
; |   D2  [GREEN]  |                                |   D2  [GREEN]  |
; |   D1  [GREEN]  |                                |   D1  [GREEN]  |
; |   D0  [RED]    |                                |   D0  [RED]    |
; |                |                                |                |
; +----------------+--------------------------------+----------------+
; |               BOTTOM BORDER  [WHITE]  (UPPER RAM)                |
; |               >>>  FLASHES  3  TIMES  <<<                        |
; +------------------------------------------------------------------+

; --------------------------
; Test Catches Summary
; --------------------------
; Phases 1–4 (uniform 00/FF/55/AA)	Stuck data bits, simple data faults
; Phases 5–7 (address-as-data)	Address decoding faults, aliasing, shorts on A0–A15
; Phases 8–9 (checkerboard)	Adjacent-cell coupling, pattern sensitivity
; Phases 10–11 (walking 1s/0s)	Bit-to-bit coupling, pattern-sensitive faults
; Phase 12 (retention)	Weak DRAM cells, refresh faults


;    ORG 0x0000
;
;START:
;    DI                  ; Disable interrupts
;    IM 1                ; Force Interrupt Mode 1 for safe VSYNC HALT sync
;    JP MAIN_INIT
;
;   ; --------------------------------------------------------------------------
;    ; INTERRUPT VECTOR (0x0038) - Stackless VSYNC synchronization
;    ; --------------------------------------------------------------------------
;    DEFS 0x0038 - $
;
;INT_HANDLER:
;    LD SP, 0x0002       ; SP points to ROM (Push writes are safely ignored)
;    JP DRAW_BARS_WAKEUP ; Jump back to the frame drawing loop

    jp MAIN_INIT

    ds #2000 - $, 0
    org #2000

; 1. Setup IM 2 to point to our ROM table
    ld a, #21  ; High byte of our vector table (0x2100)
    ld i, a
    im 2
    ei

; 2. The Vector Table (Placed exactly at 0x2100 in ROM)
; The Table: Placed at a page boundary
;  ORG 0x2100
;  DEFS 257, 0x22      ; 257 bytes of 0x22. 
; No matter what the bus reads, vector is 0x2222.
VECTOR_TABLE:

    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


    db #22


; 3. The Handler (Placed exactly at 0x2222 in ROM)
    ds #2222 - $, 0
    org #2222
INT_HANDLER:
    jp DRAW_BARS_WAKEUP

P2_START:
; --------------------------------------------------------------------------
; PHASE 2: Verify 0x00, Write 0xFF
; --------------------------------------------------------------------------
    ld c, 2
    ld d, #00
    ld a, 5  ; Cyan
    out (#fe), a
    ld hl, #4000
P2_LOOP:
    ld a, (hl)
    cp d
    jp nz, TEST_FAILED
    ld (hl), #ff
    inc hl
    ld a, h
    or l
    jr nz, P2_LOOP

    ld bc, 10245
    ld de, 100
    ld ix, P3_START
    jp PLAY_TONE

P3_START:
; --------------------------------------------------------------------------
; PHASE 3: Verify 0xFF, Write 0x55
; --------------------------------------------------------------------------
    ld c, 3
    ld a, c
    ld d, #ff
    out (#fe), a
    ld h, d
    ld l, d
P3_LOOP:
    ld a, (hl)
    cp d
    jp nz, TEST_FAILED
    ld (hl), #55
    dec hl
    ld a, h
    cp #3f
    jr nz, P3_LOOP

    ld bc, 10243
    ld de, 100
    ld ix, P4_START
    jp PLAY_TONE

P4_START:
; --------------------------------------------------------------------------
; PHASE 4: Verify 0x55, Write 0xAA
; --------------------------------------------------------------------------
    ld c, 4
    ld d, #55
    ld a, 6  ; Yellow
    out (#fe), a
    ld hl, #4000
P4_LOOP:
    ld a, (hl)
    cp d
    jp nz, TEST_FAILED
    ld (hl), #aa
    inc hl
    ld a, h
    or l
    jr nz, P4_LOOP

    ld bc, 10246
    ld de, 100
    ld ix, P5_START
    jp PLAY_TONE

P5_START:
; --------------------------------------------------------------------------
; PHASE 5: Verify 0xAA, Write Low Address Byte (L)
; --------------------------------------------------------------------------
    ld c, 5
    ld a, c
    ld d, #aa
    out (#fe), a
    ld hl, #ffff
P5_LOOP:
    ld a, (hl)
    cp d
    jp nz, TEST_FAILED
    ld (hl), l
    dec hl
    ld a, h
    cp #3f
    jr nz, P5_LOOP

    ld bc, 10245
    ld de, 100
    ld ix, P6_START
    jp PLAY_TONE

P6_START:
; --------------------------------------------------------------------------
; PHASE 6: Verify L, Write High Address Byte (H)
; --------------------------------------------------------------------------
    ld c, 6
    ld a, 3  ; Magenta
    out (#fe), a
    ld hl, #4000
P6_LOOP:
    ld d, l
    ld a, (hl)
    cp d
    jp nz, TEST_FAILED
    ld (hl), h
    inc hl
    ld a, h
    or l
    jr nz, P6_LOOP

    ld bc, 10243
    ld de, 100
    ld ix, P7_START
    jp PLAY_TONE

P7_START:
; --------------------------------------------------------------------------
; PHASE 7: Verify H
; --------------------------------------------------------------------------
    ld a, 7
    ld c, a
    out (#fe), a
    ld hl, #ffff
P7_LOOP:
    ld d, h
    ld a, (hl)
    cp d
    jp nz, TEST_FAILED
    dec hl
    ld a, h
    cp #3f
    jr nz, P7_LOOP
    ld bc, 10247
    ld de, 100
    ld ix, P8_START
    jp PLAY_TONE

P8_START:
; --------------------------------------------------------------------------
; PHASE 8: Checkerboard (Even=55, Odd=AA)
; --------------------------------------------------------------------------
    ld c, 8
    ld a, 6  ; Yellow
    out (#fe), a

    ld hl, #4000
P8_WR_LOOP:
    bit 0, l
    jr nz, P8_WR_ODD
    ld (hl), #55
    jr P8_WR_NEXT
P8_WR_ODD:
    ld (hl), #aa
P8_WR_NEXT:
    inc hl
    ld a, h
    or l
    jr nz, P8_WR_LOOP

    ld hl, #4000
P8_RD_LOOP:
    bit 0, l
    jr nz, P8_RD_ODD
    ld d, #55
    jr P8_RD_CHK
P8_RD_ODD:
    ld d, #aa
P8_RD_CHK:
    ld a, (hl)
    cp d
    jp nz, TEST_FAILED
    inc hl
    ld a, h
    or l
    jr nz, P8_RD_LOOP

    ld bc, 10246
    ld de, 100
    ld ix, P9_START
    jp PLAY_TONE

P9_START:
; --------------------------------------------------------------------------
; PHASE 9: Inverse Checkerboard (Even=AA, Odd=55)
; --------------------------------------------------------------------------
    ld c, 9
    ld a, 5  ; Cyan
    out (#fe), a

    ld hl, #4000
P9_WR_LOOP:
    bit 0, l
    jr nz, P9_WR_ODD
    ld (hl), #aa
    jr P9_WR_NEXT
P9_WR_ODD:
    ld (hl), #55
P9_WR_NEXT:
    inc hl
    ld a, h
    or l
    jr nz, P9_WR_LOOP

    ld hl, #4000
P9_RD_LOOP:
    bit 0, l
    jr nz, P9_RD_ODD
    ld d, #aa
    jr P9_RD_CHK
P9_RD_ODD:
    ld d, #55
P9_RD_CHK:
    ld a, (hl)
    cp d
    jp nz, TEST_FAILED
    inc hl
    ld a, h
    or l
    jr nz, P9_RD_LOOP

    ld bc, 10245
    ld de, 100
    ld ix, P10_START
    jp PLAY_TONE

P10_START:
; --------------------------------------------------------------------------
; PHASE 10: Walking 1s Data Bus Test
; --------------------------------------------------------------------------
    ld c, 10
    ld a, 3  ; Magenta
    out (#fe), a
    ld hl, #4000
P10_ADDR_LOOP:
    ld b, 8
    ld d, #01
P10_BIT_LOOP:
    ld (hl), d
    ld a, (hl)
    cp d
    jp nz, TEST_FAILED
    sla d
    djnz P10_BIT_LOOP
    inc hl
    ld a, h
    or l
    jr nz, P10_ADDR_LOOP

    ld bc, 10243
    ld de, 100
    ld ix, P11_START
    jp PLAY_TONE

P11_START:
; --------------------------------------------------------------------------
; PHASE 11: Walking 0s Data Bus Test
; --------------------------------------------------------------------------
    ld c, 11
    ld a, 6  ; Yellow
    out (#fe), a
    ld hl, #4000
P11_ADDR_LOOP:
    ld b, 8
    ld d, #fe
P11_BIT_LOOP:
    ld (hl), d
    ld a, d
    cp d
    jp nz, TEST_FAILED
    rlc d
    djnz P11_BIT_LOOP
    inc hl
    ld a, h
    or l
    jr nz, P11_ADDR_LOOP

    ld bc, 10246
    ld de, 100
    ld ix, P12_START
    jp PLAY_TONE

P12_START:
; --------------------------------------------------------------------------
; PHASE 12: DRAM Refresh Retention Decay Test
; --------------------------------------------------------------------------
    ld c, 12
    ld a, 7  ; White
    out (#fe), a

    ld hl, #4000
P12_WR_LOOP:
    ld a, h
    xor l
    ld (hl), a
    inc hl
    ld a, h
    or l
    jr nz, P12_WR_LOOP

; --- REFRESH DELAY (Flash Blue/Black) ---
    ld b, 10
P12_DELAY_OUTER:
    ld a, 1
    out (#fe), a
    ld ix, #ffff
P12_DEL1:
    dec ix
    ld a, ixh
    or ixl
    jr nz, P12_DEL1

    xor a
    out (#fe), a
    ld ix, #ffff
P12_DEL2:
    dec ix
    ld a, ixh
    or ixl
    jr nz, P12_DEL2
    djnz P12_DELAY_OUTER

; Verify pattern
    ld a, 7
    out (#fe), a
    ld hl, #4000
P12_RD_LOOP:
    ld a, h
    xor l
    ld d, a
    ld a, (hl)
    cp d
    jp nz, TEST_FAILED
    inc hl
    ld a, h
    or l
    jr nz, P12_RD_LOOP

    ld bc, 10247
    ld de, 100
    ld ix, P13_START
    jp PLAY_TONE




; ==========================================================================
; PHASE 13: Address-line walking, A0-A7  (tests IC3/IC4 mux paths)
; ==========================================================================
P13_START:
    ld a, 6
    out (#fe), a

    ld e, 0  ; bit index 0..7
P13_NEXT_BIT:
; D = 1 << E
    ld d, 1
    ld a, e
    or a
    jr z, P13_MASK_OK
P13_SHIFT:
    sla d
    dec a
    jr nz, P13_SHIFT
P13_MASK_OK:

; Write pattern based on L bit E
    ld hl, #4000
P13_WR_LOOP:
    ld a, l
    and d
    jr z, P13_WR_55
    ld a, #aa
    jr P13_WR_STORE
P13_WR_55:
    ld a, #55
P13_WR_STORE:
    ld (hl), a
    inc hl
    ld a, h
    or l
    jr nz, P13_WR_LOOP

; Verify pattern
    ld hl, #4000
P13_RD_LOOP:
    ld a, l
    and d
    jr z, P13_RD_55
    ld a, #aa
    jr P13_RD_CHK
P13_RD_55:
    ld a, #55
P13_RD_CHK:
    ld b, a
    ld a, (hl)
    cp b
    jr nz, P13_FAIL
    inc hl
    ld a, h
    or l
    jr nz, P13_RD_LOOP

    inc e
    ld a, e
    cp 8
    jr nz, P13_NEXT_BIT

    ld bc, 10246
    ld de, 100
    ld ix, P14_START
    jp PLAY_TONE

P13_FAIL:
    ld b, d  ; B = failed address bit mask
    ld c, 13
    jp ADDR_FAILED


; ==========================================================================
; PHASE 14: Address-line walking, A8-A15 (mux + decoder high bits)
; ==========================================================================
P14_START:
    ld a, 5
    out (#fe), a

    ld e, 0  ; bit index 0..7 => A8..A15
P14_NEXT_BIT:
; D = 1 << E
    ld d, 1
    ld a, e
    or a
    jr z, P14_MASK_OK
P14_SHIFT:
    sla d
    dec a
    jr nz, P14_SHIFT
P14_MASK_OK:

; Write pattern based on H bit E
    ld hl, #4000
P14_WR_LOOP:
    ld a, h
    and d
    jr z, P14_WR_55
    ld a, #aa
    jr P14_WR_STORE
P14_WR_55:
    ld a, #55
P14_WR_STORE:
    ld (hl), a
    inc hl
    ld a, h
    or l
    jr nz, P14_WR_LOOP

; Verify pattern
    ld hl, #4000
P14_RD_LOOP:
    ld a, h
    and d
    jr z, P14_RD_55
    ld a, #aa
    jr P14_RD_CHK
P14_RD_55:
    ld a, #55
P14_RD_CHK:
    ld b, a
    ld a, (hl)
    cp b
    jr nz, P14_FAIL
    inc hl
    ld a, h
    or l
    jr nz, P14_RD_LOOP

    inc e
    ld a, e
    cp 8
    jr nz, P14_NEXT_BIT

    ld bc, 10245
    ld de, 100
    ld ix, P15_START
    jp PLAY_TONE

P14_FAIL:
    ld b, d  ; B = failed high-address bit mask
    ld c, 14
    jp ADDR_FAILED


; ==========================================================================
; PHASE 15: 16K block / decoder boundary test (IC23-IC26)
; ==========================================================================
P15_START:
    ld a, 7
    out (#fe), a

; Distinct values in each 16K region
    ld hl, #4000
    ld (hl), #55
    add hl, hl
    ld (hl), #aa
    ld hl, #c000
    ld (hl), #33
    res 7, h

    ld a, (hl)
    cp #55
    jr nz, P15_FAIL
    ld hl, #8000
    ld a, (hl)
    cp #aa
    jr nz, P15_FAIL
    set 6, h
    ld a, (hl)
    cp #33
    jr nz, P15_FAIL

; Boundary 0x7FFF / 0x8000
    ld hl, #7fff
    ld (hl), #11
    inc hl
    ld (hl), #22
    dec hl

    ld a, (hl)
    cp #11
    jr nz, P15_FAIL
    inc hl
    ld a, (hl)
    cp #22
    jr nz, P15_FAIL

    ld bc, 10247
    ld de, 100
    ld ix, TEST_PASS
    jp PLAY_TONE

P15_FAIL:
    ld bc, 65295
    jp ADDR_FAILED




; ==========================================================================
; TEST PASSED: ALL RAM OK!
; ==========================================================================
TEST_PASS:
; 1. Clear Screen Bitmap (6144 bytes) to 0x00
    ld hl, #4000
    ld de, #1800
CLR_PIX:
    ld (hl), #00
    inc hl
    dec de
    ld a, d
    or e
    jr nz, CLR_PIX

; 2. Clear Screen Attributes (768 bytes) to 0x00
    ld de, #0300
CLR_ATTR:
    ld (hl), #00
    inc hl
    dec de
    ld a, d
    or e
    jr nz, CLR_ATTR

; 3. Play Jolly Tune (Rising Arpeggio)
    ld bc, 25604
    ld de, 150
    ld ix, JT2
    jp PLAY_TONE
JT2:
    ld b, 80
    ld de, 150
    ld ix, JT3
    jp PLAY_TONE
JT3:
    ld b, 60
    ld de, 150
    ld ix, JT4
    jp PLAY_TONE
JT4:
    ld b, 40
    ld de, 300
    ld ix, PASS_LOCK
    jp PLAY_TONE

PASS_LOCK:
; Flash Border Green / Black forever
    ld a, 4
    out (#fe), a
    ld ix, #b000
PL_DEL1:
    dec ix
    ld a, ixh
    or ixl
    jr nz, PL_DEL1

    xor a
    out (#fe), a
    ld ix, #9000
PL_DEL2:
    dec ix
    ld a, ixh
    or ixl
    jr nz, PL_DEL2
    jr PASS_LOCK


; ==========================================================================
; FAILURE HANDLER
; ==========================================================================
; Register allocation during failure loop:
;   B    = Error Mask (bit=1 means faulty)
;   D    = Phase ID (0..12)
;   IXH  = Region flash colour (6=yellow, 7=white)
;   E    = Flash state (0=ON, 1=short OFF, 2=long pause)
;   IYH  = Frame timer for current flash state
;   IYL  = Number of flashes completed so far
; ==========================================================================
TEST_FAILED:
    ld a, c
    cp 13
    jr nc, ADDR_FAILED  ; phases 13+ are address/decoder failures

    xor d  ; A = Actual XOR Expected
    ld b, a  ; B = Error Mask
    ld d, c  ; D = Phase ID

    ld a, h
    cp #80
    ld a, 6  ; yellow = lower RAM
    jr c, SAVE_COLOR
    inc a
SAVE_COLOR:
    ld ixh, a
    jr PLAY_BAD_TUNE

ADDR_FAILED:
    ld d, c  ; D = Phase ID
    ld a, h
    cp #80
    ld a, 1  ; 1 = Blue (Lower IC/decoder fault)
    jr c, SAVE_COLOR_ADDR
    ld a, 3  ; 3 = Magenta (Upper IC/decoder fault)
SAVE_COLOR_ADDR:
    ld ixh, a
; fall through to PLAY_BAD_TUNE

PLAY_BAD_TUNE:
    exx
    ld c, 2  ; Red border
    ld hl, 10270
BAD_TUNE_OUTER:
    ld b, h
    ld de, 40
BAD_TUNE_INNER:
    ld a, c
    or #10
    out (#fe), a
    ld a, b
BTI_D1:
    dec a
    jr nz, BTI_D1
    ld a, c
    and #ef
    out (#fe), a
    ld a, b
BTI_D2:
    dec a
    jr nz, BTI_D2
    dec de
    ld a, d
    or e
    jr nz, BAD_TUNE_INNER

    inc h
    dec l
    jr nz, BAD_TUNE_OUTER
    exx  ; Restore B (mask) and D (phase)

; ---- Initialise flash state ----
    ld e, 0  ; State = ON
    ld iyh, 25
    ld iyl, e

; Enter the persistent failure display loop
    ei
    halt  ; INT will jump to DRAW_BARS_WAKEUP
; (unreachable below; loop is closed by the INT handler)


; ==========================================================================
; PERSISTENT FAILURE DISPLAY (bars always on + bottom-strip phase flash)
;   Entry via INT_HANDLER -> here, once per VSYNC frame.
; ==========================================================================
DRAW_BARS_WAKEUP:
    di

; -- Top part: black (before bar drawing starts) --
    xor a
    out (#fe), a

; Sync delay so bars begin at a consistent raster line
    ld hl, 700
BAR_SYNC_DELAY:
    dec hl
    ld a, h
    or l
    jr nz, BAR_SYNC_DELAY

; -- Draw the 8 data-bit bars (D7 down to D0) --
    ld c, 8  ; 8 bits = 8 horizontal bars
    ld a, b  ; A = Error Mask

BARS_BIT_LOOP:
    rlca  ; Shift MSB (D7 first down to D0) into Carry
    ld ixl, a  ; Save rotated mask state in IXL (IXH is safe)

    jr c, BAR_FAULTY

BAR_GOOD:
    ld a, 4  ; GREEN  (Good Bit = 0)
    jr BAR_COLOR_SET
BAR_FAULTY:
    ld a, 2  ; RED (Faulty Bit = 1)

BAR_COLOR_SET:
    out (#fe), a  ; Set border colour for current bit bar

; --- BAR ON DELAY ---
    ld hl, 241 / 2
BAR_ON_DELAY:
    dec hl
    ld a, h
    or l
    jr nz, BAR_ON_DELAY

; --- BLACK GAP DELAY ---
    xor a
    out (#fe), a  ; Black separator between bars
    ld hl, 94 / 2
BAR_OFF_DELAY:
    dec hl
    ld a, h
    or l
    jr nz, BAR_OFF_DELAY

    ld a, ixl  ; Restore rotated mask state
    dec c
    jr nz, BARS_BIT_LOOP

; ---------------------------------------------------------------------
; -- Bottom strip: phase-ID flash (fills remainder of the frame) --
; ---------------------------------------------------------------------
    ld a, e
    or a
    jr z, FLASH_ON
    dec a
    jr z, FLASH_OFF
; State 2 = long pause (black)
    xor a
    out (#fe), a
    jr FLASH_STATE_DONE

FLASH_ON:
    ld a, ixh  ; Region colour (Red / Cyan)
    out (#fe), a
    jr FLASH_STATE_DONE

FLASH_OFF:
    xor a
    out (#fe), a

FLASH_STATE_DONE:
; -- Advance flash state machine when frame timer expires --
    dec iyh
    jr nz, FRAME_DONE

    ld a, e
    or a
    jr z, WAS_ON
    dec a
    jr z, WAS_OFF

; Was in long pause -> restart count from zero
    ld e, 0
    ld iyh, 25
    ld iyl, e
    jr FRAME_DONE

WAS_ON:
; ON phase just finished
    inc iyl
    ld a, iyl
    cp d
    jr z, ALL_FLASHES_DONE

; More flashes to go -> short black pause
    ld e, 1
    ld iyh, 15
    jr FRAME_DONE

ALL_FLASHES_DONE:
; All D flashes shown -> long pause before restarting
    ld e, 2
    ld iyh, 60
    jr FRAME_DONE

WAS_OFF:
; Short pause done -> back to ON
    ld e, 0
    ld iyh, 25

FRAME_DONE:
; Sync to next VSYNC. INT fires, jumps to INT_HANDLER,
; which returns control back to DRAW_BARS_WAKEUP.
    ei
    halt
    jp DRAW_BARS_WAKEUP  ; Fallback (normally the INT handler handles this)


MAIN_INIT:

;  ---- DEBUG
;    LD C, 1             ; Phase ID = 1
;    LD D, 0x00          ; Expected value
;    LD A, 0x81          ; Actual (faulty) value read
;    LD H, 0xB0          ; Lower RAM address high byte
;    JP TEST_FAILED
;
;    LD C, 1             ; Phase ID = 1
;    LD D, 0x00
;    LD A, 0x81          ; Faulty D7 & D0 -> IC22 & IC15 (upper RAM)
;    LD H, 0x80
;    JP TEST_FAILED
;
;    LD C, 3
;    LD D, 0x00
;    LD A, 0xFF          ; All bits faulty
;    LD H, 0xC0
;    JP TEST_FAILED
;
;    LD C, 6
;    LD D, 0x00
;    LD A, 0xAA          ; D7, D5, D3, D1 faulty
;    LD H, 0x80
;    JP TEST_FAILED
;
;    LD C, 12
;    LD D, 0x00
;    LD A, 0x08          ; Bit D3 faulty -> IC18
;    LD H, 0xFF
;    JP TEST_FAILED
;  ---------------------------------------------------------------------------

; --------------------------------------------------------------------------
; PHASE 1: Write 0x00
; --------------------------------------------------------------------------
    ld d, #00
    ld a, 6  ; Yellow
    out (#fe), a
    ld hl, #4000
P1_LOOP:
    ld (hl), d
    inc hl
    ld a, h
    or l
    jr nz, P1_LOOP

    ld bc, 10246
    ld de, 100
    ld ix, P2_START
;     JP PLAY_TONE  ; -mdl


; ==========================================================================
; STACKLESS AUDIO SUBROUTINE
; ==========================================================================
PLAY_TONE:
PT_LOOP:
    ld a, c
    or #10  ; Set Beeper bit 4 high
    out (#fe), a
    ld a, b
PT_DEL1:
    dec a
    jr nz, PT_DEL1

    ld a, c
    and #ef  ; Set Beeper bit 4 low
    out (#fe), a
    ld a, b
PT_DEL2:
    dec a
    jr nz, PT_DEL2

    dec de
    ld a, d
    or e
    jr nz, PT_LOOP

    jp (ix)  ; Stackless Return






; ==========================================================================
; ROM PADDING (Exact 16K binary size for EPROM burning)
; ==========================================================================
    ds #4000 - $, #ff
