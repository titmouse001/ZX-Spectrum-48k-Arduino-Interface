#include <stdint.h>
#include "CommandRegistry.h"
#include "Z80Bus.h"
#include "utils.h"

namespace CommandRegistry {

    // NOTE: FUTURE ME - ARE YOU RUNNNING OUT OF VARIABLE SPACE... 
    //       !!!MAYBE HARDCODE THESE!!! OR VECTOR TABLE Z80 SIDE
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


    // ORG $00D0
	// jp command_TransmitKey			;1  (offset +0)
	// jp command_Fill					;2  (offset +3)
	// jp command_Transfer				;3  (offset +6)
	// jp command_Copy					;4	...
	// jp command_Copy32				;5
	// jp command_VBL_Wait				;6
	// jp command_Stack				    ;7
	// jp command_Execute				;8
	// jp command_fill_mem_bytecount	;9
	// jp command_SendData				;10

    // API: At startup, we read the location of each support method on the Z80 side,
    // as these memory locations will change, be added, or be deleted over time.

    constexpr uint16_t CMD_BASE = 0x00D0;
    constexpr uint16_t cmd_addr(uint8_t n) {
         return CMD_BASE + (3 * n); 
        }

    void initialize() {
      //  NOP        = cmd_addr(0);
        command_TransmitKey        = cmd_addr(1);
        command_Fill               = cmd_addr(2);
        command_Transfer           = cmd_addr(3);
        command_Copy               = cmd_addr(4);
        command_Copy32             = cmd_addr(5);
        command_VBL_Wait           = cmd_addr(6);
        command_Stack              = cmd_addr(7);
        command_Execute            = cmd_addr(8);
        command_fill_mem_bytecount = cmd_addr(9);
        command_SendData           = cmd_addr(10);

        Z80Bus::waitHalt_syncWithZ80(); Z80Bus::triggerZ80NMI();Z80Bus::waitHalt_syncWithZ80();Z80Bus::triggerZ80NMI();
        Z80Bus::waitHalt_syncWithZ80(); Z80Bus::triggerZ80NMI();Z80Bus::waitHalt_syncWithZ80();Z80Bus::triggerZ80NMI();
        Z80Bus::waitHalt_syncWithZ80(); Z80Bus::triggerZ80NMI();Z80Bus::waitHalt_syncWithZ80();Z80Bus::triggerZ80NMI();
        Z80Bus::waitHalt_syncWithZ80(); Z80Bus::triggerZ80NMI();Z80Bus::waitHalt_syncWithZ80();Z80Bus::triggerZ80NMI();
        Z80Bus::waitHalt_syncWithZ80(); Z80Bus::triggerZ80NMI();Z80Bus::waitHalt_syncWithZ80();Z80Bus::triggerZ80NMI();
        Z80Bus::waitHalt_syncWithZ80(); Z80Bus::triggerZ80NMI();Z80Bus::waitHalt_syncWithZ80();Z80Bus::triggerZ80NMI();
        Z80Bus::waitHalt_syncWithZ80(); Z80Bus::triggerZ80NMI();Z80Bus::waitHalt_syncWithZ80();Z80Bus::triggerZ80NMI();
        Z80Bus::waitHalt_syncWithZ80(); Z80Bus::triggerZ80NMI();Z80Bus::waitHalt_syncWithZ80();Z80Bus::triggerZ80NMI();
        Z80Bus::waitHalt_syncWithZ80(); Z80Bus::triggerZ80NMI();Z80Bus::waitHalt_syncWithZ80();Z80Bus::triggerZ80NMI();
        Z80Bus::waitHalt_syncWithZ80(); Z80Bus::triggerZ80NMI();Z80Bus::waitHalt_syncWithZ80();Z80Bus::triggerZ80NMI();
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

