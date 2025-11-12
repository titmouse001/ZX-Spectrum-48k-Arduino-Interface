#include <stdint.h>
#include "CommandRegistry.h"
#include "Z80Bus.h"
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
uint16_t CommandRegistry::command_FillVariableEven;
uint16_t CommandRegistry::command_FillVariableOdd;

//#include "SSD1306AsciiAvrI2c.h"
//extern SSD1306AsciiAvrI2c oled;

void CommandRegistry::initialize(){

  command_TransmitKey = Z80Bus::get_IO_Byte();
  command_TransmitKey += Z80Bus::get_IO_Byte()<< 8 ;
  //  command_TransmitKey = Utils::get16bitPulseValue();
  //   oled.print(command_TransmitKey,HEX);
  //  delay(1000);

  command_Fill = Z80Bus::get_IO_Byte();
  command_Fill += Z80Bus::get_IO_Byte()<< 8;
  //command_Fill = Utils::get16bitPulseValue();
  //     oled.print(command_Fill);
  //  delay(1000);

  command_SmallFill = Z80Bus::get_IO_Byte();
  command_SmallFill += Z80Bus::get_IO_Byte()<< 8 ;
  // command_SmallFill = Utils::get16bitPulseValue();
  //      oled.print(command_SmallFill);
  //  delay(1000);

  command_Transfer = Z80Bus::get_IO_Byte();
  command_Transfer += Z80Bus::get_IO_Byte()<< 8 ;
  //  command_Transfer = Utils::get16bitPulseValue();
  //  oled.print(",");
  //    oled.print(command_Transfer,HEX);
  //  delay(1000);

  command_Copy = Z80Bus::get_IO_Byte();
  command_Copy += Z80Bus::get_IO_Byte()<< 8 ;
  //command_Copy = Utils::get16bitPulseValue();
  //    oled.print(command_Copy);
  //  delay(1000);
  command_Copy32 = Z80Bus::get_IO_Byte();
  command_Copy32 += Z80Bus::get_IO_Byte()<< 8 ;
  //command_Copy32 = Utils::get16bitPulseValue();
  // delay(1000);

  command_VBL_Wait = Z80Bus::get_IO_Byte();
  command_VBL_Wait += Z80Bus::get_IO_Byte()<< 8 ;
  //command_VBL_Wait = Utils::get16bitPulseValue();
  //  delay(1000);

  command_Stack = Z80Bus::get_IO_Byte();
  command_Stack += Z80Bus::get_IO_Byte()<< 8 ;
  //command_Stack = Utils::get16bitPulseValue();
  // delay(1000);

  command_Execute = Z80Bus::get_IO_Byte();
  command_Execute += Z80Bus::get_IO_Byte()<< 8 ;
  //command_Execute = Utils::get16bitPulseValue();
  // delay(1000);

  command_FillVariableEven = Z80Bus::get_IO_Byte();
  command_FillVariableEven += Z80Bus::get_IO_Byte()<< 8;
  //command_FillVariableEven = Utils::get16bitPulseValue();
  //  delay(1000);

  command_FillVariableOdd = Z80Bus::get_IO_Byte();
  command_FillVariableOdd += Z80Bus::get_IO_Byte()<< 8;
  //command_FillVariableOdd = Utils::get16bitPulseValue();
  ///    oled.print(",");
  //   oled.print(command_FillVariableOdd,HEX);
  // delay(1000);
}
