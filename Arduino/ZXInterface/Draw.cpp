#include <stdint.h>
#include "Draw.h"
#include "PacketBuilder.h"
#include "Z80Bus.h"
#include "utils.h"
#include "RenderFont.h"
#include "constants.h"
#include "CommandRegistry.h"
#include "PacketTypes.h"
#include "BufferManager.h"

constexpr uint16_t RENDER_SIZE = (SmallFont::FNT_BUFFER_SIZE * SmallFont::FNT_HEIGHT) + E(Copy32Packet::PACKET_LEN);

// textLine:  Draws a full-width line of text (32 bytes per line).
// Optimised to draw menu text row-by-row to the screen memory.
__attribute__((optimize("-Ofast"))) 
void Draw::textLine(int ypos, const char *message) {
  // For ZX Spectrum, moving down 1 pixel within same character row adds 0x0100
  constexpr uint16_t PIX_INC = 0x0100;  // +256 for next pixel line
  constexpr uint16_t screen_width_offset = ZX_SCREEN_WIDTH_BYTES - (7 * PIX_INC);

  uint16_t mark = BufferManager::getMark();
  uint8_t *renderBuffer = BufferManager::allocate(RENDER_SIZE);
  uint8_t *outputLine1st = renderBuffer;
  renderBuffer+=E(Copy32Packet::PACKET_LEN);

  // SOME ROOM HERE FOR FETURE OPTIMISING - prepareTextGraphics used for this part looks for null (could just loop 32)
  RenderFont::prepareTextGraphics(renderBuffer, message);  // Pre-render text into renderBuffer
  outputLine1st[E(Copy32Packet::CMD_HIGH)] = (uint8_t)(CommandRegistry::command_Copy32 >> 8);
  outputLine1st[E(Copy32Packet::CMD_LOW)] = (uint8_t)(CommandRegistry::command_Copy32 & 0xFF);

  uint16_t destAddr = Utils::zx_spectrum_screen_address(ypos);
 
  for (uint8_t y = 0; y < SmallFont::FNT_HEIGHT; ++y) {
    outputLine1st[E(Copy32Packet::CMD_DEST_ADDR_HIGH)] = (uint8_t)(destAddr >> 8);
    outputLine1st[E(Copy32Packet::CMD_DEST_ADDR_LOW)] =  (uint8_t)(destAddr & 0xFF);
    Z80Bus::sendBytes(outputLine1st, E(Copy32Packet::PACKET_LEN) ); // expects 32 bytes
    Z80Bus::sendBytes(renderBuffer, SmallFont::FNT_BUFFER_SIZE); // must be x32 bytes

    renderBuffer += SmallFont::FNT_BUFFER_SIZE;

    if ((y & 0x07) == 0x07) {
      destAddr += screen_width_offset;  // Every 8 pixels, handle interleaved layout
    } else {
      destAddr += PIX_INC;
    }
  }

  BufferManager::freeToMark(mark);
}


//---------------------------------
// Slower more general-purpose text
//---------------------------------

// text_P - text in flash memory: Draws only the required part of the screen buffer.
void Draw::text_P(int xpos, int ypos, const __FlashStringHelper *flashStr) {
  uint16_t mark = BufferManager::getMark();
  uint8_t *renderBuffer = BufferManager::allocate(RENDER_SIZE);

  const uint8_t charCount = RenderFont::prepareTextGraphics_P(renderBuffer, flashStr);
  drawTextInternal(xpos, ypos, charCount, renderBuffer);

  BufferManager::freeToMark(mark);
}

// text: Draws only the required part of the screen buffer.
void Draw::text(int xpos, int ypos, const char *message) {
  uint16_t mark = BufferManager::getMark();
  uint8_t *renderBuffer = BufferManager::allocate(RENDER_SIZE);

  const uint8_t charCount = RenderFont::prepareTextGraphics(renderBuffer, message);
  drawTextInternal(xpos, ypos, charCount, renderBuffer);

  BufferManager::freeToMark(mark);
}

__attribute__((optimize("-Os"))) 
void Draw::drawTextInternal(int xpos, int ypos, uint8_t charCount, uint8_t *renderBuffer) {
  const uint8_t byteWidth = ((charCount * (SmallFont::FNT_WIDTH + SmallFont::FNT_GAP)) + 7) / 8;  // byte alignment

  uint16_t mark = BufferManager::getMark();
  uint8_t *packet = BufferManager::allocate(E(CopyPacket::PACKET_LEN));

  packet[E(CopyPacket::CMD_HIGH)] = (uint8_t)((CommandRegistry::command_Copy) >> 8);
  packet[E(CopyPacket::CMD_LOW)] = (uint8_t)((CommandRegistry::command_Copy)&0xFF);
  packet[E(CopyPacket::CMD_AMOUNT)] = (uint8_t)byteWidth;

  for (uint8_t y = 0; y < SmallFont::FNT_HEIGHT; y++, renderBuffer += SmallFont::FNT_BUFFER_SIZE) {
    const uint16_t destAddr = Utils::zx_spectrum_screen_address(xpos, ypos + y);
    packet[E(CopyPacket::CMD_DEST_ADDR_HIGH)] = (uint8_t)(destAddr >> 8);
    packet[E(CopyPacket::CMD_DEST_ADDR_LOW)] = (uint8_t)(destAddr & 0xFF);

    Z80Bus::sendBytes(packet, E(CopyPacket::PACKET_LEN));
    Z80Bus::sendBytes(renderBuffer, byteWidth);
  }

  BufferManager::freeToMark(mark);
}
