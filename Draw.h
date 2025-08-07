#ifndef DRAW_H
#define DRAW_H

namespace Draw {
  
 // textLine:  Draws a full-width line of text (32 bytes per line).
 // Optimised to draw menu text row-by-row to the screen memory.
__attribute__((optimize("-Ofast"))) 
void textLine(int ypos, const char *message) {
  SmallFont::prepareTextGraphics(TextBuffer, message);  // Pre-render text into TextBuffer
  uint8_t *outputLine = TextBuffer;
  packetBuffer[0] = (uint8_t)((Buffers::command_Copy32) >> 8); 
  packetBuffer[1] = (uint8_t)((Buffers::command_Copy32)&0xFF); 
  for (uint8_t y = 0; y < SmallFont::FNT_HEIGHT; ++y, outputLine += SmallFont::FNT_BUFFER_SIZE) {
    const uint16_t destAddr  = Utils::zx_spectrum_screen_address(0, ypos + y);
    packetBuffer[2] = (uint8_t)((destAddr) >> 8); 
    packetBuffer[3] = (uint8_t)((destAddr)&0xFF); 
    memcpy(&packetBuffer[4], outputLine, SmallFont::FNT_BUFFER_SIZE);
    Z80Bus::sendBytes(packetBuffer, 4  + SmallFont::FNT_BUFFER_SIZE);
  }
}

// text - text in flash memory: Draws only the required part of the screen buffer.
// Slower but useful for general-purpose text drawing.
void text_P(int xpos, int ypos, const __FlashStringHelper *flashStr) {
  const uint8_t charCount = SmallFont::prepareTextGraphics_P(TextBuffer, flashStr);
  const uint8_t byteCount = ((charCount * (SmallFont::FNT_WIDTH + SmallFont::FNT_GAP)) + 7) / 8;  // byte alignment
  uint8_t *outputLine = TextBuffer;
  packetBuffer[0] = (uint8_t)((Buffers::command_Copy) >> 8); 
  packetBuffer[1] = (uint8_t)((Buffers::command_Copy)&0xFF); 
  for (uint8_t y = 0; y < SmallFont::FNT_HEIGHT; y++, outputLine += SmallFont::FNT_BUFFER_SIZE) {
    const uint16_t destAddr  = Utils::zx_spectrum_screen_address(xpos, ypos + y);
    packetBuffer[2] = (uint8_t)byteCount ; 
    packetBuffer[3] = (uint8_t)((destAddr) >> 8); 
    packetBuffer[4] = (uint8_t)((destAddr)&0xFF); 
    memcpy(&packetBuffer[5], outputLine, byteCount);
    Z80Bus::sendBytes(packetBuffer, 5 + byteCount);
  }
} 
// text: Draws only the required part of the screen buffer.
// Slower but useful for general-purpose text drawing.
void text(int xpos, int ypos, const char *message) {
  const uint8_t charCount = SmallFont::prepareTextGraphics(TextBuffer, message);
  const uint8_t byteCount = ((charCount * (SmallFont::FNT_WIDTH + SmallFont::FNT_GAP)) + 7) / 8;  // byte alignment
  uint8_t *outputLine = TextBuffer;
  packetBuffer[0] = (uint8_t)((Buffers::command_Copy) >> 8); 
  packetBuffer[1] = (uint8_t)((Buffers::command_Copy)&0xFF); 
  for (uint8_t y = 0; y < SmallFont::FNT_HEIGHT; y++, outputLine += SmallFont::FNT_BUFFER_SIZE) {
    const uint16_t destAddr  = Utils::zx_spectrum_screen_address(xpos, ypos + y);
    packetBuffer[2] = (uint8_t)byteCount ; 
    packetBuffer[3] = (uint8_t)((destAddr) >> 8); 
    packetBuffer[4] = (uint8_t)((destAddr)&0xFF); 
    memcpy(&packetBuffer[5], outputLine, byteCount);
    Z80Bus::sendBytes(packetBuffer, 5 + byteCount);
  }
} 

}

#endif