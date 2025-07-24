#pragma once

//#include <cstdint>

#include "Constants.h"


namespace Z802SNA {
    // Simulates the function based on its usage in the prototype
    // It takes the Z80 V1 header, and fills the SNA header.
    // Returns the stack offset where PC should be pushed.
    uint16_t convertHeaders(const uint8_t* z80Header_v1, uint8_t* snaHeader) {
        // Copy relevant Z80 V1 header bytes to SNA header
        // SNA header format: I, HL', DE', BC', AF', HL, DE, BC, IY, IX, IFF1, R, AF, SP, IM, Border
        // (SNA header is 27 bytes)

        // Corrected mapping from Z80 V1 header to SNA header based on constants.h
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

        // --- Re-inserted missing SP calculation ---
        uint16_t original_sp = (z80Header_v1[Z80_V1_SP_HIGH] << 8) | z80Header_v1[Z80_V1_SP_LOW];
        uint16_t new_sp = original_sp - 2;
        snaHeader[SNA_SP_LOW] = (uint8_t)(new_sp & 0xFF);
        snaHeader[SNA_SP_HIGH] = (uint8_t)(new_sp >> 8);
        // --- End re-insertion ---

        snaHeader[SNA_IM_MODE] = (z80Header_v1[Z80_V1_IM_AND_FLAGS2] & 0x03); // IM mode is bits 0-1 of IM_AND_FLAGS2
        snaHeader[SNA_BORDER_COLOUR] = (z80Header_v1[Z80_V1_FLAGS1] & 0x0E) >> 1; // Border color is bits 1-3 of FLAGS1

        return (uint16_t)(new_sp); // - 0x4000); // Return offset relative to 0x4000 for PC write
    }
} // namespace Z802SNA

