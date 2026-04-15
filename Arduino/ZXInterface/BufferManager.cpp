#include "BufferManager.h"

namespace BufferManager {

  static uint8_t pool[POOL_SIZE];
  uint16_t poolOffset = 0;

  uint8_t* allocate(size_t size) {
 //   if (poolOffset + size > POOL_SIZE) {
  //    return nullptr; 
  //  }
    // TODO - WARN ... LOCK OUT AND FLASH NANO LED

    uint8_t* ptr = &pool[poolOffset];
    poolOffset += size;
    return ptr;
  }

  uint16_t getMark() {
    return poolOffset;
  }

  void freeToMark(uint16_t mark) {
    poolOffset = mark;
  }

  void resetPool() {
    poolOffset = 0;
  }
}
