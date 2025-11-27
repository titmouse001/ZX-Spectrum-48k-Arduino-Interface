#include <stdint.h>
#include "BufferManager.h"

uint8_t BufferManager::packetBuffer[TOTAL_PACKET_BUFFER_SIZE];
uint8_t BufferManager::head27_Execute[SNA_TOTAL_ITEMS + E(ExecutePacket::PACKET_LEN)];
uint8_t BufferManager::TextBuffer[SmallFont::FNT_BUFFER_SIZE * SmallFont::FNT_HEIGHT] = { 0 };

