#include <stdint.h>
#include "buffer_manager.h"
#include "constants.h"
#include "command_registry.h"

namespace BufferManager {
    uint8_t packetBuffer[TOTAL_PACKET_BUFFER_SIZE];
    uint8_t head27_Execute[SNA_TOTAL_ITEMS + E(ExecutePacket::PACKET_LEN)];
    uint8_t TextBuffer[SmallFont::FNT_BUFFER_SIZE * SmallFont::FNT_HEIGHT] = { 0 };
    
    void initialize() {
      // head27_Execute[E(ExecutePacket::CMD_HIGH)] = (uint8_t)((CommandRegistry::command_Execute) >> 8);
      // head27_Execute[E(ExecutePacket::CMD_LOW)] = (uint8_t)((CommandRegistry::command_Execute)&0xFF);
    }
}