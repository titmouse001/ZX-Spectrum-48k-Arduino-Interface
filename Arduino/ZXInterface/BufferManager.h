#ifndef BUFFER_MANAGER_H
#define BUFFER_MANAGER_H

#include <stdint.h>
#include <stddef.h>
#include "PacketTypes.h"
#include "Constants.h"

namespace BufferManager {

  constexpr uint16_t POOL_SIZE = 255+128+7+128; //(32*7)+7+255; 

  // TODO ... find best value for above ... go deep capture max value of mark


  uint8_t* allocate(size_t size);
  uint16_t getMark();
  void freeToMark(uint16_t mark);
  void resetPool();
}

#endif

