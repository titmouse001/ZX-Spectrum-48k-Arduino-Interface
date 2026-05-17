#include <stdint.h>
#include "CommandRegistry.h"
#include "Z80Bus.h"
#include "utils.h"

namespace CommandRegistry {

    uint16_t command_TransmitKey;
    uint16_t command_Fill;
    uint16_t command_Transfer;
    uint16_t command_Copy;
    uint16_t command_Copy32;
    uint16_t command_VBL_Wait;
    uint16_t command_Stack;
    uint16_t command_Execute;
    uint16_t command_fill_mem_bytecount;
    uint16_t command_SendData;

    static uint16_t read_IO_Word() {
        uint16_t low = Z80Bus::get_IO_Byte();
        uint16_t high = Z80Bus::get_IO_Byte();
        return low | (high << 8);
    }

    void initialize() {
        command_TransmitKey        = read_IO_Word();
        command_Fill               = read_IO_Word();
        command_Transfer           = read_IO_Word();
        command_Copy               = read_IO_Word();
        command_Copy32             = read_IO_Word();
        command_VBL_Wait           = read_IO_Word();
        command_Stack              = read_IO_Word();
        command_Execute            = read_IO_Word();
        command_fill_mem_bytecount = read_IO_Word();
        command_SendData           = read_IO_Word();
    }
}

