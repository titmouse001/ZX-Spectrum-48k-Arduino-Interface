#ifndef BUFFER_MANAGER_H
#define BUFFER_MANAGER_H

#include <stdint.h>
#include "smallfont.h"
#include "buffer_config.h"
#include "constants.h"
#include "packet_types.h"

namespace BufferManager {
    extern uint8_t packetBuffer[TOTAL_PACKET_BUFFER_SIZE];
    extern uint8_t head27_Execute[SNA_TOTAL_ITEMS + E(ExecutePacket::PACKET_LEN)];
    extern uint8_t TextBuffer[SmallFont::FNT_BUFFER_SIZE * SmallFont::FNT_HEIGHT];
    
    void initialize();
}

#endif
