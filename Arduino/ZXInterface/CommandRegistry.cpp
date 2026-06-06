#include <stdint.h>
#include "CommandRegistry.h"
#include "Z80Bus.h"
#include "utils.h"

namespace CommandRegistry {

    // NOTE: FUTURE ME - ARE YOU RUNNNING OUT OF VARIABLE SPACE... 
    //       !!!MAYBE HARDCODE THESE!!!
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

   // API: At startup, we read the location of each support method on the Z80 side,
   // as these memory locations will change, be added, or be deleted over time.
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

 //   BUG HUNT... THIS IS GOOD - PROVED OK WITH HARDCODE ADDRESS (...LATER THE ISSUE (128k machines not showing menu!!!) FOUND DUE TO BAD FILL (using CPI) ON Z80 SIDE!!!)
 //   NOTE: REFESH FROM "SnaLauncher.lst" file .
    // void initialize() {
        // these values will be stale... 
    //     command_TransmitKey        = 0x0122; Z80Bus::waitHalt_syncWithZ80(); Z80Bus::triggerZ80NMI();Z80Bus::waitHalt_syncWithZ80();Z80Bus::triggerZ80NMI();
    //     command_Fill               = 0x012B; Z80Bus::waitHalt_syncWithZ80(); Z80Bus::triggerZ80NMI();Z80Bus::waitHalt_syncWithZ80(); Z80Bus::triggerZ80NMI();
    //     command_Transfer           = 0x0168; Z80Bus::waitHalt_syncWithZ80(); Z80Bus::triggerZ80NMI();Z80Bus::waitHalt_syncWithZ80(); Z80Bus::triggerZ80NMI();
    //     command_Copy               = 0x0180; Z80Bus::waitHalt_syncWithZ80(); Z80Bus::triggerZ80NMI();Z80Bus::waitHalt_syncWithZ80(); Z80Bus::triggerZ80NMI();
    //     command_Copy32             = 0x0192; Z80Bus::waitHalt_syncWithZ80(); Z80Bus::triggerZ80NMI();Z80Bus::waitHalt_syncWithZ80(); Z80Bus::triggerZ80NMI();
    //     command_VBL_Wait           = 0x01FB; Z80Bus::waitHalt_syncWithZ80(); Z80Bus::triggerZ80NMI();Z80Bus::waitHalt_syncWithZ80(); Z80Bus::triggerZ80NMI();
    //     command_Stack              = 0x0202; Z80Bus::waitHalt_syncWithZ80(); Z80Bus::triggerZ80NMI();Z80Bus::waitHalt_syncWithZ80(); Z80Bus::triggerZ80NMI();
    //     command_Execute            = 0x0220; Z80Bus::waitHalt_syncWithZ80(); Z80Bus::triggerZ80NMI();Z80Bus::waitHalt_syncWithZ80(); Z80Bus::triggerZ80NMI();
    //     command_fill_mem_bytecount = 0x00FA; Z80Bus::waitHalt_syncWithZ80(); Z80Bus::triggerZ80NMI();Z80Bus::waitHalt_syncWithZ80(); Z80Bus::triggerZ80NMI();
    //     command_SendData           = 0x00DD; Z80Bus::waitHalt_syncWithZ80(); Z80Bus::triggerZ80NMI();Z80Bus::waitHalt_syncWithZ80(); Z80Bus::triggerZ80NMI();
    // }
}

