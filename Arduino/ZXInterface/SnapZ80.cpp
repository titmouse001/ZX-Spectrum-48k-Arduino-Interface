#include <stdint.h>
#include "SnapZ80.h"
#include "Z802SNA.h"
#include "Constants.h"
#include "SdCardSupport.h"
#include "BufferManager.h" 
#include "Z80Bus.h" 
#include "PacketBuilder.h"
#include "Utils.h"

/*
 *******************************************************************
 * The .z80 format is rather unqiue - here's what I've found online:
 *******************************************************************
 * Z80 Snapshot Format Summary
 *
 * The .z80 format is a memory snapshot of the ZX Spectrum and is widely supported by emulators.
 * It cannot reproduce original tape content but allows near-instant loading.
 *
 * Versions:
 * v1 - 48K only, 30-byte header + compressed RAM (ED ED xx yy for repeated bytes >= 5)
 * v2/v3 - 30-byte base header (PC=0 signals v2/v3) + extended header (23 bytes for v2, 54/55 for v3)
 *
 * Compression:
 * ED ED xx yy -> repeat yy, xx times (only for runs >= 5)
 * ED sequences are encoded even if shorter (e.g., ED ED -> ED ED 02 ED)
 * End marker (v1 only): 00 ED ED 00
 *
 * v2/v3 add:
 * - More machine types (128K, +2A, Pentagon, etc.)
 * - Extended metadata (AY registers, T states, joystick config, paging info)
 * - Memory stored as blocks: [2-byte length][1-byte page][data]
 * - Length 0xFFFF = uncompressed 16K
 * - Pages vary by machine type (e.g., 48K: pages 4,5,8; 128K: 3-10)
 *
 * Notes:
 * - Bit 5 of byte 12 in v1 = compression flag (ignored in v2/v3)
 * - Hardware type defined at byte 34 of extended header
 * - No end marker for v2/v3 blocks
 *
 * https://worldofspectrum.org/faq/reference/z80format.htm
 */

// Search buffer to match the read buffer
constexpr uint16_t SEARCH_BUFFER_SIZE = FILE_READ_BUFFER_SIZE;
static const uint8_t END_MARKER[] = { 0x00, 0xED, 0xED, 0x00 };
#define MARKER_SIZE (sizeof(END_MARKER) / sizeof(END_MARKER[0]))

__attribute__((optimize("-Os")))
MachineType SnapZ80::getMachineDetails(int16_t z80_version, uint8_t Z80_EXT_HW_MODE) {
	if (z80_version == 1) {
		return MACHINE_48K;  // For V1, machine is implicitly 48K
	} else if (z80_version == 2) {
		switch (Z80_EXT_HW_MODE) {
			case 0: return MACHINE_48K; break;
			case 1: return MACHINE_48K; break;
			case 2: return MACHINE_48K; break;
			case 3: return MACHINE_128K; break;
			case 4: return MACHINE_128K; break;
		}
	} else if (z80_version == 3) {
		switch (Z80_EXT_HW_MODE) {
			case 0: return MACHINE_48K; break;
			case 1: return MACHINE_48K; break;
			case 2: return MACHINE_48K; break;
			case 3: return MACHINE_48K; break;
			case 4: return MACHINE_128K; break;
			case 5: return MACHINE_128K; break;
			case 6: return MACHINE_128K; break;
		}
	}
	return MACHINE_UNKNOWN;
}

__attribute__((optimize("-Os")))
Z80HeaderVersion SnapZ80::readZ80Header(FatFile* pFile, Z80HeaderInfo* headerInfo) {

  if (pFile->read(headerInfo->headerV1Data, Z80_V1_HEADERLENGTH) != Z80_V1_HEADERLENGTH) {
    return Z80_VERSION_UNKNOWN; 
  }

  uint8_t* v1_header = headerInfo->headerV1Data;

  // Ver1 Check: PC != 0
  if (v1_header[Z80_V1_PC_LOW] || v1_header[Z80_V1_PC_HIGH]) { 
    headerInfo->pc_low = v1_header[Z80_V1_PC_LOW];
    headerInfo->pc_high = v1_header[Z80_V1_PC_HIGH];
    headerInfo->hw_mode = 0;
    headerInfo->isV1Compressed = (v1_header[Z80_V1_FLAGS1] & 0x20) >> 5;
    headerInfo->version = Z80_VERSION_1;
    return Z80_VERSION_1;
  } 
  
  // Ver2/Ver3 Check (PC is 0, read extended header length)
  uint8_t len_buf[2];
  if (pFile->read(len_buf, 2) != 2) return Z80_VERSION_UNKNOWN;
  
  uint16_t ext_len = len_buf[0] | ((uint16_t)len_buf[1] << 8);
  if (ext_len < 3) return Z80_VERSION_UNKNOWN; // Corrupt file check

  uint8_t ext_data[3];
  if (pFile->read(ext_data, 3) != 3) return Z80_VERSION_UNKNOWN;
  headerInfo->pc_low = ext_data[Z80_EXT_PC_LOW];
  headerInfo->pc_high = ext_data[Z80_EXT_PC_HIGH];
  headerInfo->hw_mode = ext_data[Z80_EXT_HW_MODE];
  headerInfo->isV1Compressed = false; 
  headerInfo->version = (ext_len == Z80_V2_HEADERLENGTH) ? Z80_VERSION_2 : Z80_VERSION_3;

	// Jump past the extended header
  if (!pFile->seekCur(ext_len - 3)) {
    return Z80_VERSION_UNKNOWN; // Seek failed
  }

  return headerInfo->version;
}


__attribute__((optimize("-Os")))
bool SnapZ80::locateV1Terminator(FatFile* pFile, uint32_t start_pos, uint32_t& rle_data_length) {
	uint32_t file_size = pFile->fileSize();

	// Quick check: Most valid .Z80 Ver1 files end with the 4-byte marker
	if (file_size >= (start_pos + MARKER_SIZE)) {
		uint32_t end_pos = file_size - MARKER_SIZE;
		if (pFile->seekSet(end_pos)) {
			uint8_t buf[4];
			if (pFile->read(buf, 4) == 4) {
				if (buf[0] == 0x00 && buf[1] == 0xED && 
					  buf[2] == 0xED && buf[3] == 0x00) {
					rle_data_length = end_pos - start_pos;
					return pFile->seekSet(start_pos);
				}
			}
		}
	}

	// Fallback: Full Scan (In case there is trailing junk or an early termination)
	if (!pFile->seekSet(start_pos)) return false;

	uint16_t mark = BufferManager::getMark();
	uint8_t* search_buffer = BufferManager::allocate(SEARCH_BUFFER_SIZE);

	uint8_t state = 0;  // Counts marker bytes
	uint32_t absolute_pos = start_pos;

	while (pFile->available()) {
		int16_t bytes_read = pFile->read(search_buffer, SEARCH_BUFFER_SIZE);
		if (bytes_read <= 0) break;

		for (int16_t i = 0; i < bytes_read; i++) {
			absolute_pos++;  // where are we in the file

			// State machine logic for {0x00, 0xED, 0xED, 0x00}
			if (search_buffer[i] == END_MARKER[state]) {
				state++;
				if (state == MARKER_SIZE) {
					rle_data_length = (absolute_pos - MARKER_SIZE) - start_pos;
					BufferManager::freeToMark(mark);
					return pFile->seekSet(start_pos);  // found it!
				}
			} else {
				// Reset state - allow continue for start of marker
				state = (search_buffer[i] == END_MARKER[0]) ? 1 : 0;
			}
		}
	}

	BufferManager::freeToMark(mark);
	return false;
}

__attribute__((optimize("-Os")))
bool SnapZ80::checkZ80FileValidity(FatFile* pFile, Z80HeaderInfo* headerInfo) {
	bool result = true;
	uint32_t initial_file_pos = pFile->curPosition();

	if (getMachineDetails(headerInfo->version, headerInfo->hw_mode) != MACHINE_48K) {
		result = false;
	} else if (headerInfo->version >= Z80_VERSION_2) {
		while (pFile->available()) {
			uint8_t len_buf[3];  // compressed length (2 bytes), page number (1 byte)
			if (pFile->read(len_buf, 3) != 3) {
				return false;
			}
			uint16_t compressed_len = len_buf[0] | (len_buf[1] << 8);
			if (!pFile->seekCur(compressed_len)) { // RESTORE NO REALLY NEEDED
				return false;  // skip block data
			}
		}
	} else {  // V1
		if (headerInfo->isV1Compressed) {
			if (!locateV1Terminator(pFile, initial_file_pos, headerInfo->v1PayloadLength ) ) {
				result = false;
			}
		} else { // check uncompressed data is 48k + file header
			if (pFile->fileSize() != (0xC000 + Z80_V1_HEADERLENGTH)) {  
				result = false;
			}
		}
	}

	pFile->seekSet(initial_file_pos);
	return result;
}

// Z80 format - Block decompression support for it's "ED ED [count] [value]" format
__attribute__((optimize("-Ofast"))) 
void SnapZ80::decodeRLE_core(FatFile* pFile, uint16_t sourceLengthLimit, uint16_t currentAddress) {
  const uint16_t PAYLOAD_SIZE = 255;
  uint16_t mark = BufferManager::getMark();
  uint8_t* fileReadBufferPtr = BufferManager::allocate(FILE_READ_BUFFER_SIZE);
  uint8_t* txBuffer = BufferManager::allocate( PAYLOAD_SIZE);
	uint8_t* header = BufferManager::allocate(sizeof(TransferPacket)); 

	uint8_t commandPayloadPos = 0;
  uint16_t fileReadBufferCurrentPos = 0;
  uint16_t fileReadBufferBytesAvailable = 0;
  uint16_t bytesReadFromSource = 0;

	// Cache SD card reads
  auto getNextByteFromFile = [&]() -> uint8_t {
    if (fileReadBufferCurrentPos >= fileReadBufferBytesAvailable) {
      uint32_t remaining = (sourceLengthLimit > bytesReadFromSource) ? (sourceLengthLimit - bytesReadFromSource) : 0;
      uint16_t bytesToRead = min((uint32_t)FILE_READ_BUFFER_SIZE, remaining);
      fileReadBufferBytesAvailable = pFile->read(fileReadBufferPtr, bytesToRead);
      fileReadBufferCurrentPos = 0;
    }
    bytesReadFromSource++;
    return fileReadBufferPtr[fileReadBufferCurrentPos++];
  };

	// Send uncompressed to Z80
  auto flushCommandPayloadBuffer = [&]() {
    if (commandPayloadPos > 0) {
      uint8_t headerLen = PacketBuilder::buildTransferCommand(header, currentAddress, commandPayloadPos);
    	Z80Bus::sendBytes(header, headerLen );
      Z80Bus::sendBytes(txBuffer, commandPayloadPos);
      currentAddress += commandPayloadPos;
      commandPayloadPos = 0;
    }
  };

	// Queue up uncompressed sending when buffer full
  auto addByteToCommandPayloadBuffer = [&](uint8_t byte) {
    txBuffer[commandPayloadPos++] = byte;
    if (commandPayloadPos >= PAYLOAD_SIZE) {
      flushCommandPayloadBuffer();
    }
  };

  while (bytesReadFromSource < sourceLengthLimit) {
    uint8_t b1 = getNextByteFromFile();
    if (b1 == 0xED) { // maybe the start of compressed sequence
      uint8_t b2 = getNextByteFromFile();
      if (b2 == 0xED) {  // double ED then it's compressed data to follow
				// We are officially in a compressed block!  (Format: ED ED [count] [value])
        flushCommandPayloadBuffer(); // Flush uncompressed bytes
        uint8_t runAmount = getNextByteFromFile(); 	// repeat count
        uint8_t value = getNextByteFromFile();  		// byte to repeat
			
			  uint8_t packetLen = PacketBuilder::build_command_fill_mem_bytecount(txBuffer, currentAddress , runAmount, value);
        Z80Bus::sendBytes(txBuffer, packetLen);   	// Ok to reuse and build with txBuffer here to send fill
        currentAddress += runAmount;
      } else {
				// We found a 'ED' followed by something that is NOT 'ED'. The format says that the byte immediately following a literal 'ED' 
				// is NOT part of a compression block. So, we treat both b1 ('ED') and b2 as normal, uncompressed data.
        addByteToCommandPayloadBuffer(0xED);
        addByteToCommandPayloadBuffer(b2);
      }
    } else {
      addByteToCommandPayloadBuffer(b1);  // Not 'ED' so just uncompressed data.
    }
  }
	
  flushCommandPayloadBuffer(); 		// Send any leftover uncompressed bytes
  BufferManager::freeToMark(mark); // release all mallocs in this method
}

__attribute__((optimize("-Ofast")))
BlockReadResult SnapZ80::readAndWriteBlock(FatFile* pFile) {
	uint8_t header[3];  // compressed length (2 bytes), page number (1 byte)
	if (pFile->read(header, sizeof(header)) != sizeof(header)) {
		return pFile->available() ? BLOCK_ERROR : BLOCK_END_OF_FILE;
	}

	const uint16_t compressed_len = header[0] | ((uint16_t)(header[1]) << 8);
	const uint8_t page_number = header[2];
	uint16_t mem_offset;  // offset for 48K Z80 snapshot pages
	switch (page_number) {
		case 8: mem_offset = 0x4000; break;
		case 4: mem_offset = 0x8000; break;
		case 5: mem_offset = 0xC000; break;
		default:
			if (!pFile->seekCur(compressed_len)) {
				return BLOCK_ERROR;
			}
			return BLOCK_UNSUPPORTED_PAGE;
	}

	decodeRLE_core(pFile, compressed_len, mem_offset);
	return BLOCK_SUCCESS;
}

// .z80 files get converted to reuse existing ".SNA" game loading functionaliy
bool SnapZ80::convertSendZ80toSNA(FatFile* pFile, Z80HeaderInfo* headerInfo, uint8_t* snaHeader) {
	uint16_t stackAddrForPushingPC = 0;
	uint8_t* v1_header = headerInfo->headerV1Data;

	if (headerInfo->version >= 2) {  
		//
		// V2 or V3 format
		//
		// V2/V3 don't store the PC (it's zero)
		// Restore PC from extended header and convert to V1 format for processing
		v1_header[Z80_V1_PC_LOW] = headerInfo->pc_low;
		v1_header[Z80_V1_PC_HIGH] = headerInfo->pc_high;
		// Clear bit 7 of R register
		v1_header[Z80_V1_R_7BITS] &= ~0x80;

		stackAddrForPushingPC = Z802SNA::convertZ80HeaderToSna(v1_header, snaHeader);

		while (true) {
			BlockReadResult block_result = readAndWriteBlock(pFile); 
			if (block_result == BLOCK_END_OF_FILE) break;
			if (block_result == BLOCK_UNSUPPORTED_PAGE) { continue; }  // Skip to the next block
			if (block_result < 0) { return false; }                    // block_result; }             // negative critical errors
		}
	} else {      
		//                                                     
		// V1 Format
		//
		const uint32_t DEST_BUFFER_SIZE = ZX_SPECTRUM_48K_TOTAL_MEMORY;  // Expected uncompressed size (48K machine)
		stackAddrForPushingPC = Z802SNA::convertZ80HeaderToSna(v1_header, snaHeader);

		if (headerInfo->isV1Compressed) {
			uint32_t rle_data_length = headerInfo->v1PayloadLength; 
      decodeRLE_core(pFile, rle_data_length, ZX_SCREEN_ADDRESS_START);

			// uint32_t start_of_rle_data = pFile->curPosition();
			// uint32_t rle_data_length = 0;
			// if (!locateV1Terminator(pFile, start_of_rle_data, rle_data_length) || rle_data_length == 0) {
			// 	return false;  //  BLOCK_ERROR_NO_V1_MARKER;
			// }
			// decodeRLE_core(pFile, rle_data_length, ZX_SCREEN_ADDRESS_START);

			// // Skip past the marker (locateV1Terminator leaves us at the start, so we need to skip past data + marker)
			// uint32_t expected_pos_after_marker = start_of_rle_data + rle_data_length + 4;
			// if (pFile->curPosition() < expected_pos_after_marker) {
			// 	if (!pFile->seekSet(expected_pos_after_marker)) {
			// 		return false;  // Failed to seek past marker
			// 	}
			// }
		} else {  // Uncompressed V1 data
			decodeRLE_core(pFile, DEST_BUFFER_SIZE, ZX_SCREEN_ADDRESS_START);
		}
	}

	// Fake push 'PC' onto the stack  
	constexpr uint8_t TRANSMIT_AMOUNT = 2;
	uint8_t buf[sizeof(TransferPacket)];
	uint8_t packetLen = PacketBuilder::buildTransferCommand(buf,stackAddrForPushingPC, TRANSMIT_AMOUNT);
	Z80Bus::sendBytes(buf, packetLen );   // send Command
	buf[0] = headerInfo->pc_low;   				// it's safe to reuse buf
	buf[1] = headerInfo->pc_high;  
	Z80Bus::sendBytes(buf, TRANSMIT_AMOUNT);  // send the data (Z80's PC to be next off the stack)

	return true; // BLOCK_SUCCESS;
}




