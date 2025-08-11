#include <stdint.h>
#include "command_registry.h"
#include "utils.h" 

namespace CommandRegistry {
    uint16_t command_TransmitKey;
    uint16_t command_Fill;
    uint16_t command_SmallFill;
    uint16_t command_Transfer;
    uint16_t command_Copy;
    uint16_t command_Copy32;
    uint16_t command_VBL_Wait;
    uint16_t command_Stack;
    uint16_t command_Execute;

    void initialize() {
        command_TransmitKey = Utils::get16bitPulseValue();
        command_Fill = Utils::get16bitPulseValue();
        command_SmallFill = Utils::get16bitPulseValue();
        command_Transfer = Utils::get16bitPulseValue();
        command_Copy = Utils::get16bitPulseValue();
        command_Copy32 = Utils::get16bitPulseValue();
        command_VBL_Wait = Utils::get16bitPulseValue();
        command_Stack = Utils::get16bitPulseValue();
        command_Execute = Utils::get16bitPulseValue();
    }
}