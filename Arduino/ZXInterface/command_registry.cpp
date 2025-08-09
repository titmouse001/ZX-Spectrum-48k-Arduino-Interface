#include "command_registry.h"
#include "utils.h" 

namespace CommandRegistry {
    uint16_t command_TransmitKey;
    uint16_t command_Fill;
    uint16_t command_SmallFill;
    uint16_t command_Transfer;
    uint16_t command_Copy;
    uint16_t command_Copy32;
    uint16_t command_Wait;
    uint16_t command_Stack;
    uint16_t command_Execute;

    void initialize() {
        command_TransmitKey = Utils::GetValueFromPulseStream();
        command_Fill = Utils::GetValueFromPulseStream();
        command_SmallFill = Utils::GetValueFromPulseStream();
        command_Transfer = Utils::GetValueFromPulseStream();
        command_Copy = Utils::GetValueFromPulseStream();
        command_Copy32 = Utils::GetValueFromPulseStream();
        command_Wait = Utils::GetValueFromPulseStream();
        command_Stack = Utils::GetValueFromPulseStream();
        command_Execute = Utils::GetValueFromPulseStream();
    }
}