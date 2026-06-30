#include <stdint.h>
#include "Draw.h"
#include "Z80Bus.h"
#include "utils.h"
#include "RenderFont.h"
#include "constants.h"
#include "PacketTypes.h"
#include "BufferManager.h"

#include "FontData.h"

constexpr uint16_t RENDER_SIZE = (SmallFont::FNT_BUFFER_SIZE * SmallFont::FNT_HEIGHT) + sizeof(Copy32Packet);
constexpr uint16_t PIX_INC = 0x0100;  // within same character -> +256 for next pixel line
constexpr uint16_t screen_width_offset = ZX_SCREEN_WIDTH_BYTES - (7 * PIX_INC);

// textLine: Draws up to a full-width line of text (32 bytes per line)
// Text must be NULL-terminated and no longer than 42 characters (font width is 6 pixels, screen width 256 pixels -> max 42 chars)
__attribute__((optimize("-Ofast"))) 
void Draw::textLine(int ypos, const char *message) {

    uint16_t mark = BufferManager::getMark();
    uint8_t *fontData = BufferManager::allocate(RENDER_SIZE);     // 32*7+4
    
    RenderFont::prepareTextGraphics(fontData, message);
    uint16_t destAddr = Utils::zx_spectrum_screen_address(ypos);

    Copy32Packet pkt;
    for (uint8_t y = 0; y < SmallFont::FNT_HEIGHT; ++y) {
        pkt.dest_addr_high = static_cast<uint8_t>(destAddr >> 8);
        pkt.dest_addr_low  = static_cast<uint8_t>(destAddr & 0xFF);
        Z80Bus::sendBytes(reinterpret_cast<uint8_t*>(&pkt), sizeof(Copy32Packet));  // header
        Z80Bus::sendBytes(fontData, SmallFont::FNT_BUFFER_SIZE);                    // body
        fontData += SmallFont::FNT_BUFFER_SIZE;
        destAddr += PIX_INC;  // down 1 pixel scanline 
    }

    BufferManager::freeToMark(mark);
}

// text_P: TEXT SUPPORT FROM FLASH MEMORY 
void Draw::text_P(int xpos, int ypos, const __FlashStringHelper *flashStr) {
    uint16_t mark = BufferManager::getMark();
    uint8_t *buffer = BufferManager::allocate(RENDER_SIZE);
    const uint8_t charCount = RenderFont::prepareTextGraphics_P(buffer, flashStr);
    drawTextInternal(xpos, ypos, charCount, buffer);
    BufferManager::freeToMark(mark);
}

// text: General purpuse text
void Draw::text(int xpos, int ypos, const char *message) {
    uint16_t mark = BufferManager::getMark();
    uint8_t *buffer = BufferManager::allocate(RENDER_SIZE);
    const uint8_t charCount = RenderFont::prepareTextGraphics(buffer, message);
    drawTextInternal(xpos, ypos, charCount, buffer);
    BufferManager::freeToMark(mark);
}

void Draw::drawTextInternal(int xpos, int ypos, uint8_t charCount, uint8_t *fontData) {

    CopyPacket pkt;
    const uint8_t byteWidth = ((charCount * (SmallFont::FNT_WIDTH + SmallFont::FNT_GAP)) + 7) / 8;
    pkt.amount = byteWidth;

    for (uint8_t y = 0; y < SmallFont::FNT_HEIGHT; y++, fontData += SmallFont::FNT_BUFFER_SIZE) {
        const uint16_t destAddr = Utils::zx_spectrum_screen_address(xpos, ypos + y);
        pkt.dest_addr_high = static_cast<uint8_t>(destAddr >> 8);
        pkt.dest_addr_low  = static_cast<uint8_t>(destAddr & 0xFF);
        Z80Bus::sendBytes(reinterpret_cast<uint8_t*>(&pkt), sizeof(CopyPacket));
        Z80Bus::sendBytes(fontData, byteWidth);
    }
}

