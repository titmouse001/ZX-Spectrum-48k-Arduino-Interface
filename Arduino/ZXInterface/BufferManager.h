#ifndef BUFFER_MANAGER_H
#define BUFFER_MANAGER_H

#include <stdint.h>
#include <stddef.h>

namespace BufferManager {
  constexpr uint16_t POOL_SIZE = 255+128+7+128; 

  extern uint16_t poolOffsetLastMax ; // DEBUG 

  uint8_t* allocate(size_t size);
  uint16_t getMark();
  void freeToMark(uint16_t mark);
  void resetPool();
}

#endif

