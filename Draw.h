#ifndef DRAW_H
#define DRAW_H

//extern FatFile root;
//extern FatFile file;

namespace Draw {
  
/**
 * textLine:  Draws a full-width line of text (32 bytes per line).
 * The text is rendered into a buffer and sent row-by-row to the screen memory.
 * 
 * @param xpos     X coords
 * @param ypos     Y coords
 * @param message  String/null-terminated
 */
__attribute__((optimize("-Ofast"))) 
void textLine(int xpos, int ypos, const char *message) {
  SmallFont::prepareTextGraphics(TextBuffer, message);  // Pre-render text into TextBuffer (output count ignored)
  // START_UPLOAD_COMMAND(packetBuffer, 'Z', SmallFont::FNT_BUFFER_SIZE); // Z = command_Copy32

  uint8_t *outputLine = TextBuffer;
 // packetBuffer[0] = 'Z';
  for (uint8_t y = 0; y < SmallFont::FNT_HEIGHT; ++y, outputLine += SmallFont::FNT_BUFFER_SIZE) {
    const uint16_t destAddr  = Utils::zx_spectrum_screen_address(xpos, ypos + y);
  //  ADDR_UPLOAD_COMMAND(packetBuffer, destAddr );
//    packetBuffer[1] = (uint8_t)(destAddr >> 8);
//    packetBuffer[2] = (uint8_t)(destAddr & 0xFF);
//    memcpy(&packetBuffer[3], outputLine, SmallFont::FNT_BUFFER_SIZE);
//    Z80Bus::sendBytes(packetBuffer, 3 + SmallFont::FNT_BUFFER_SIZE); // Send 32 byte line


    packetBuffer[0] = (uint8_t)((command_Copy32) >> 8); 
    packetBuffer[1] = (uint8_t)((command_Copy32)&0xFF); 
    packetBuffer[2] = (uint8_t)((destAddr) >> 8); 
    packetBuffer[3] = (uint8_t)((destAddr)&0xFF); 
    memcpy(&packetBuffer[4], outputLine, SmallFont::FNT_BUFFER_SIZE);
    Z80Bus::sendBytes(packetBuffer, 4  + SmallFont::FNT_BUFFER_SIZE);

  }
}

/**
 * text: Draws only the required part of the screen buffer.
 * Useful for general-purpose text drawing.
 * 
 * @param xpos     X coords
 * @param ypos     Y coords
 * @param message  String/null-terminated
 */
__attribute__((optimize("-Os")))   // Optimized for size - not used in time critical areas.
void text(int xpos, int ypos, const char *message) {
  const uint8_t charCount = SmallFont::prepareTextGraphics(TextBuffer, message);
  const uint8_t byteCount = ((charCount * (SmallFont::FNT_WIDTH + SmallFont::FNT_GAP)) + 7) / 8;  // byte alignment
 // START_UPLOAD_COMMAND(packetBuffer, 'C', byteCount);  // C = command_Copy
  uint8_t *outputLine = TextBuffer;
  for (uint8_t y = 0; y < SmallFont::FNT_HEIGHT; y++, outputLine += SmallFont::FNT_BUFFER_SIZE) {
    const uint16_t destAddr  = Utils::zx_spectrum_screen_address(xpos, ypos + y);
  //  ADDR_UPLOAD_COMMAND(packetBuffer, destAddr );
  //  memcpy(&packetBuffer[SIZE_OF_HEADER], outputLine, byteCount);
  //  Z80Bus::sendBytes(packetBuffer, SIZE_OF_HEADER + byteCount); // send trimmed line


    packetBuffer[0] = (uint8_t)((command_Copy) >> 8); 
    packetBuffer[1] = (uint8_t)((command_Copy)&0xFF); 
    packetBuffer[2] = (uint8_t)byteCount ; 
    packetBuffer[3] = (uint8_t)((destAddr) >> 8); 
    packetBuffer[4] = (uint8_t)((destAddr)&0xFF); 

    Z80Bus::sendBytes(packetBuffer, 5);


  }
} 


}

#endif