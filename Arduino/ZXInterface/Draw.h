#ifndef DRAW_H
#define DRAW_H

#include "packet_builder.h"
#include "Z80Bus.h"

namespace Draw {
  
constexpr uint16_t SKIP_CMD_ADDR = E(Copy32Packet::CMD_LOW) + 1;

// textLine:  Draws a full-width line of text (32 bytes per line).
// Optimised to draw menu text row-by-row to the screen memory.
void textLine(int ypos, const char *message) {
  // For ZX Spectrum, moving down 1 pixel within same character row adds 0x0100
  constexpr uint16_t PIX_INC = 0x0100;                   // +256 for next pixel line
  constexpr uint8_t packet_data_size = E(Copy32Packet::PACKET_LEN) + SmallFont::FNT_BUFFER_SIZE;
  constexpr uint16_t screen_width_offset = ZX_SCREEN_WIDTH_BYTES - (7 * PIX_INC);
  
  SmallFont::prepareTextGraphics(BufferManager::TextBuffer, message);  // Pre-render text into TextBuffer
  uint8_t *const packetBase = BufferManager::packetBuffer;
  packetBase[E(Copy32Packet::CMD_HIGH)] = (uint8_t)(CommandRegistry::command_Copy32 >> 8);
  packetBase[E(Copy32Packet::CMD_LOW)] = (uint8_t)(CommandRegistry::command_Copy32 & 0xFF);
  
  uint16_t destAddr = Utils::zx_spectrum_screen_address(ypos);
  const uint8_t *outputLine = BufferManager::TextBuffer; 

  for (uint8_t y = 0; y < SmallFont::FNT_HEIGHT; ++y) {
    uint8_t *packet = &packetBase[SKIP_CMD_ADDR];  // reset
    *packet++ = (uint8_t)(destAddr >> 8);
    *packet++ = (uint8_t)(destAddr & 0xFF);
    const uint8_t *src = outputLine;
    for (uint8_t i = 0; i < SmallFont::FNT_BUFFER_SIZE; ++i) {
      *packet++ = *src++;
    }

    Z80Bus::sendBytes(packetBase, packet_data_size);
    outputLine += SmallFont::FNT_BUFFER_SIZE;
    if ((y & 0x07) == 0x07) {
      destAddr += screen_width_offset;      // Every 8 pixels, handle interleaved layout
    } else {
      destAddr += PIX_INC;
    }
  }
}

// text - text in flash memory: Draws only the required part of the screen buffer.
// Slower but useful for general-purpose text drawing.
void text_P(int xpos, int ypos, const __FlashStringHelper *flashStr) {
  const uint8_t charCount = SmallFont::prepareTextGraphics_P(BufferManager::TextBuffer, flashStr);
  const uint8_t byteCount = ((charCount * (SmallFont::FNT_WIDTH + SmallFont::FNT_GAP)) + 7) / 8;  // byte alignment
  
  uint8_t *outputLine = BufferManager::TextBuffer;
  uint8_t *packet = BufferManager::packetBuffer;
  *packet++ = (uint8_t)(CommandRegistry::command_Copy >> 8);
  *packet++ = (uint8_t)(CommandRegistry::command_Copy & 0xFF);
  
  for (uint8_t y = 0; y < SmallFont::FNT_HEIGHT; y++, outputLine += SmallFont::FNT_BUFFER_SIZE) {
    const uint16_t destAddr = Utils::zx_spectrum_screen_address(xpos, ypos + y);
    packet = &BufferManager::packetBuffer[SKIP_CMD_ADDR];
    *packet++ = (uint8_t)byteCount;
    *packet++ = (uint8_t)(destAddr >> 8);
    *packet++ = (uint8_t)(destAddr & 0xFF);
    
    for (uint8_t i = 0; i < byteCount; ++i) { *packet++ = *outputLine++; }
    
    Z80Bus::sendBytes(BufferManager::packetBuffer, 5 + byteCount);
  }
}

// text: Draws only the required part of the screen buffer.
// Slower but useful for general-purpose text drawing.
void text(int xpos, int ypos, const char *message) {
  const uint8_t charCount = SmallFont::prepareTextGraphics(BufferManager::TextBuffer, message);
  const uint8_t byteCount = ((charCount * (SmallFont::FNT_WIDTH + SmallFont::FNT_GAP)) + 7) / 8;  // byte alignment
  
  uint8_t *outputLine = BufferManager::TextBuffer;
  uint8_t *packet = BufferManager::packetBuffer;
  *packet++ = (uint8_t)(CommandRegistry::command_Copy >> 8);
  *packet++ = (uint8_t)(CommandRegistry::command_Copy & 0xFF);
  
  for (uint8_t y = 0; y < SmallFont::FNT_HEIGHT; y++, outputLine += SmallFont::FNT_BUFFER_SIZE) {
    const uint16_t destAddr = Utils::zx_spectrum_screen_address(xpos, ypos + y);
    packet = &BufferManager::packetBuffer[SKIP_CMD_ADDR];
    *packet++ = (uint8_t)byteCount;
    *packet++ = (uint8_t)(destAddr >> 8);
    *packet++ = (uint8_t)(destAddr & 0xFF);
    
    for (uint8_t i = 0; i < byteCount; ++i) { *packet++ = *outputLine++; }
    
    Z80Bus::sendBytes(BufferManager::packetBuffer, 5 + byteCount);
  }
}

//  // textLine:  Draws a full-width line of text (32 bytes per line).
//  // Optimised to draw menu text row-by-row
//  __attribute__((optimize("-Ofast"))) 
// void textLine(int ypos, const char *message) {
//   // For ZX Spectrum, moving down 1 pixel within same character row adds 0x0100
//   constexpr uint16_t pixel_increment = 0x0100;  // +256 for next pixel line
  
//   SmallFont::prepareTextGraphics(PacketBuilder::TextBuffer, message);  // Pre-render text into TextBuffer

//   PacketBuilder::packetBuffer[0] = (uint8_t)((PacketBuilder::command_Copy32) >> 8); 
//   PacketBuilder::packetBuffer[1] = (uint8_t)((PacketBuilder::command_Copy32) & 0xFF); 
  
//   uint16_t destAddr = Utils::zx_spectrum_screen_address(ypos);
//   uint8_t *outputLine = PacketBuilder::TextBuffer;
//   for (uint8_t y = 0; y < SmallFont::FNT_HEIGHT; ++y) {
//     PacketBuilder::packetBuffer[2] = (uint8_t)(destAddr >> 8); 
//     PacketBuilder::packetBuffer[3] = (uint8_t)(destAddr & 0xFF); 
//     memcpy(&PacketBuilder::packetBuffer[4], outputLine, SmallFont::FNT_BUFFER_SIZE);
//     Z80Bus::sendBytes(PacketBuilder::packetBuffer, E(Copy32Packet::PACKET_LEN) + SmallFont::FNT_BUFFER_SIZE);
//     outputLine += SmallFont::FNT_BUFFER_SIZE;
//     if ((y & 0x07) == 0x07) {
//       // Every 8 pixels, we need to handle the interleaved layout
//       destAddr += ZX_SCREEN_WIDTH_BYTES - (7 * pixel_increment);  
//     } else {
//       destAddr += pixel_increment;
//     }
//   }
// }

// // text - text in flash memory: Draws only the required part of the screen buffer.
// // Slower but useful for general-purpose text drawing.
// void text_P(int xpos, int ypos, const __FlashStringHelper *flashStr) {
//   const uint8_t charCount = SmallFont::prepareTextGraphics_P(PacketBuilder::TextBuffer, flashStr);
//   const uint8_t byteCount = ((charCount * (SmallFont::FNT_WIDTH + SmallFont::FNT_GAP)) + 7) / 8;  // byte alignment
//   uint8_t *outputLine = PacketBuilder::TextBuffer;
//   PacketBuilder::packetBuffer[0] = (uint8_t)((PacketBuilder::command_Copy) >> 8); 
//   PacketBuilder::packetBuffer[1] = (uint8_t)((PacketBuilder::command_Copy)&0xFF); 
//   for (uint8_t y = 0; y < SmallFont::FNT_HEIGHT; y++, outputLine += SmallFont::FNT_BUFFER_SIZE) {
//     const uint16_t destAddr  = Utils::zx_spectrum_screen_address(xpos, ypos + y);
//    PacketBuilder:: packetBuffer[2] = (uint8_t)byteCount ; 
//     PacketBuilder::packetBuffer[3] = (uint8_t)((destAddr) >> 8); 
//     PacketBuilder::packetBuffer[4] = (uint8_t)((destAddr)&0xFF); 
//     memcpy(&PacketBuilder::packetBuffer[5], outputLine, byteCount);
//     Z80Bus::sendBytes(PacketBuilder::packetBuffer, 5 + byteCount);
//   }
// } 
// // text: Draws only the required part of the screen buffer.
// // Slower but useful for general-purpose text drawing.
// void text(int xpos, int ypos, const char *message) {
//   const uint8_t charCount = SmallFont::prepareTextGraphics(PacketBuilder::TextBuffer, message);
//   const uint8_t byteCount = ((charCount * (SmallFont::FNT_WIDTH + SmallFont::FNT_GAP)) + 7) / 8;  // byte alignment
//   uint8_t *outputLine = PacketBuilder::TextBuffer;
//   PacketBuilder::packetBuffer[0] = (uint8_t)((PacketBuilder::command_Copy) >> 8); 
//   PacketBuilder::packetBuffer[1] = (uint8_t)((PacketBuilder::command_Copy)&0xFF); 
//   for (uint8_t y = 0; y < SmallFont::FNT_HEIGHT; y++, outputLine += SmallFont::FNT_BUFFER_SIZE) {
//     const uint16_t destAddr  = Utils::zx_spectrum_screen_address(xpos, ypos + y);
//     PacketBuilder::packetBuffer[2] = (uint8_t)byteCount ; 
//     PacketBuilder::packetBuffer[3] = (uint8_t)((destAddr) >> 8); 
//     PacketBuilder::packetBuffer[4] = (uint8_t)((destAddr)&0xFF); 
//     memcpy(&PacketBuilder::packetBuffer[5], outputLine, byteCount);
//     Z80Bus::sendBytes(PacketBuilder::packetBuffer, 5 + byteCount);
//   }
// } 

}

#endif