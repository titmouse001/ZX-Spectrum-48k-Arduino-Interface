#ifndef BUFFERS_H
#define BUFFERS_H
// -----------------------------------------------------------------------------------
// Copy/Transfer Structure
//
// - Index for command character 
//  'C'    // Transfer/copy (things like drawing Text, displying .scr files)
//	'F'    // Fill (clearing screen areas, selector bar)   
//  'G'    // Transfer/copy with flashing boarder (use for loading .sna files)
//  'E'    // Execute program (includes restoring registers/states from the 27 byte header) 
//  'W'    // Wait for 50Hz maskable interrupt (prevents interrupts from interfering at final run stage)
const uint8_t HEADER_TOKEN = 0;          // Hold command charater 'C','F','G',E','W'
/* Index for payload as bytes (max send of 255 bytes) */
const uint8_t HEADER_PAYLOADSIZE = 1;    // Only 'C' and 'G' commands
/* Index for high byte of destination address */
const uint8_t HEADER_HIGH_BYTE = 2;      // Only 'C' and 'G' commands
/* Index for low byte of destination addresa */
const uint8_t HEADER_LOW_BYTE = 3;       // Only 'C' and 'G' commands
// ----------------------------------------------------------------------------------
const uint8_t SIZE_OF_HEADER = HEADER_LOW_BYTE + 1;
/* Maximum payload per transfer is one byte (0–255) */   
const uint8_t PAYLOAD_BUFFER_SIZE = 255;
const uint16_t BUFFER_SIZE = SIZE_OF_HEADER + PAYLOAD_BUFFER_SIZE;

// ----------------------------------------------------------------------------------  
uint8_t packetBuffer[BUFFER_SIZE] ;     // Used by 'C','G' & 'F' commands
uint8_t head27_Execute[27 + 1];         // for 'E' Execute command (stores sna header) 
// ----------------------------------------------------------------------------------
#define SCREEN_TEXT_ROWS 24            // Number of on screen text list items
byte TextBuffer[SmallFont::FNT_BUFFER_SIZE * SmallFont::FNT_HEIGHT] = { 0 };
// ----------------------------------------------------------------------------------

char fileName[65];




#endif