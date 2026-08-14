#include <stdint.h>
//#include "snapZ802SNA.h"
#include "SnapZ80.h"
#include "BufferManager.h" 
#include "Z80Bus.h" 
#include "PacketTypes.h" 
#include "utils.h" 

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


MachineType SnapZ80::getMachineDetails(int8_t z80_version, uint8_t Z80_EXT_HW_MODE) {
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

  if (pFile->read(&headerInfo->headerV1Data, sizeof(Z80V1Header)) != sizeof(Z80V1Header)) {
    return Z80_VERSION_UNKNOWN; 
  }

  //uint8_t* v1_header = headerInfo->headerV1Data;

  Z80V1Header* v1_header = &headerInfo->headerV1Data;

  // Ver1 Check: PC != 0
  if (v1_header->pc_lo || v1_header->pc_hi) { 
    headerInfo->pc_low = v1_header->pc_lo;
    headerInfo->pc_high = v1_header->pc_hi;
    headerInfo->hw_mode = 0;
    headerInfo->isV1Compressed = (v1_header->flags1 & 0x20) >> 5;
    headerInfo->version = Z80_VERSION_1;
    return Z80_VERSION_1;
  } 
  
  // Ver2/Ver3 Check 
  // Next 2 bytes - Length of additional header block 
  uint8_t len_buf[2];
  if (pFile->read(len_buf, 2) != 2) return Z80_VERSION_UNKNOWN;
  
  // Check length of block
  uint16_t ext_len = len_buf[0] | ((uint16_t)len_buf[1] << 8);
  if (ext_len < 3) return Z80_VERSION_UNKNOWN; 

  // Next 3 bytes - Program counter / Hardware mode
  uint8_t ext_data[3];
  if (pFile->read(ext_data, 3) != 3) return Z80_VERSION_UNKNOWN;
  headerInfo->pc_low = ext_data[Z80_EXT_PC_LOW];
  headerInfo->pc_high = ext_data[Z80_EXT_PC_HIGH];
  headerInfo->hw_mode = ext_data[Z80_EXT_HW_MODE];
  headerInfo->isV1Compressed = false; 
  headerInfo->version = (ext_len == Z80_V2_HEADERLENGTH) ? Z80_VERSION_2 : Z80_VERSION_3;

  // Skip rest of the extended header
  if (!pFile->seekCur(ext_len - 3)) {
    return Z80_VERSION_UNKNOWN; // Seek failed
  }

  return headerInfo->version;
}

uint32_t SnapZ80::locateV1Terminator(FatFile* pFile, uint32_t start_pos) {
    uint32_t file_size = pFile->fileSize();
    // Quick check: Most valid .Z80 Ver1 files end with the 4-byte marker
    if (file_size >= (start_pos + MARKER_SIZE)) {
        uint32_t end_pos = file_size - MARKER_SIZE;
        if (pFile->seekSet(end_pos)) {
            uint8_t buf[4];
            if (pFile->read(buf, 4) == 4) {
                if (buf[0] == 0x00 && buf[1] == 0xED && 
                    buf[2] == 0xED && buf[3] == 0x00) {
                    if (!pFile->seekSet(start_pos)) return 0;
                    return end_pos - start_pos;
                }
            }
        }
    }

    // Fallback: Full Scan (In case there is trailing junk or an early termination)
    if (!pFile->seekSet(start_pos)) return 0;
    uint16_t mark = BufferManager::getMark();
    uint8_t* search_buffer = BufferManager::allocate(SEARCH_BUFFER_SIZE);
    uint8_t state = 0;  // Counts marker bytes
    uint32_t absolute_pos = start_pos;
    uint32_t rle_length = 0;
    while (pFile->available()) {
        int16_t bytes_read = pFile->read(search_buffer, SEARCH_BUFFER_SIZE);
        if (bytes_read <= 0) break;
        for (int16_t i = 0; i < bytes_read; i++) {
            absolute_pos++;  // where are we in the file
            // State machine logic for {0x00, 0xED, 0xED, 0x00}
            if (search_buffer[i] == END_MARKER[state]) {
                state++;
                if (state == MARKER_SIZE) {
                    rle_length = (absolute_pos - MARKER_SIZE) - start_pos;
                    break;
                }
            } else {
                // Reset state - allow continue for start of marker
                state = (search_buffer[i] == END_MARKER[0]) ? 1 : 0;
            }
        }
        if (rle_length > 0) break; // Exit outer file-read loop on match
    }
    // Consolidated single cleanup point for allocated search memory
    BufferManager::freeToMark(mark);
    if (rle_length > 0) {
        if (!pFile->seekSet(start_pos)) return 0;
    }
    return rle_length; // Returns calculated length or 0 if not found/seek failed
}

bool SnapZ80::checkZ80FileValidity(FatFile* pFile, Z80HeaderInfo* headerInfo) {
  bool result = false;
  uint32_t initial_file_pos = pFile->curPosition();

  // Hardware check: must be a 48K machine snapshot
  if (getMachineDetails(headerInfo->version, headerInfo->hw_mode) == MACHINE_48K) {

    // Version 2+ validation (Block-based payload)
    if (headerInfo->version >= Z80_VERSION_2) {
      result = true;  // Assume valid, invalidate if any block is corrupted
      while (pFile->available()) {
        uint8_t len_buf[3];  // compressed length (2 bytes), page number (1 byte)
        if (pFile->read(len_buf, 3) != 3) {
          result = false;  // Truncated block header
          break;
        }

        uint16_t compressed_len = len_buf[0] | (static_cast<uint16_t>(len_buf[1]) << 8);
        constexpr uint16_t UNCOMPRESSED_16KB = 0x4000;
        constexpr uint16_t UNCOMPRESSED_FLAG = 0xFFFF;
        uint16_t skip_len = (compressed_len == UNCOMPRESSED_FLAG) ? UNCOMPRESSED_16KB : compressed_len;
        if (!pFile->seekCur(skip_len)) {
          result = false;  // Truncated block payload
          break;
        }
      }
    }
    // Version 1 Compressed
    else if (headerInfo->isV1Compressed) {
      headerInfo->v1PayloadLength = locateV1Terminator(pFile, initial_file_pos);
      result = (headerInfo->v1PayloadLength > 0);
    }
    // Version 1 Uncompressed
    else {
      constexpr uint32_t RAM_48K_SIZE = 0xC000;
      result = (pFile->fileSize() >= (RAM_48K_SIZE + sizeof(SnapZ80::Z80V1Header)));
    }
  }
  pFile->seekSet(initial_file_pos);  // Restore original file position
  return result;
}

// Z80 format - Block decompression (supports the "ED ED [count] [value]" format)
__attribute__((optimize("-Ofast")))
void SnapZ80::decodeRLE_core(FatFile *pFile, uint16_t sourceLengthLimit, uint16_t currentAddress) {

	constexpr uint16_t PAYLOAD_SIZE = 255;

	uint16_t mark = BufferManager::getMark();
	uint8_t *fileReadBufferPtr = BufferManager::allocate(FILE_READ_BUFFER_SIZE);
	uint8_t *txBuffer = BufferManager::allocate(PAYLOAD_SIZE);

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
			uint8_t headerLen = sizeof (TransferPacket);
			TransferPacket header(currentAddress, commandPayloadPos); // commandPayloadPos will be the length
			Z80Bus::sendBytes8((uint8_t*)&header, headerLen);
			Z80Bus::sendBytes8(txBuffer, commandPayloadPos);
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
		if (b1 == 0xED) { 
			// Check if this 0xED is the very last byte of the compressed block!
			if (bytesReadFromSource >= sourceLengthLimit) {
				addByteToCommandPayloadBuffer(0xED);
				break; 
			}

			// maybe the start of compressed sequence
			uint8_t b2 = getNextByteFromFile();
			if (b2 == 0xED)	{							   
				// We are officially in a compressed block!  (Format: ED ED [count] [value])
				flushCommandPayloadBuffer();			   // Flush uncompressed bytes
				uint8_t runAmount = getNextByteFromFile(); // repeat count
				uint8_t value = getNextByteFromFile();	   // byte to repeat
				
		  		// NOTE: Amount added to address because the Z80 routine fills backwards.
				Fill8Packet header(currentAddress+runAmount, runAmount,value); 
				Z80Bus::sendBytes8((uint8_t *)&header, sizeof (Fill8Packet));

				currentAddress += runAmount;
			}
			else {
				// We found a 'ED' followed by something that is NOT 'ED'. 
				addByteToCommandPayloadBuffer(0xED);
				addByteToCommandPayloadBuffer(b2);
			}
		}
		else {
			addByteToCommandPayloadBuffer(b1); // Not 'ED' so just uncompressed data.
		}
	}

	flushCommandPayloadBuffer();	 // Send any leftover uncompressed bytes
	BufferManager::freeToMark(mark); // release all mallocs in this method
}

__attribute__((optimize("-Ofast")))
void SnapZ80::sendRawBytes_core(FatFile* pFile, uint16_t length, uint16_t currentAddress) {
  const uint16_t PAYLOAD_SIZE = 255;
  uint16_t mark = BufferManager::getMark();
  uint8_t* fileReadBufferPtr = BufferManager::allocate(FILE_READ_BUFFER_SIZE);
  uint8_t* txBuffer = BufferManager::allocate(PAYLOAD_SIZE);

  uint8_t commandPayloadPos = 0;
  uint16_t bytesReadFromSource = 0;

  while (bytesReadFromSource < length) {
    uint16_t remaining = length - bytesReadFromSource;
    uint16_t bytesToRead = min((uint16_t)FILE_READ_BUFFER_SIZE, remaining);
    int16_t bytesRead = pFile->read(fileReadBufferPtr, bytesToRead);
    if (bytesRead <= 0) break;

    for (int16_t i = 0; i < bytesRead; i++) {
      txBuffer[commandPayloadPos++] = fileReadBufferPtr[i];
      if (commandPayloadPos >= PAYLOAD_SIZE) {

		TransferPacket header(currentAddress, commandPayloadPos); // commandPayloadPos will be the length

        Z80Bus::sendBytes8((uint8_t*)&header, sizeof (TransferPacket));
        Z80Bus::sendBytes8(txBuffer, commandPayloadPos);
        currentAddress += commandPayloadPos;
        commandPayloadPos = 0;
      }
    }
    bytesReadFromSource += bytesRead;
  }

  // Flush any leftover bytes
  if (commandPayloadPos > 0) {
	TransferPacket header(currentAddress, commandPayloadPos); // commandPayloadPos will be the length
	uint8_t headerLen = sizeof (TransferPacket);
    Z80Bus::sendBytes8((uint8_t*)&header, headerLen);
    Z80Bus::sendBytes8(txBuffer, commandPayloadPos);
  }

  BufferManager::freeToMark(mark);
}

__attribute__((optimize("-Ofast")))
BlockReadResult SnapZ80::readAndWriteBlock(FatFile *pFile) {
	
	constexpr uint16_t UNCOMPRESSED_FLAG = 0xFFFF;

	uint8_t header[3]; // compressed length (2 bytes), page number (1 byte)
	if (pFile->read(header, sizeof(header)) != sizeof(header)) {
		return pFile->available() ? BLOCK_ERROR : BLOCK_END_OF_FILE;
	}

	const uint16_t compressed_len = header[0] | ((uint16_t)(header[1]) << 8);
	const uint8_t page_number = header[2];
	uint16_t mem_offset; // offset for 48K Z80 snapshot pages
	switch (page_number) {
	case 8:
		mem_offset = 0x4000;
		break;
	case 4:
		mem_offset = 0x8000;
		break;
	case 5:
		mem_offset = 0xC000;
		break;
	default:
		// Skip: uncompressed 0x4000 (16KB) chunk or compressed chunk
		uint16_t skip_len = (compressed_len == UNCOMPRESSED_FLAG) ? 0x4000 : compressed_len;
		if (!pFile->seekCur(skip_len)) {
			return BLOCK_ERROR;
		}
		return BLOCK_UNSUPPORTED_PAGE;
	}

	if (compressed_len == UNCOMPRESSED_FLAG) {
		sendRawBytes_core(pFile, 0x4000, mem_offset);   // uncompressed 16K blocks
	}
	else {
		decodeRLE_core(pFile, compressed_len, mem_offset);
	}

	return BLOCK_SUCCESS;
}

/*
 * Version 2/3 .z80 snapshot files can crash due to the conversion of z80->sna format i.e. convertSendZ80toSNA().
 * At times (down to bad luck), any .z80 snapshot captured by emulators while the game was abusing the stack pointer will not end well. 
 *
 *  UPDATE THIS TO USE DEDICATED Z80 FILE LOADER
 */

// .z80 files get converted to reuse existing ".SNA" game loading functionaliy
bool SnapZ80::convertSendZ80toSNA(FatFile* pFile, Z80HeaderInfo* headerInfo, Z80Registers* regs) {
  Z80V1Header* v1_header = &headerInfo->headerV1Data;

  if (headerInfo->version >= 2) {
    //
    // >>> V2 or V3 format <<<
    //
    v1_header->r &= ~0x80;
    while (true) {
      BlockReadResult block_result = readAndWriteBlock(pFile);
      if (block_result == BLOCK_END_OF_FILE) break;
      if (block_result == BLOCK_UNSUPPORTED_PAGE) {
        continue;
      }  // Skip to the next block

      if (block_result == BLOCK_ERROR) {
        return false;
      }
    }
  } else {
    //
    // >>> V1 Format <<<
    //
    if (headerInfo->isV1Compressed) {
      uint32_t rle_data_length = headerInfo->v1PayloadLength;
      decodeRLE_core(pFile, rle_data_length, ZX_SCREEN_ADDRESS_START);
    } else { // Uncompressed V1 data
      sendRawBytes_core(pFile, ZX_SPECTRUM_48K_TOTAL_MEMORY,
                        ZX_SCREEN_ADDRESS_START);
    }
  }

  regs->pc_lo = headerInfo->pc_low;
  regs->pc_hi = headerInfo->pc_high;

  return true;
}

void SnapZ80::convertZ80HeaderToSna(const Z80V1Header* z80Header_v1, Z80Registers* regs) {
    SNAHeader* snaHeader = &regs->header;

    snaHeader->i         = z80Header_v1->i;
    snaHeader->l_prime   = z80Header_v1->l_prime;
    snaHeader->h_prime   = z80Header_v1->h_prime;
    snaHeader->e_prime   = z80Header_v1->e_prime;
    snaHeader->d_prime   = z80Header_v1->d_prime;
    snaHeader->c_prime   = z80Header_v1->c_prime;
    snaHeader->b_prime   = z80Header_v1->b_prime;
    snaHeader->f_prime   = z80Header_v1->f_prime;
    snaHeader->a_prime   = z80Header_v1->a_prime;
    snaHeader->l         = z80Header_v1->l;
    snaHeader->h         = z80Header_v1->h;
    snaHeader->e         = z80Header_v1->e;
    snaHeader->d         = z80Header_v1->d;
    snaHeader->c         = z80Header_v1->c;
    snaHeader->b         = z80Header_v1->b;
    snaHeader->iyl       = z80Header_v1->iyl;
    snaHeader->iyh       = z80Header_v1->iyh;
    snaHeader->ixl       = z80Header_v1->ixl;
    snaHeader->ixh       = z80Header_v1->ixh;
    
    // Active interrupt flip-flop
    snaHeader->iff2      = z80Header_v1->iff1;
    
    // Combine 7-bit R register with bit 7 from flags1
    snaHeader->r         = (z80Header_v1->r & 0x7F) | ((z80Header_v1->flags1 & 0x01) << 7);
    
    snaHeader->f         = z80Header_v1->f;
    snaHeader->a         = z80Header_v1->a;
    snaHeader->sp_lo     = z80Header_v1->sp_lo;
    snaHeader->sp_hi     = z80Header_v1->sp_hi;
    
    // Bitfield masking for IM mode and border color
    snaHeader->im        = z80Header_v1->flags2 & 0x03; 
    snaHeader->borderCol = (z80Header_v1->flags1 >> 1) & 0x07; 

    // Store Program Counter in regs container
   // regs->pc_lo          = z80Header_v1->pc_lo;
   // regs->pc_hi          = z80Header_v1->pc_hi;
}
