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
        command_TransmitKey = Utils::readPulseEncodedValue();
        command_Fill = Utils::readPulseEncodedValue();
        command_SmallFill = Utils::readPulseEncodedValue();
        command_Transfer = Utils::readPulseEncodedValue();
        command_Copy = Utils::readPulseEncodedValue();
        command_Copy32 = Utils::readPulseEncodedValue();
        command_VBL_Wait = Utils::readPulseEncodedValue();
        command_Stack = Utils::readPulseEncodedValue();
        command_Execute = Utils::readPulseEncodedValue();
    }
}