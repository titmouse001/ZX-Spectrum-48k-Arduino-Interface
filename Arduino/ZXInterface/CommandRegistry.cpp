#include <stdint.h>
#include "CommandRegistry.h"
#include "utils.h"

uint16_t CommandRegistry::command_TransmitKey;
uint16_t CommandRegistry::command_Fill;
uint16_t CommandRegistry::command_SmallFill;
uint16_t CommandRegistry::command_Transfer;
uint16_t CommandRegistry::command_Copy;
uint16_t CommandRegistry::command_Copy32;
uint16_t CommandRegistry::command_VBL_Wait;
uint16_t CommandRegistry::command_Stack;
uint16_t CommandRegistry::command_Execute;

void CommandRegistry::initialize() {
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
