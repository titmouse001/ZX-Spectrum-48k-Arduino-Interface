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

Z80HeaderVersion SnapZ80::readZ80Header(FatFile* pFile, Z80HeaderInfo* headerInfo) {
	uint16_t mark = BufferManager::getMark();
	uint8_t* buf = BufferManager::allocate(Z80_V1_HEADERLENGTH);
	Z80HeaderVersion ver = readZ80HeaderInternal(pFile, headerInfo, buf);
	BufferManager::freeToMark(mark);
	return ver;
}


__attribute__((optimize("-Os")))
Z80HeaderVersion SnapZ80::readZ80HeaderInternal(FatFile* pFile, Z80HeaderInfo* headerInfo, uint8_t* buf) {

	uint8_t* v1_header = buf;
	if (pFile->read(buf, Z80_V1_HEADERLENGTH) != Z80_V1_HEADERLENGTH) {
		return Z80_VERSION_UNKNOWN;  // Error reading header
	}

	memcpy(headerInfo->headerData, buf, Z80_V1_HEADERLENGTH);

	if (v1_header[Z80_V1_PC_LOW] || v1_header[Z80_V1_PC_HIGH]) {  // V1: PC!=0

		headerInfo->pc_low = v1_header[Z80_V1_PC_LOW];
		headerInfo->pc_high = v1_header[Z80_V1_PC_HIGH];
		headerInfo->hw_mode = 0;
		headerInfo->isV1Compressed = (v1_header[Z80_V1_FLAGS1] & 0x20) >> 5;
		headerInfo->version = Z80_VERSION_1;
		return Z80_VERSION_1;  // ... here we know PC must hold a valid address

	} else {  // (PC==0) check for a valid V2/V3
		if (pFile->read(buf, 2) != 2) {
			return Z80_VERSION_UNKNOWN;
		}
		uint8_t len_lo = buf[0];
		uint8_t len_hi = buf[1];
		uint16_t ext_len = len_lo | ((uint16_t)len_hi << 8);

		uint16_t bytes_to_read_ext_header = ext_len;
		uint16_t bytes_read_so_far = 0;

		// Initialize extended header values (v2/v3)
		headerInfo->pc_low = 0;
		headerInfo->pc_high = 0;
		headerInfo->hw_mode = 0;

		while (bytes_read_so_far < bytes_to_read_ext_header) {
			uint16_t chunk_size = min(30, (uint16_t)(bytes_to_read_ext_header - bytes_read_so_far));
			if (pFile->read(buf, chunk_size) != (int16_t)chunk_size) {
				return Z80_VERSION_UNKNOWN;  // Error reading extended header
			}

			// Find PC and Mode dor the extended/additional header part (v2/v3)
			for (uint16_t i = 0; i < chunk_size; i++) {
				uint16_t current_ext_byte_index = bytes_read_so_far + i;
				uint8_t b = buf[i];

				switch (current_ext_byte_index) {  // Extracting only the needed bytes from extended header
					case Z80_EXT_PC_LOW: headerInfo->pc_low = b; break;
					case Z80_EXT_PC_HIGH: headerInfo->pc_high = b; break;
					case Z80_EXT_HW_MODE: headerInfo->hw_mode = b; break;
				}
			}
			bytes_read_so_far += chunk_size;
		}

		headerInfo->isV1Compressed = false;  // V2/V3 don't use this flag
		headerInfo->version = (ext_len == Z80_V2_HEADERLENGTH) ? Z80_VERSION_2 : Z80_VERSION_3;
		return headerInfo->version;
	}
}


__attribute__((optimize("-Os")))
bool SnapZ80::findMarkerOptimized(FatFile* pFile, int32_t start_pos, uint32_t& rle_data_length) {
	static const uint8_t MARKER[] = { 0x00, 0xED, 0xED, 0x00 };
	constexpr uint8_t MARKER_SIZE = 4;
	constexpr uint16_t SEARCH_BUFFER_SIZE = FILE_READ_BUFFER_SIZE;
	uint16_t mark = BufferManager::getMark();
	uint8_t* search_buffer = BufferManager::allocate(SEARCH_BUFFER_SIZE);

	// Quick end-of-file check first
	int32_t file_size = pFile->fileSize();
	if (file_size != -1 && file_size >= (start_pos + MARKER_SIZE)) {
		int32_t potential_marker_pos = file_size - MARKER_SIZE;
		if (pFile->seekSet(potential_marker_pos)) {
			if (pFile->read(search_buffer, MARKER_SIZE) == MARKER_SIZE && memcmp(search_buffer, MARKER, MARKER_SIZE) == 0) {
				rle_data_length = (uint32_t)(potential_marker_pos - start_pos);
				pFile->seekSet(start_pos);  // Seek back to start position for caller

				BufferManager::freeToMark(mark);
				return true;
			}
		}
		pFile->seekSet(start_pos);  // Always seek back to start position
	}
	// Fallback to buffered search if marker not at end
	int32_t current_pos = start_pos;
	uint16_t overlap = 0;
	while (pFile->available()) {
		uint16_t bytes_to_read = SEARCH_BUFFER_SIZE - overlap;
		int16_t bytes_read = pFile->read(search_buffer + overlap, bytes_to_read);
		if (bytes_read == 0) break;
		uint16_t search_end = overlap + bytes_read;
		for (uint16_t i = 0; i <= search_end - MARKER_SIZE; i++) {
			if (memcmp(search_buffer + i, MARKER, MARKER_SIZE) == 0) {  // look for '0x00EDED00'
				rle_data_length = (uint32_t)((current_pos + i) - start_pos);

				BufferManager::freeToMark(mark);
				return true;
			}
		}
		// hold back 3 bytes for next time around as marker might span
		if (search_end >= MARKER_SIZE - 1) {
			memcpy(search_buffer, search_buffer + search_end - (MARKER_SIZE - 1), MARKER_SIZE - 1);
			overlap = MARKER_SIZE - 1;
			current_pos += bytes_read;
		} else {
			break;  // Not enough data left
		}
	}

	BufferManager::freeToMark(mark);
	return false;  // Marker not found
}


__attribute__((optimize("-Os")))
bool SnapZ80::checkZ80FileValidity(FatFile* pFile, const Z80HeaderInfo* headerInfo) {
  bool result = true;
  int32_t initial_file_pos = pFile->curPosition();
  if (initial_file_pos == -1) return false;

  if (getMachineDetails(headerInfo->version, headerInfo->hw_mode) != MACHINE_48K) {
    result = false;
  } else if (headerInfo->version >= Z80_VERSION_2) {

		uint8_t tmp[3];
    while (pFile->available()) {
      uint32_t current_pos = pFile->curPosition();
      if (pFile->read(tmp, 3) != 3) {
        if (pFile->available()) result = false;
        break;
      }
      uint16_t compressed_length = tmp[0] | ((uint16_t)tmp[1] << 8);
      if (!pFile->seekSet(current_pos + 3 + compressed_length)) {
        result = false;
        break;
      }
    }
  } else { // V1
    if (headerInfo->isV1Compressed) {
      uint32_t dummy_length;
      if (!findMarkerOptimized(pFile, initial_file_pos, dummy_length)) result = false;
    } else {
      if (pFile->fileSize() != (0xC000 + Z80_V1_HEADERLENGTH)) result = false;
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
	uint8_t* header = BufferManager::allocate( E(TransferPacket::PACKET_LEN));

  uint16_t commandPayloadPos = 0;
  uint16_t fileReadBufferCurrentPos = 0;
  uint16_t fileReadBufferBytesAvailable = 0;
  uint32_t bytesReadFromSource = 0;

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

	// Z80 RLE DECODING LOOP
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
        addByteToCommandPayloadBuffer(b1);
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
BlockReadResult SnapZ80::z80_readAndWriteBlock(FatFile* pFile, uint8_t* page_number_out) {
    uint8_t headerBuf[2];

    if (pFile->read(headerBuf, 2) != 2) {
        *page_number_out = (uint8_t)-1;
        return (!pFile->available()) ? BLOCK_END_OF_FILE : BLOCK_ERROR;
    }
    
    uint16_t compressed_length = headerBuf[0] | ((uint16_t)headerBuf[1] << 8);

    if (pFile->read(headerBuf, 1) != 1) {
        *page_number_out = (uint8_t)-1;
        return BLOCK_ERROR;
    }
    *page_number_out = headerBuf[0];
    
    int32_t sna_offset = -1;
    if (*page_number_out == 8) sna_offset = 0x4000;
    else if (*page_number_out == 4) sna_offset = 0x8000;
    else if (*page_number_out == 5) sna_offset = 0xC000;

    if (sna_offset == -1) {
        // Use seekCur to jump over the compressed data block
        pFile->seekCur(compressed_length);
        return BLOCK_UNSUPPORTED_PAGE;
    }

    decodeRLE_core(pFile, compressed_length, (uint16_t)sna_offset);
    return BLOCK_SUCCESS;
}

bool SnapZ80::convertZ80toSNA_impl(FatFile* pFile, Z80HeaderInfo* headerInfo, uint8_t* snaHeader) {
	uint16_t stackAddrForPushingPC = 0;
	uint8_t* v1_header = headerInfo->headerData;

	if (headerInfo->version >= 2) {  // V2 or V3 format
		// V2/V3 save states store PC as zero in the main header
		// Restore PC from extended header and convert to V1 format for processing
		v1_header[Z80_V1_PC_LOW] = headerInfo->pc_low;
		v1_header[Z80_V1_PC_HIGH] = headerInfo->pc_high;
		// Clear bit 7 of R register
		v1_header[Z80_V1_R_7BITS] &= ~0x80;

		stackAddrForPushingPC = Z802SNA::convertZ80HeaderToSna(v1_header, snaHeader);

		while (true) {
			uint8_t page_number;
			BlockReadResult block_result = z80_readAndWriteBlock(pFile, &page_number);  //, &uncompressed_length);
			if (block_result == BLOCK_END_OF_FILE) break;
			if (block_result == BLOCK_UNSUPPORTED_PAGE) { continue; }  // Skip to the next block
			if (block_result < 0) { return false; }                    // block_result; }             // negative critical errors
		}
	} else {                                                           // version 1
		const uint32_t DEST_BUFFER_SIZE = ZX_SPECTRUM_48K_TOTAL_MEMORY;  // Expected uncompressed size (48K machine)
		stackAddrForPushingPC = Z802SNA::convertZ80HeaderToSna(v1_header, snaHeader);

		if (headerInfo->isV1Compressed) {
			uint32_t start_of_rle_data = pFile->curPosition();
			//		if (start_of_rle_data == -1) return -1;
			uint32_t rle_data_length = 0;
			if (!findMarkerOptimized(pFile, start_of_rle_data, rle_data_length) || rle_data_length == 0) {
				return false;  //  BLOCK_ERROR_NO_V1_MARKER;
			}
			decodeRLE_core(pFile, rle_data_length, ZX_SCREEN_ADDRESS_START);

			// Skip past the marker (findMarkerOptimized leaves us at the start, so we need to skip past data + marker)
			uint32_t expected_pos_after_marker = start_of_rle_data + rle_data_length + 4;
			if (pFile->curPosition() < expected_pos_after_marker) {
				if (!pFile->seekSet(expected_pos_after_marker)) {
					return false;  // -1;  // Failed to seek past marker
				}
			}
		} else {  // Uncompressed V1 data
			decodeRLE_core(pFile, DEST_BUFFER_SIZE, ZX_SCREEN_ADDRESS_START);
		}
	}


	// Fake push 'PC' onto the stack
	// NOTE: We are currently reusing existing ".SNA" functionaliy for loading the final Z80's CPU registers.

	constexpr uint8_t TRANSMIT_AMOUNT = 2;
	uint8_t buf[E(TransferPacket::PACKET_LEN)];
	uint8_t packetLen = PacketBuilder::buildTransferCommand(buf,stackAddrForPushingPC, TRANSMIT_AMOUNT);
	Z80Bus::sendBytes(buf, packetLen );   // Command packate
	buf[0] = headerInfo->pc_low;   
	buf[1] = headerInfo->pc_high;  
	Z80Bus::sendBytes(buf, TRANSMIT_AMOUNT);  // send the data (Z80's PC to be next off the stack)

	return true; // BLOCK_SUCCESS;
}

bool SnapZ80::convertZ80toSNA(FatFile* pFile, uint8_t* snaHeader) {
	Z80HeaderInfo headerInfo;
	if (readZ80Header(pFile, &headerInfo) == Z80_VERSION_UNKNOWN) {
		return false;
	}
	if (checkZ80FileValidity(pFile, &headerInfo)) {
		return convertZ80toSNA_impl(pFile, &headerInfo , snaHeader);
	}
	return false;
}


