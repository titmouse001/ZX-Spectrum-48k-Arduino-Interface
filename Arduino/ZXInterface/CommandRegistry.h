#ifndef COMMAND_REGISTRY_H
#define COMMAND_REGISTRY_H

#include "stdint.h"

namespace CommandRegistry {

extern uint16_t command_TransmitKey;
extern uint16_t command_Fill;
extern uint16_t command_SmallFill;
extern uint16_t command_Transfer;
extern uint16_t command_Copy;
extern uint16_t command_Copy32;
extern uint16_t command_VBL_Wait;
extern uint16_t command_Stack;
extern uint16_t command_Execute;
extern uint16_t command_fill_mem_bytecount;

void initialize();

}

#endif
