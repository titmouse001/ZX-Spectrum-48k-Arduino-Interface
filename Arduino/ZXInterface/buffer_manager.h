#ifndef BUFFER_MANAGER_H
#define BUFFER_MANAGER_H

#include <stdint.h>
#include "constants.h"
#include "packet_types.h"

constexpr uint16_t COMMAND_PAYLOAD_SECTION_SIZE = 255;
constexpr uint16_t FILE_READ_BUFFER_SIZE = 128;
constexpr uint16_t FILE_READ_BUFFER_OFFSET = 5 + COMMAND_PAYLOAD_SECTION_SIZE;
constexpr uint16_t TOTAL_PACKET_BUFFER_SIZE = 5 + COMMAND_PAYLOAD_SECTION_SIZE + FILE_READ_BUFFER_SIZE;

namespace BufferManager {
    extern uint8_t packetBuffer[TOTAL_PACKET_BUFFER_SIZE];
    extern uint8_t head27_Execute[SNA_TOTAL_ITEMS + E(ExecutePacket::PACKET_LEN)];
    extern uint8_t TextBuffer[SmallFont::FNT_BUFFER_SIZE * SmallFont::FNT_HEIGHT];
    
    void initialize();
}

#endif
