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
uint16_t CommandRegistry::command_fill_mem_bytecount;


void CommandRegistry::initialize(){

  command_TransmitKey = Z80Bus::get_IO_Byte();
  command_TransmitKey += Z80Bus::get_IO_Byte()<< 8 ;

  command_Fill = Z80Bus::get_IO_Byte();
  command_Fill += Z80Bus::get_IO_Byte()<< 8;

  command_SmallFill = Z80Bus::get_IO_Byte();
  command_SmallFill += Z80Bus::get_IO_Byte()<< 8 ;

  command_Transfer = Z80Bus::get_IO_Byte();
  command_Transfer += Z80Bus::get_IO_Byte()<< 8 ;

  command_Copy = Z80Bus::get_IO_Byte();
  command_Copy += Z80Bus::get_IO_Byte()<< 8 ;
  command_Copy32 = Z80Bus::get_IO_Byte();

  command_Copy32 += Z80Bus::get_IO_Byte()<< 8 ;

  command_VBL_Wait = Z80Bus::get_IO_Byte();
  command_VBL_Wait += Z80Bus::get_IO_Byte()<< 8 ;

  command_Stack = Z80Bus::get_IO_Byte();
  command_Stack += Z80Bus::get_IO_Byte()<< 8 ;

  command_Execute = Z80Bus::get_IO_Byte();
  command_Execute += Z80Bus::get_IO_Byte()<< 8 ;

  command_fill_mem_bytecount = Z80Bus::get_IO_Byte();
  command_fill_mem_bytecount += Z80Bus::get_IO_Byte()<< 8;
}
