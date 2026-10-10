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
 
JP MAIN_INIT

ORG 0x2000
    
    ; 1. Setup IM 2 to point to our ROM table
    LD A, 0x21          ; High byte of our vector table (0x2100)
    LD I, A
    IM 2
    EI

    ; 2. The Vector Table (Placed exactly at 0x2100 in ROM)
    ; The Table: Placed at a page boundary
  ;  ORG 0x2100
  ;  DEFS 257, 0x22      ; 257 bytes of 0x22. 
                        ; No matter what the bus reads, vector is 0x2222.
VECTOR_TABLE:
REPT 257
   DB 0x22
ENDM

    ; 3. The Handler (Placed exactly at 0x2222 in ROM)
    ORG 0x2222
INT_HANDLER:
    LD SP, 0x3FFF       ; Safely point SP to ROM padding
    JP DRAW_BARS_WAKEUP


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
    LD C, 1
    LD D, 0x00
    LD A, 6             ; Yellow
    OUT (0xFE), A
    LD HL, 0x4000
P1_LOOP:
    LD (HL), D
    INC HL
    LD A, H
    OR L
    JR NZ, P1_LOOP

    LD B, 40
    LD DE, 100
    LD C, 6
    LD IX, P2_START
    JP PLAY_TONE

P2_START:
    ; --------------------------------------------------------------------------
    ; PHASE 2: Verify 0x00, Write 0xFF
    ; --------------------------------------------------------------------------
    LD C, 2
    LD D, 0x00
    LD A, 5             ; Cyan
    OUT (0xFE), A
    LD HL, 0x4000
P2_LOOP:
    LD A, (HL)
    CP D
    JP NZ, TEST_FAILED
    LD (HL), 0xFF
    INC HL
    LD A, H
    OR L
    JR NZ, P2_LOOP

    LD B, 40
    LD DE, 100
    LD C, 5
    LD IX, P3_START
    JP PLAY_TONE

P3_START:
    ; --------------------------------------------------------------------------
    ; PHASE 3: Verify 0xFF, Write 0x55
    ; --------------------------------------------------------------------------
    LD C, 3
    LD D, 0xFF
    LD A, 3             ; Magenta
    OUT (0xFE), A
    LD HL, 0xFFFF
P3_LOOP:
    LD A, (HL)
    CP D
    JP NZ, TEST_FAILED
    LD (HL), 0x55
    DEC HL
    LD A, H
    CP 0x3F
    JR NZ, P3_LOOP

    LD B, 40
    LD DE, 100
    LD C, 3
    LD IX, P4_START
    JP PLAY_TONE

P4_START:
    ; --------------------------------------------------------------------------
    ; PHASE 4: Verify 0x55, Write 0xAA
    ; --------------------------------------------------------------------------
    LD C, 4
    LD D, 0x55
    LD A, 6             ; Yellow
    OUT (0xFE), A
    LD HL, 0x4000
P4_LOOP:
    LD A, (HL)
    CP D
    JP NZ, TEST_FAILED
    LD (HL), 0xAA
    INC HL
    LD A, H
    OR L
    JR NZ, P4_LOOP

    LD B, 40
    LD DE, 100
    LD C, 6
    LD IX, P5_START
    JP PLAY_TONE

P5_START:
    ; --------------------------------------------------------------------------
    ; PHASE 5: Verify 0xAA, Write Low Address Byte (L)
    ; --------------------------------------------------------------------------
    LD C, 5
    LD D, 0xAA
    LD A, 5             ; Cyan
    OUT (0xFE), A
    LD HL, 0xFFFF
P5_LOOP:
    LD A, (HL)
    CP D
    JP NZ, TEST_FAILED
    LD (HL), L
    DEC HL
    LD A, H
    CP 0x3F
    JR NZ, P5_LOOP

    LD B, 40
    LD DE, 100
    LD C, 5
    LD IX, P6_START
    JP PLAY_TONE

P6_START:
    ; --------------------------------------------------------------------------
    ; PHASE 6: Verify L, Write High Address Byte (H)
    ; --------------------------------------------------------------------------
    LD C, 6
    LD A, 3             ; Magenta
    OUT (0xFE), A
    LD HL, 0x4000
P6_LOOP:
    LD D, L
    LD A, (HL)
    CP D
    JP NZ, TEST_FAILED
    LD (HL), H
    INC HL
    LD A, H
    OR L
    JR NZ, P6_LOOP

    LD B, 40
    LD DE, 100
    LD C, 3
    LD IX, P7_START
    JP PLAY_TONE

P7_START:
    ; --------------------------------------------------------------------------
    ; PHASE 7: Verify H
    ; --------------------------------------------------------------------------
    LD C, 7
    LD A, 7             ; White
    OUT (0xFE), A
    LD HL, 0xFFFF
P7_LOOP:
    LD D, H
    LD A, (HL)
    CP D
    JP NZ, TEST_FAILED
    DEC HL
    LD A, H
    CP 0x3F
    JR NZ, P7_LOOP
    LD B, 40
    LD DE, 100
    LD C, 7
    LD IX, P8_START
    JP PLAY_TONE

P8_START:
    ; --------------------------------------------------------------------------
    ; PHASE 8: Checkerboard (Even=55, Odd=AA)
    ; --------------------------------------------------------------------------
    LD C, 8
    LD A, 6             ; Yellow
    OUT (0xFE), A

    LD HL, 0x4000
P8_WR_LOOP:
    BIT 0, L
    JR NZ, P8_WR_ODD
    LD (HL), 0x55
    JR P8_WR_NEXT
P8_WR_ODD:
    LD (HL), 0xAA
P8_WR_NEXT:
    INC HL
    LD A, H
    OR L
    JR NZ, P8_WR_LOOP

    LD HL, 0x4000
P8_RD_LOOP:
    BIT 0, L
    JR NZ, P8_RD_ODD
    LD D, 0x55
    JR P8_RD_CHK
P8_RD_ODD:
    LD D, 0xAA
P8_RD_CHK:
    LD A, (HL)
    CP D
    JP NZ, TEST_FAILED
    INC HL
    LD A, H
    OR L
    JR NZ, P8_RD_LOOP

    LD B, 40
    LD DE, 100
    LD C, 6
    LD IX, P9_START
    JP PLAY_TONE

P9_START:
    ; --------------------------------------------------------------------------
    ; PHASE 9: Inverse Checkerboard (Even=AA, Odd=55)
    ; --------------------------------------------------------------------------
    LD C, 9
    LD A, 5             ; Cyan
    OUT (0xFE), A

    LD HL, 0x4000
P9_WR_LOOP:
    BIT 0, L
    JR NZ, P9_WR_ODD
    LD (HL), 0xAA
    JR P9_WR_NEXT
P9_WR_ODD:
    LD (HL), 0x55
P9_WR_NEXT:
    INC HL
    LD A, H
    OR L
    JR NZ, P9_WR_LOOP

    LD HL, 0x4000
P9_RD_LOOP:
    BIT 0, L
    JR NZ, P9_RD_ODD
    LD D, 0xAA
    JR P9_RD_CHK
P9_RD_ODD:
    LD D, 0x55
P9_RD_CHK:
    LD A, (HL)
    CP D
    JP NZ, TEST_FAILED
    INC HL
    LD A, H
    OR L
    JR NZ, P9_RD_LOOP

    LD B, 40
    LD DE, 100
    LD C, 5
    LD IX, P10_START
    JP PLAY_TONE

P10_START:
    ; --------------------------------------------------------------------------
    ; PHASE 10: Walking 1s Data Bus Test
    ; --------------------------------------------------------------------------
    LD C, 10
    LD A, 3             ; Magenta
    OUT (0xFE), A
    LD HL, 0x4000
P10_ADDR_LOOP:
    LD B, 8
    LD D, 0x01
P10_BIT_LOOP:
    LD (HL), D
    LD A, (HL)
    CP D
    JP NZ, TEST_FAILED
    SLA D
    DJNZ P10_BIT_LOOP
    INC HL
    LD A, H
    OR L
    JR NZ, P10_ADDR_LOOP

    LD B, 40
    LD DE, 100
    LD C, 3
    LD IX, P11_START
    JP PLAY_TONE

P11_START:
    ; --------------------------------------------------------------------------
    ; PHASE 11: Walking 0s Data Bus Test
    ; --------------------------------------------------------------------------
    LD C, 11
    LD A, 6             ; Yellow
    OUT (0xFE), A
    LD HL, 0x4000
P11_ADDR_LOOP:
    LD B, 8
    LD D, 0xFE
P11_BIT_LOOP:
    LD (HL), D
    LD A, (HL)
    CP D
    JP NZ, TEST_FAILED
    RLC D
    DJNZ P11_BIT_LOOP
    INC HL
    LD A, H
    OR L
    JR NZ, P11_ADDR_LOOP

    LD B, 40
    LD DE, 100
    LD C, 6
    LD IX, P12_START
    JP PLAY_TONE

P12_START:
    ; --------------------------------------------------------------------------
    ; PHASE 12: DRAM Refresh Retention Decay Test
    ; --------------------------------------------------------------------------
    LD C, 12
    LD A, 7             ; White
    OUT (0xFE), A

    LD HL, 0x4000
P12_WR_LOOP:
    LD A, H
    XOR L
    LD (HL), A
    INC HL
    LD A, H
    OR L
    JR NZ, P12_WR_LOOP

    ; --- REFRESH DELAY (Flash Blue/Black) ---
    LD B, 10
P12_DELAY_OUTER:
    LD A, 1
    OUT (0xFE), A
    LD IX, 0xFFFF
P12_DEL1:
    DEC IX
    LD A, IXH
    OR IXL
    JR NZ, P12_DEL1

    LD A, 0
    OUT (0xFE), A
    LD IX, 0xFFFF
P12_DEL2:
    DEC IX
    LD A, IXH
    OR IXL
    JR NZ, P12_DEL2
    DJNZ P12_DELAY_OUTER

    ; Verify pattern
    LD A, 7
    OUT (0xFE), A
    LD HL, 0x4000
P12_RD_LOOP:
    LD A, H
    XOR L
    LD D, A
    LD A, (HL)
    CP D
    JP NZ, TEST_FAILED
    INC HL
    LD A, H
    OR L
    JR NZ, P12_RD_LOOP

    LD B, 40
    LD DE, 100
    LD C, 7
    LD IX, P13_START
    JP PLAY_TONE




; ==========================================================================
; PHASE 13: Address-line walking, A0-A7  (tests IC3/IC4 mux paths)
; ==========================================================================
P13_START:
    LD C, 13
    LD A, 6                 ; Yellow border
    OUT (0xFE), A

    LD E, 0                 ; bit index 0..7
P13_NEXT_BIT:
    ; D = 1 << E
    LD D, 1
    LD A, E
    OR A
    JR Z, P13_MASK_OK
P13_SHIFT:
    SLA D
    DEC A
    JR NZ, P13_SHIFT
P13_MASK_OK:

    ; Write pattern based on L bit E
    LD HL, 0x4000
P13_WR_LOOP:
    LD A, L
    AND D
    JR Z, P13_WR_55
    LD A, 0xAA
    JR P13_WR_STORE
P13_WR_55:
    LD A, 0x55
P13_WR_STORE:
    LD (HL), A
    INC HL
    LD A, H
    OR L
    JR NZ, P13_WR_LOOP

    ; Verify pattern
    LD HL, 0x4000
P13_RD_LOOP:
    LD A, L
    AND D
    JR Z, P13_RD_55
    LD A, 0xAA
    JR P13_RD_CHK
P13_RD_55:
    LD A, 0x55
P13_RD_CHK:
    LD B, A
    LD A, (HL)
    CP B
    JR NZ, P13_FAIL
    INC HL
    LD A, H
    OR L
    JR NZ, P13_RD_LOOP

    INC E
    LD A, E
    CP 8
    JR NZ, P13_NEXT_BIT

    LD B, 40
    LD DE, 100
    LD C, 6
    LD IX, P14_START
    JP PLAY_TONE

P13_FAIL:
    LD B, D                 ; B = failed address bit mask
    LD C, 13
    JP ADDR_FAILED


; ==========================================================================
; PHASE 14: Address-line walking, A8-A15 (mux + decoder high bits)
; ==========================================================================
P14_START:
    LD C, 14
    LD A, 5                 ; Cyan border
    OUT (0xFE), A

    LD E, 0                 ; bit index 0..7 => A8..A15
P14_NEXT_BIT:
    ; D = 1 << E
    LD D, 1
    LD A, E
    OR A
    JR Z, P14_MASK_OK
P14_SHIFT:
    SLA D
    DEC A
    JR NZ, P14_SHIFT
P14_MASK_OK:

    ; Write pattern based on H bit E
    LD HL, 0x4000
P14_WR_LOOP:
    LD A, H
    AND D
    JR Z, P14_WR_55
    LD A, 0xAA
    JR P14_WR_STORE
P14_WR_55:
    LD A, 0x55
P14_WR_STORE:
    LD (HL), A
    INC HL
    LD A, H
    OR L
    JR NZ, P14_WR_LOOP

    ; Verify pattern
    LD HL, 0x4000
P14_RD_LOOP:
    LD A, H
    AND D
    JR Z, P14_RD_55
    LD A, 0xAA
    JR P14_RD_CHK
P14_RD_55:
    LD A, 0x55
P14_RD_CHK:
    LD B, A
    LD A, (HL)
    CP B
    JR NZ, P14_FAIL
    INC HL
    LD A, H
    OR L
    JR NZ, P14_RD_LOOP

    INC E
    LD A, E
    CP 8
    JR NZ, P14_NEXT_BIT

    LD B, 40
    LD DE, 100
    LD C, 5
    LD IX, P15_START
    JP PLAY_TONE

P14_FAIL:
    LD B, D                 ; B = failed high-address bit mask
    LD C, 14
    JP ADDR_FAILED


; ==========================================================================
; PHASE 15: 16K block / decoder boundary test (IC23-IC26)
; ==========================================================================
P15_START:
    LD C, 15
    LD A, 7                 ; White border
    OUT (0xFE), A

    ; Distinct values in each 16K region
    LD HL, 0x4000
    LD (HL), 0x55
    LD HL, 0x8000
    LD (HL), 0xAA
    LD HL, 0xC000
    LD (HL), 0x33

    LD HL, 0x4000
    LD A, (HL)
    CP 0x55
    JR NZ, P15_FAIL
    LD HL, 0x8000
    LD A, (HL)
    CP 0xAA
    JR NZ, P15_FAIL
    LD HL, 0xC000
    LD A, (HL)
    CP 0x33
    JR NZ, P15_FAIL

    ; Boundary 0x7FFF / 0x8000
    LD HL, 0x7FFF
    LD (HL), 0x11
    LD HL, 0x8000
    LD (HL), 0x22

    LD HL, 0x7FFF
    LD A, (HL)
    CP 0x11
    JR NZ, P15_FAIL
    LD HL, 0x8000
    LD A, (HL)
    CP 0x22
    JR NZ, P15_FAIL

    LD B, 40
    LD DE, 100
    LD C, 7
    LD IX, TEST_PASS
    JP PLAY_TONE

P15_FAIL:
    LD B, 0xFF              ; generic decoder / block fault
    LD C, 15
    JP ADDR_FAILED




    ; ==========================================================================
    ; TEST PASSED: ALL RAM OK!
    ; ==========================================================================
TEST_PASS:
    ; 1. Clear Screen Bitmap (6144 bytes) to 0x00
    LD HL, 0x4000
    LD DE, 0x1800
CLR_PIX:
    LD (HL), 0x00
    INC HL
    DEC DE
    LD A, D
    OR E
    JR NZ, CLR_PIX

    ; 2. Clear Screen Attributes (768 bytes) to 0x00
    LD DE, 0x0300
CLR_ATTR:
    LD (HL), 0x00
    INC HL
    DEC DE
    LD A, D
    OR E
    JR NZ, CLR_ATTR

    ; 3. Play Jolly Tune (Rising Arpeggio)
    LD C, 4             ; Green border
    LD B, 100
    LD DE, 150
    LD IX, JT2
    JP PLAY_TONE
JT2:
    LD B, 80
    LD DE, 150
    LD IX, JT3
    JP PLAY_TONE
JT3:
    LD B, 60
    LD DE, 150
    LD IX, JT4
    JP PLAY_TONE
JT4:
    LD B, 40
    LD DE, 300
    LD IX, PASS_LOCK
    JP PLAY_TONE

PASS_LOCK:
    ; Flash Border Green / Black forever
    LD A, 4
    OUT (0xFE), A
    LD IX, 0xB000
PL_DEL1:
    DEC IX
    LD A, IXH
    OR IXL
    JR NZ, PL_DEL1

    LD A, 0
    OUT (0xFE), A
    LD IX, 0x9000
PL_DEL2:
    DEC IX
    LD A, IXH
    OR IXL
    JR NZ, PL_DEL2
    JR PASS_LOCK


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
    LD A, C
    CP 13
    JR NC, ADDR_FAILED      ; phases 13+ are address/decoder failures

    XOR D                   ; A = Actual XOR Expected
    LD B, A                 ; B = Error Mask
    LD D, C                 ; D = Phase ID

    LD A, H
    CP 0x80
    LD A, 6                 ; yellow = lower RAM
    JR C, SAVE_COLOR
    LD A, 7                 ; white = upper RAM
SAVE_COLOR:
    LD IXH, A
    JR PLAY_BAD_TUNE

ADDR_FAILED:
    LD D, C                 ; D = Phase ID
    LD A, H
    CP 0x80
    LD A, 1     ; 1 = Blue (Lower IC/decoder fault)
    JR C, SAVE_COLOR_ADDR
    LD A, 3     ; 3 = Magenta (Upper IC/decoder fault)
SAVE_COLOR_ADDR:
    LD IXH, A
    ; fall through to PLAY_BAD_TUNE

PLAY_BAD_TUNE:
    EXX
    LD C, 2                 ; Red border
    LD H, 40
    LD L, 30
BAD_TUNE_OUTER:
    LD B, H
    LD DE, 40
BAD_TUNE_INNER:
    LD A, C
    OR 0x10
    OUT (0xFE), A
    LD A, B
BTI_D1:
    DEC A
    JR NZ, BTI_D1
    LD A, C
    AND 0xEF
    OUT (0xFE), A
    LD A, B
BTI_D2:
    DEC A
    JR NZ, BTI_D2
    DEC DE
    LD A, D
    OR E
    JR NZ, BAD_TUNE_INNER

    INC H
    DEC L
    JR NZ, BAD_TUNE_OUTER
    EXX                 ; Restore B (mask) and D (phase)

    ; ---- Initialise flash state ----
    LD E, 0             ; State = ON
    LD IYH, 25          ; Frames ON
    LD IYL, 0           ; Flashes completed

    ; Enter the persistent failure display loop
    LD SP, 0x0002
    EI
    HALT                ; INT will jump to DRAW_BARS_WAKEUP
    ; (unreachable below; loop is closed by the INT handler)


    ; ==========================================================================
    ; PERSISTENT FAILURE DISPLAY (bars always on + bottom-strip phase flash)
    ;   Entry via INT_HANDLER -> here, once per VSYNC frame.
    ; ==========================================================================
DRAW_BARS_WAKEUP:
    DI

    ; -- Top part: black (before bar drawing starts) --
    XOR A
    OUT (0xFE), A

    ; Sync delay so bars begin at a consistent raster line
    LD HL, 700
BAR_SYNC_DELAY:
    DEC HL
    LD A, H
    OR L
    JR NZ, BAR_SYNC_DELAY

    ; -- Draw the 8 data-bit bars (D7 down to D0) --
    LD C, 8             ; 8 bits = 8 horizontal bars
    LD A, B             ; A = Error Mask

BARS_BIT_LOOP:
    RLCA                ; Shift MSB (D7 first down to D0) into Carry
    LD IXL, A           ; Save rotated mask state in IXL (IXH is safe)

    JR C, BAR_FAULTY

BAR_GOOD:
    LD A, 4             ; GREEN  (Good Bit = 0)
    JR BAR_COLOR_SET
BAR_FAULTY:
    LD A, 2             ; RED (Faulty Bit = 1)

BAR_COLOR_SET:
    OUT (0xFE), A       ; Set border colour for current bit bar

    ; --- BAR ON DELAY ---
    LD HL, 241/2
BAR_ON_DELAY:
    DEC HL
    LD A, H
    OR L
    JR NZ, BAR_ON_DELAY

    ; --- BLACK GAP DELAY ---
    XOR A
    OUT (0xFE), A       ; Black separator between bars
    LD HL, 94/2
BAR_OFF_DELAY:
    DEC HL
    LD A, H
    OR L
    JR NZ, BAR_OFF_DELAY

    LD A, IXL           ; Restore rotated mask state
    DEC C
    JR NZ, BARS_BIT_LOOP

    ; ---------------------------------------------------------------------
    ; -- Bottom strip: phase-ID flash (fills remainder of the frame) --
    ; ---------------------------------------------------------------------
    LD A, E
    OR A
    JR Z, FLASH_ON
    CP 1
    JR Z, FLASH_OFF
    ; State 2 = long pause (black)
    XOR A
    OUT (0xFE), A
    JR FLASH_STATE_DONE

FLASH_ON:
    LD A, IXH           ; Region colour (Red / Cyan)
    OUT (0xFE), A
    JR FLASH_STATE_DONE

FLASH_OFF:
    XOR A
    OUT (0xFE), A

FLASH_STATE_DONE:
    ; -- Advance flash state machine when frame timer expires --
    DEC IYH
    JR NZ, FRAME_DONE

    LD A, E
    OR A
    JR Z, WAS_ON
    CP 1
    JR Z, WAS_OFF

    ; Was in long pause -> restart count from zero
    LD E, 0
    LD IYH, 25
    LD IYL, 0
    JR FRAME_DONE

WAS_ON:
    ; ON phase just finished
    INC IYL
    LD A, IYL
    CP D
    JR Z, ALL_FLASHES_DONE

    ; More flashes to go -> short black pause
    LD E, 1
    LD IYH, 15
    JR FRAME_DONE

ALL_FLASHES_DONE:
    ; All D flashes shown -> long pause before restarting
    LD E, 2
    LD IYH, 60
    JR FRAME_DONE

WAS_OFF:
    ; Short pause done -> back to ON
    LD E, 0
    LD IYH, 25

FRAME_DONE:
    ; Sync to next VSYNC. INT fires, jumps to INT_HANDLER,
    ; which returns control back to DRAW_BARS_WAKEUP.
    LD SP, 0x0002
    EI
    HALT
    JP DRAW_BARS_WAKEUP ; Fallback (normally the INT handler handles this)


    ; ==========================================================================
    ; STACKLESS AUDIO SUBROUTINE
    ; ==========================================================================
PLAY_TONE:
PT_LOOP:
    LD A, C
    OR 0x10             ; Set Beeper bit 4 high
    OUT (0xFE), A
    LD A, B
PT_DEL1:
    DEC A
    JR NZ, PT_DEL1

    LD A, C
    AND 0xEF            ; Set Beeper bit 4 low
    OUT (0xFE), A
    LD A, B
PT_DEL2:
    DEC A
    JR NZ, PT_DEL2

    DEC DE
    LD A, D
    OR E
    JR NZ, PT_LOOP

    JP (IX)             ; Stackless Return






    ; ==========================================================================
    ; ROM PADDING (Exact 16K binary size for EPROM burning)
    ; ==========================================================================
    DS 0x4000 - $, 0xFF