#pragma once

#include "Constants.h"

// SNAPSHOT (.SNA) FILE HEADER (27 bytes, no PC value stored):
//      REG_I  =00, REG_HL'=01, REG_DE'=03, REG_BC'=05
//      REG_AF'=07, REG_HL =09, REG_DE =11, REG_BC =13
//      REG_IY =15, REG_IX =17, REG_IFF=19, REG_R  =20
//      REG_AF =21, REG_SP =23, REG_IM =25, REG_BDR=26

namespace Z802SNA {

    // convertHeaders: It takes the Z80 V1 header, and fillout the SNA header.
    // Returns the stack offset to the caller where PC should be pushed.
    uint16_t convertZ80HeaderToSna(const uint8_t* z80Header_v1, uint8_t* snaHeader) {
        // '.Z80' V1 header to '.SNA' header  (SNA header is 27 bytes)
        snaHeader[SNA_I] = z80Header_v1[Z80_V1_I];
        snaHeader[SNA_HL_PRIME_LOW] = z80Header_v1[Z80_V1_HL_PRIME_LOW];
        snaHeader[SNA_HL_PRIME_HIGH] = z80Header_v1[Z80_V1_HL_PRIME_HIGH];
        snaHeader[SNA_DE_PRIME_LOW] = z80Header_v1[Z80_V1_DE_PRIME_LOW];
        snaHeader[SNA_DE_PRIME_HIGH] = z80Header_v1[Z80_V1_DE_PRIME_HIGH];
        snaHeader[SNA_BC_PRIME_LOW] = z80Header_v1[Z80_V1_BC_PRIME_LOW];
        snaHeader[SNA_BC_PRIME_HIGH] = z80Header_v1[Z80_V1_BC_PRIME_HIGH];
        snaHeader[SNA_AF_PRIME_LOW] = z80Header_v1[Z80_V1_AF_PRIME_LOW];
        snaHeader[SNA_AF_PRIME_HIGH] = z80Header_v1[Z80_V1_AF_PRIME_HIGH];
        snaHeader[SNA_HL_LOW] = z80Header_v1[Z80_V1_HL_LOW];
        snaHeader[SNA_HL_HIGH] = z80Header_v1[Z80_V1_HL_HIGH];
        snaHeader[SNA_DE_LOW] = z80Header_v1[Z80_V1_DE_LOW];
        snaHeader[SNA_DE_HIGH] = z80Header_v1[Z80_V1_DE_HIGH];
        snaHeader[SNA_BC_LOW] = z80Header_v1[Z80_V1_BC_LOW];
        snaHeader[SNA_BC_HIGH] = z80Header_v1[Z80_V1_BC_HIGH];
        snaHeader[SNA_IY_LOW] = z80Header_v1[Z80_V1_IY_LOW];
        snaHeader[SNA_IY_HIGH] = z80Header_v1[Z80_V1_IY_HIGH];
        snaHeader[SNA_IX_LOW] = z80Header_v1[Z80_V1_IX_LOW];
        snaHeader[SNA_IX_HIGH] = z80Header_v1[Z80_V1_IX_HIGH];
        snaHeader[SNA_IFF2_BIT] = z80Header_v1[Z80_V1_IFF1]; // SNA IFF2 is Z80 IFF1
        snaHeader[SNA_R_REGISTER] = z80Header_v1[Z80_V1_R_7BITS];
        snaHeader[SNA_AF_LOW] = z80Header_v1[Z80_V1_AF_LOW];
        snaHeader[SNA_AF_HIGH] = z80Header_v1[Z80_V1_AF_HIGH];
        // -----------------------------------------------------------------------------------------
        // Modify stack to use the .SNA format style 
        // -2 as all .sna files by design have a fudged PC placed next in the stack, 
        // this allows the sna format to use the return instruction (RET) to start the game. 
        const uint16_t original_sp = (z80Header_v1[Z80_V1_SP_HIGH] << 8) | z80Header_v1[Z80_V1_SP_LOW];
        const uint16_t new_sp = original_sp - 2;  
        snaHeader[SNA_SP_LOW] = (uint8_t)(new_sp & 0xFF);
        snaHeader[SNA_SP_HIGH] = (uint8_t)(new_sp >> 8);
        // -----------------------------------------------------------------------------------------
        // IM mode is bits 0-1 of IM_AND_FLAGS2
        snaHeader[SNA_IM_MODE] = z80Header_v1[Z80_V1_IM_AND_FLAGS2] & 0b0011; 
        // Border color is bits 1-3 of FLAGS1
        snaHeader[SNA_BORDER_COLOUR] = (z80Header_v1[Z80_V1_FLAGS1] & 0b1110) >> 1; 
        return new_sp;   // Caller will use this to place PC on stack
    }

} // namespace Z802SNA

