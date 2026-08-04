#pragma once

#include <stdint.h>
#include "Constants.h"
#include "Utils.h"

// SNAPSHOT (.SNA) FILE HEADER (27 bytes, no PC value stored):
//      REG_I  =00, REG_HL'=01, REG_DE'=03, REG_BC'=05
//      REG_AF'=07, REG_HL =09, REG_DE =11, REG_BC =13
//      REG_IY =15, REG_IX =17, REG_IFF=19, REG_R  =20
//      REG_AF =21, REG_SP =23, REG_IM =25, REG_BDR=26

namespace Z802SNA {

// 30-byte .Z80 Version 1 Header
struct __attribute__((packed)) Z80V1Header {
    uint8_t a, f;             // Bytes 0-1
    uint8_t c, b;             // Bytes 2-3
    uint8_t l, h;             // Bytes 4-5
    uint8_t pc_lo, pc_hi;     // Bytes 6-7
    uint8_t sp_lo, sp_hi;     // Bytes 8-9
    uint8_t i;                // Byte 10
    uint8_t r;                // Byte 11 (Low 7 bits)
    uint8_t flags1;           // Byte 12
    uint8_t e, d;             // Bytes 13-14 (Main DE)
    uint8_t c_prime, b_prime; // Bytes 15-16 (BC')
    uint8_t e_prime, d_prime; // Bytes 17-18 (DE')
    uint8_t l_prime, h_prime; // Bytes 19-20 (HL')
    uint8_t a_prime, f_prime; // Bytes 21-22 (AF')
    uint8_t iyl, iyh;         // Bytes 23-24
    uint8_t ixl, ixh;         // Bytes 25-26
    uint8_t iff1;             // Byte 27
    uint8_t iff2;             // Byte 28
    uint8_t flags2;           // Byte 29
};
static_assert(sizeof(Z80V1Header) == 30, "Z80V1Header must be exactly 30 bytes");


    // convertHeaders: It takes the Z80 V1 header, and fillout the SNA header.
    // Returns the stack offset to the caller where PC should be pushed.
    
//     void convertZ80HeaderToSna(const uint8_t* z80Header_v1, Z80Registers* regs) {

//  //       SNAHeader* snaHeader = &regs->header;
//         uint8_t* snaHeader =(uint8_t*) &regs->header;

//         // '.Z80' V1 header to '.SNA' header  (SNA header is 27 bytes)
//         snaHeader[SNA_I] = z80Header_v1[Z80_V1_I];
//         snaHeader[SNA_HL_PRIME_LOW] = z80Header_v1[Z80_V1_HL_PRIME_LOW];
//         snaHeader[SNA_HL_PRIME_HIGH] = z80Header_v1[Z80_V1_HL_PRIME_HIGH];
//         snaHeader[SNA_DE_PRIME_LOW] = z80Header_v1[Z80_V1_DE_PRIME_LOW];
//         snaHeader[SNA_DE_PRIME_HIGH] = z80Header_v1[Z80_V1_DE_PRIME_HIGH];
//         snaHeader[SNA_BC_PRIME_LOW] = z80Header_v1[Z80_V1_BC_PRIME_LOW];
//         snaHeader[SNA_BC_PRIME_HIGH] = z80Header_v1[Z80_V1_BC_PRIME_HIGH];
//         snaHeader[SNA_AF_PRIME_LOW] = z80Header_v1[Z80_V1_AF_PRIME_LOW];
//         snaHeader[SNA_AF_PRIME_HIGH] = z80Header_v1[Z80_V1_AF_PRIME_HIGH];
//         snaHeader[SNA_HL_LOW] = z80Header_v1[Z80_V1_HL_LOW];
//         snaHeader[SNA_HL_HIGH] = z80Header_v1[Z80_V1_HL_HIGH];
//         snaHeader[SNA_DE_LOW] = z80Header_v1[Z80_V1_DE_LOW];
//         snaHeader[SNA_DE_HIGH] = z80Header_v1[Z80_V1_DE_HIGH];
//         snaHeader[SNA_BC_LOW] = z80Header_v1[Z80_V1_BC_LOW];
//         snaHeader[SNA_BC_HIGH] = z80Header_v1[Z80_V1_BC_HIGH];
//         snaHeader[SNA_IY_LOW] = z80Header_v1[Z80_V1_IY_LOW];
//         snaHeader[SNA_IY_HIGH] = z80Header_v1[Z80_V1_IY_HIGH];
//         snaHeader[SNA_IX_LOW] = z80Header_v1[Z80_V1_IX_LOW];
//         snaHeader[SNA_IX_HIGH] = z80Header_v1[Z80_V1_IX_HIGH];
//         snaHeader[SNA_IFF2_BIT] = z80Header_v1[Z80_V1_IFF1]; // SNA IFF2 is Z80 IFF1
//         snaHeader[SNA_R_REGISTER] = z80Header_v1[Z80_V1_R_7BITS];
//         snaHeader[SNA_AF_LOW] = z80Header_v1[Z80_V1_AF_LOW];
//         snaHeader[SNA_AF_HIGH] = z80Header_v1[Z80_V1_AF_HIGH];
//         snaHeader[SNA_SP_LOW] = z80Header_v1[Z80_V1_SP_LOW];
//         snaHeader[SNA_SP_HIGH] = z80Header_v1[Z80_V1_SP_HIGH];
//         // IM mode is bits 0-1 of IM_AND_FLAGS2
//         snaHeader[SNA_IM_MODE] = z80Header_v1[Z80_V1_IM_AND_FLAGS2] & 0b0011; 
//         // Border color is bits 1-3 of FLAGS1
//         snaHeader[SNA_BORDER_COLOUR] = (z80Header_v1[Z80_V1_FLAGS1] & 0b1110) >> 1; 
//     }

   
//  inline void convertZ80HeaderToSna(const uint8_t* z80Header_v1, Z80Registers* regs) {
//     SNAHeader* snaHeader = &regs->header;

//     // '.Z80' V1 header to '.SNA' header (SNA header is 27 bytes)
//     snaHeader->i           = z80Header_v1[Z80_V1_I];
//     snaHeader->l_prime     = z80Header_v1[Z80_V1_HL_PRIME_LOW];
//     snaHeader->h_prime     = z80Header_v1[Z80_V1_HL_PRIME_HIGH];
//     snaHeader->e_prime     = z80Header_v1[Z80_V1_DE_PRIME_LOW];
//     snaHeader->d_prime     = z80Header_v1[Z80_V1_DE_PRIME_HIGH];
//     snaHeader->c_prime     = z80Header_v1[Z80_V1_BC_PRIME_LOW];
//     snaHeader->b_prime     = z80Header_v1[Z80_V1_BC_PRIME_HIGH];
//     snaHeader->f_prime     = z80Header_v1[Z80_V1_AF_PRIME_LOW];
//     snaHeader->a_prime     = z80Header_v1[Z80_V1_AF_PRIME_HIGH];
//     snaHeader->l           = z80Header_v1[Z80_V1_HL_LOW];
//     snaHeader->h           = z80Header_v1[Z80_V1_HL_HIGH];
//     snaHeader->e           = z80Header_v1[Z80_V1_DE_LOW];
//     snaHeader->d           = z80Header_v1[Z80_V1_DE_HIGH];
//     snaHeader->c           = z80Header_v1[Z80_V1_BC_LOW];
//     snaHeader->b           = z80Header_v1[Z80_V1_BC_HIGH];
//     snaHeader->iyl         = z80Header_v1[Z80_V1_IY_LOW];
//     snaHeader->iyh         = z80Header_v1[Z80_V1_IY_HIGH];
//     snaHeader->ixl         = z80Header_v1[Z80_V1_IX_LOW];
//     snaHeader->ixh         = z80Header_v1[Z80_V1_IX_HIGH];
//     snaHeader->iff2        = z80Header_v1[Z80_V1_IFF1]; // SNA IFF2 is Z80 IFF1
//     snaHeader->r           = z80Header_v1[Z80_V1_R_7BITS];
//     snaHeader->f           = z80Header_v1[Z80_V1_AF_LOW];
//     snaHeader->a           = z80Header_v1[Z80_V1_AF_HIGH];
//     snaHeader->sp_lo       = z80Header_v1[Z80_V1_SP_LOW];
//     snaHeader->sp_hi       = z80Header_v1[Z80_V1_SP_HIGH];
    
//     // IM mode is bits 0-1 of IM_AND_FLAGS2
//     snaHeader->im          = z80Header_v1[Z80_V1_IM_AND_FLAGS2] & 0b0011; 
    
//     // Border color is bits 1-3 of FLAGS1
//     snaHeader->borderCol   = (z80Header_v1[Z80_V1_FLAGS1] & 0b1110) >> 1; 
// }

inline void convertZ80HeaderToSna(const Z80V1Header* z80Header_v1, Z80Registers* regs) {
    SNAHeader* snaHeader = &regs->header;

    snaHeader->i         = z80Header_v1->i;
    snaHeader->l_prime   = z80Header_v1->l_prime;
    snaHeader->h_prime   = z80Header_v1->h_prime;
    snaHeader->e_prime   = z80Header_v1->e_prime;
    snaHeader->d_prime   = z80Header_v1->d_prime;
    snaHeader->c_prime   = z80Header_v1->c_prime;
    snaHeader->b_prime   = z80Header_v1->b_prime;
    snaHeader->f_prime   = z80Header_v1->f_prime;
    snaHeader->a_prime   = z80Header_v1->a_prime;
    snaHeader->l         = z80Header_v1->l;
    snaHeader->h         = z80Header_v1->h;
    snaHeader->e         = z80Header_v1->e;
    snaHeader->d         = z80Header_v1->d;
    snaHeader->c         = z80Header_v1->c;
    snaHeader->b         = z80Header_v1->b;
    snaHeader->iyl       = z80Header_v1->iyl;
    snaHeader->iyh       = z80Header_v1->iyh;
    snaHeader->ixl       = z80Header_v1->ixl;
    snaHeader->ixh       = z80Header_v1->ixh;
    
    // Active interrupt flip-flop
    snaHeader->iff2      = z80Header_v1->iff1;
    
    // Combine 7-bit R register with bit 7 from flags1
    snaHeader->r         = (z80Header_v1->r & 0x7F) | ((z80Header_v1->flags1 & 0x01) << 7);
    
    snaHeader->f         = z80Header_v1->f;
    snaHeader->a         = z80Header_v1->a;
    snaHeader->sp_lo     = z80Header_v1->sp_lo;
    snaHeader->sp_hi     = z80Header_v1->sp_hi;
    
    // Bitfield masking for IM mode and border color
    snaHeader->im        = z80Header_v1->flags2 & 0x03; 
    snaHeader->borderCol = (z80Header_v1->flags1 >> 1) & 0x07; 

    // Store Program Counter in regs container
   // regs->pc_lo          = z80Header_v1->pc_lo;
   // regs->pc_hi          = z80Header_v1->pc_hi;
}

} // namespace Z802SNA

