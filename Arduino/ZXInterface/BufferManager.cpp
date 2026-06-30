#include "BufferManager.h"

#include "Constants.h"
#include "digitalWriteFast.h"
#include "utils.h"

namespace BufferManager {

uint8_t pool[];
uint16_t poolOffset = 0;
#ifdef _DEBUG_POOL_SIZE_ENABLED_
uint16_t poolOffsetLastMax = 0;  // BEBUG ONLY
#endif

uint8_t* allocate(size_t size) {

#ifdef _DEBUG_POOL_SIZE_ENABLED_
  if (poolOffset + size > POOL_SIZE) {
    pinModeFast(13, OUTPUT);  // LED
    pinModeFast(1, OUTPUT);   // TX
    while (true) {
      digitalWrite(LED_BUILTIN, LOW);
      digitalWrite(1, HIGH);
      Utils::delay16(250);
      digitalWrite(1, LOW);
      digitalWrite(LED_BUILTIN, HIGH);
      Utils::delay16(250);
    }
  }
#endif

  uint8_t* ptr = &pool[poolOffset];
  poolOffset += size;

#ifdef _DEBUG_POOL_SIZE_ENABLED_
  if (poolOffset > poolOffsetLastMax) {
    poolOffsetLastMax = poolOffset;
  } 
#endif

  return ptr;
}

uint16_t getMark() { return poolOffset; }
void freeToMark(uint16_t mark) { poolOffset = mark; }
void resetPool() { poolOffset = 0; }

}  // namespace BufferManager
