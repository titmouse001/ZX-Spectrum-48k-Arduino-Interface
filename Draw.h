#ifndef DRAW_H
#define DRAW_H

extern FatFile root;
extern FatFile file;
extern char fileName[65];

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
  START_UPLOAD_COMMAND(packetBuffer, 'Z', SmallFont::FNT_BUFFER_SIZE);
  uint8_t *outputLine = TextBuffer;
  for (uint8_t y = 0; y < SmallFont::FNT_HEIGHT; ++y, outputLine += SmallFont::FNT_BUFFER_SIZE) {
    const uint16_t destAddr  = Utils::zx_spectrum_screen_address(xpos, ypos + y);
    END_UPLOAD_COMMAND(packetBuffer, destAddr );
    memcpy(&packetBuffer[SIZE_OF_HEADER], outputLine, SmallFont::FNT_BUFFER_SIZE);
    Z80Bus::sendBytes(packetBuffer, SIZE_OF_HEADER + SmallFont::FNT_BUFFER_SIZE); // Send line data
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
  START_UPLOAD_COMMAND(packetBuffer, 'C', byteCount);
  uint8_t *outputLine = TextBuffer;
  for (uint8_t y = 0; y < SmallFont::FNT_HEIGHT; y++, outputLine += SmallFont::FNT_BUFFER_SIZE) {
    const uint16_t destAddr  = Utils::zx_spectrum_screen_address(xpos, ypos + y);
    END_UPLOAD_COMMAND(packetBuffer, destAddr );
    memcpy(&packetBuffer[SIZE_OF_HEADER], outputLine, byteCount);
    Z80Bus::sendBytes(packetBuffer, SIZE_OF_HEADER + byteCount); // send trimmed line
  }
} // refactored DrawText... but not tested yet .. should be ok


__attribute__((optimize("-Ofast"))) 
void fileList(uint16_t startFileIndex) {
  root.rewind();
  uint8_t clr = 0;
  uint8_t count = 0;
  while (file.openNext(&root, O_RDONLY)) {
    if (file.isFile()) {
      if (file.fileSize() == 49179) {

        if ((count >= startFileIndex) && (count < startFileIndex + SCREEN_TEXT_ROWS)) {
          int len = file.getName7(fileName, 64);
          if (len == 0) { file.getSFN(fileName, 20); }

          if (len > 42) {
            fileName[40] = '.';
            fileName[41] = '.';
            fileName[42] = '\0';
          }

          textLine(0, ((count - startFileIndex) * 8), fileName);
          clr++;
        }
        count++;
      }
    }
    file.close();
    if (clr == SCREEN_TEXT_ROWS) {
      break;
    }
  }

  // Clear the remaining screen after last list item when needed.
  fileName[0] = ' ';  // Empty file selector slots (just wipes area)
  fileName[1] = '\0';
  for (uint8_t i = clr; i < SCREEN_TEXT_ROWS; i++) {
    textLine(0, (i * 8), fileName);
  }
}

}

#endif