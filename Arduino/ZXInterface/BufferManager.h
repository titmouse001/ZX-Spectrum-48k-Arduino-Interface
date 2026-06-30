#ifndef BUFFER_MANAGER_H
#define BUFFER_MANAGER_H

#include <stdint.h>
#include <stddef.h>

constexpr uint16_t POOL_SIZE = 450;
// seeing  449
  
namespace BufferManager {
  extern uint8_t pool[POOL_SIZE];
  extern uint16_t poolOffset;
  extern uint16_t poolOffsetLastMax;  // BEBUG ONLY

  uint8_t* allocate(size_t size);
  uint16_t getMark();
  void freeToMark(uint16_t mark);
  void resetPool();
}

#endif

