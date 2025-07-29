#ifndef BUFFERS_H
#define BUFFERS_H
// -----------------------------------------------------------------------------------
// Z80 Command Transfer Structure and Buffer Definitions
//
// This header defines the structure of command packets sent to the Z80,
// and manages memory buffers for both command payloads and file reading.

/*
// --- Command Packet Formats (Arduino -> Z80) ---
// Each row describes a command packet structure, as received by the Z80 from the Arduino.
// Fields are ordered by their byte index in the received packet.
//
// +-------+---------------------------------+----------------------------------------------------------------+------------------------------+---------------------+
// | Cmd   | Description                     | Packet Structure (Index: Field Name / Data Type)               | Additional Data After Packet | Total Packet Length |
// +-------+---------------------------------+----------------------------------------------------------------+------------------------------+---------------------+
// | 'Z'   | Copy 32 Bytes                   | 0: 'Z', 1: 1-2: <dest_addr> (16-bit)                           | 32 bytes of data             | 3+<32> bytes        |
// | 'C'   | Copy Data                       | 0: 'C', 1: <payload_size> (8-bit), 2-3: <dest_addr> (16-bit)   | <payload_size> bytes of data | 4+<N> bytes         |
// | 'F'   | Fill Memory Region              | 0: 'F', 1-2: <fill_amount> (16-bit), 3-4: <dest_addr> (16-bit) |                              |                     |
// |       |                                 |         , 5: <fill_value> (8-bit)                              | None                         | 6 bytes             |
// | 'f'   | Small Fill Memory Region        | 0: 'F', 1: <fill_amount> (16-bit), 2-3: <dest_addr> (16-bit)   |                              |                     |
// |       |                                 |         , 4: <fill_value> (8-bit)                              | None                         | 5 bytes             |
// | 'G'   | Transfer Data (Flashing Border) | 0: 'G', 1: <payload_size> (8-bit), 2-3: <dest_addr> (16-bit)   | <payload_size> bytes of data | 4+<N> bytes         |
// | 'E'   | Execute Program / Restore SNA   | 0: 'E'                                                         | 27 bytes (SNA header)        | 1+<27> bytes        |
// | 'W'   | Wait for 50Hz Interrupt         | 0: 'W'                                                         | None                         | 1 byte              |
// | 'S'   | Set Stack Pointer               | 0: 'S', 1-2: <sp_addr> (16-bit)                                | None                         | 3 bytes             |
// | 'T'   | Transmit Key Press              | 0: 'T', 1: <delay_value> (8-bit)                               | None                         | 2 bytes             |
// +-------+---------------------------------+----------------------------------------------------------------+------------------------------+---------------------+
*/
// Standard 4-byte header size used by Z, C, G, E:
const uint8_t SIZE_OF_HEADER = 4;       // 4 is more common, this is our default use case
const uint8_t HEADER_TOKEN = 0;           // Index for the command character
const uint8_t HEADER_PAYLOADSIZE = 1;     // Index for payload size (bytes) for 'C' and 'G' commands
const uint8_t HEADER_HIGH_BYTE = 2;       // Index for high byte of destination address for 'C' and 'G' commands
const uint8_t HEADER_LOW_BYTE = 3;        // Index for low byte of destination address for 'C' and 'G' commands
// ----------------------------------------------------------------------------------
// --- Buffer Segment Definitions ---
const uint16_t COMMAND_PAYLOAD_SECTION_SIZE = 255; // Maximum payload for 'C' and 'G' commands (0-255 bytes)
const uint16_t FILE_READ_BUFFER_SIZE = 128; // Size for buffering file read operations
const uint16_t TOTAL_PACKET_BUFFER_SIZE = SIZE_OF_HEADER + COMMAND_PAYLOAD_SECTION_SIZE + FILE_READ_BUFFER_SIZE;

// The byte offset where the file read buffer section begins within 'packetBuffer'.
const uint16_t FILE_READ_BUFFER_OFFSET = SIZE_OF_HEADER + COMMAND_PAYLOAD_SECTION_SIZE;

// --- Global Buffers ---
// The primary general-purpose buffer used for Z80 commands and file read operations.
uint8_t packetBuffer[TOTAL_PACKET_BUFFER_SIZE];

// Buffer for storing the 27-byte header of .sna files when using the 'E' (Execute) command.
uint8_t head27_Execute[27 + 1];

// Buffer for rendering text with SmallFont.
byte TextBuffer[SmallFont::FNT_BUFFER_SIZE * SmallFont::FNT_HEIGHT] = { 0 }; // SmallFont 5x7

#endif
