#ifndef BUFFER_MANAGER_H
#define BUFFER_MANAGER_H

#include <stdint.h>
#include <stddef.h>
#include "PacketTypes.h"
#include "Constants.h"

namespace BufferManager {

// MAX ALLOC ... DEBUG SEEING : 457
  constexpr uint16_t POOL_SIZE = 255+128+7+128; //(32*7)+7+255; 
 // constexpr uint16_t POOL_SIZE =512;

//  Sketch uses 28140 bytes (91%) of program storage space. Maximum is 30720 bytes.
//Global variables use 1380 bytes (67%) of dynamic memory, leaving 668 bytes for 

  // TODO ... find best value for above ... go deep capture max value of mark

extern uint16_t poolOffsetLastMax ;

  uint8_t* allocate(size_t size);
  uint16_t getMark();
  void freeToMark(uint16_t mark);
  void resetPool();
}

#endif

