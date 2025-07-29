#include "Constants.h"
#include "Z802SNA.h"
#include "SdCardSupport.h"

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

MachineType getMachineDetails(int16_t z80_version, uint8_t Z80_EXT_HW_MODE) {
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

__attribute__((optimize("-Ofast"))) 
int16_t readZ80Header(uint8_t* v1_header, uint8_t* pc_low, uint8_t* pc_high, uint8_t* hw_mode, bool* isV1Compressed) {
	if (SdCardSupport::fileRead(v1_header, Z80_V1_HEADERLENGTH) != Z80_V1_HEADERLENGTH) {
		return Z80_VERSION_UNKNOWN;  // Error reading header
	}

	if (v1_header[Z80_V1_PC_LOW] || v1_header[Z80_V1_PC_HIGH]) {
		*pc_low = v1_header[Z80_V1_PC_LOW];
		*pc_high = v1_header[Z80_V1_PC_HIGH];
		*isV1Compressed = (v1_header[Z80_V1_FLAGS1] & 0x20) >> 5;
		return Z80_VERSION_1;  // ... here we know PC must hold a valid address
	} else {                 // (PC==0) check for a valid V2/V3
		// Read into the file read buffer area
		if (SdCardSupport::fileRead(packetBuffer, FILE_READ_BUFFER_OFFSET, 2) != 2) return -1;
		uint8_t len_lo = packetBuffer[FILE_READ_BUFFER_OFFSET];
		uint8_t len_hi = packetBuffer[FILE_READ_BUFFER_OFFSET + 1];
		uint16_t ext_len = len_lo | ((uint16_t)len_hi << 8);

		uint16_t bytes_to_read_ext_header = ext_len;
		uint16_t bytes_read_so_far = 0;

		while (bytes_read_so_far < bytes_to_read_ext_header) {
			uint16_t chunk_size = min(FILE_READ_BUFFER_SIZE, (uint16_t)(bytes_to_read_ext_header - bytes_read_so_far));
			if (SdCardSupport::fileRead(packetBuffer, FILE_READ_BUFFER_OFFSET, chunk_size) != (int16_t)chunk_size) {
				return Z80_VERSION_UNKNOWN;  // Error reading extended header
			}

			for (uint16_t i = 0; i < chunk_size; i++) {
				uint16_t current_ext_byte_index = bytes_read_so_far + i;
				uint8_t b = packetBuffer[FILE_READ_BUFFER_OFFSET + i];
				switch (current_ext_byte_index) {  // Extracting only the needed bytes from extended header
					case Z80_EXT_PC_LOW: *pc_low = b; break;
					case Z80_EXT_PC_HIGH: *pc_high = b; break;
					case Z80_EXT_HW_MODE: *hw_mode = b; break;
				}
			}
			bytes_read_so_far += chunk_size;
		}

		return (ext_len == Z80_V2_HEADERLENGTH) ? Z80_VERSION_2 : Z80_VERSION_3;
	}
}

__attribute__((optimize("-Ofast"))) 
bool findMarkerOptimized(int32_t start_pos, uint32_t& rle_data_length) {
	static const uint8_t MARKER[] = { 0x00, 0xED, 0xED, 0x00 };
	constexpr uint8_t MARKER_SIZE = 4;
	uint8_t* search_buffer = &packetBuffer[FILE_READ_BUFFER_OFFSET];
	constexpr uint16_t SEARCH_BUFFER_SIZE = FILE_READ_BUFFER_SIZE;

	// Quick end-of-file check first
	int32_t file_size = SdCardSupport::fileSize();
	if (file_size != -1 && file_size >= (start_pos + MARKER_SIZE)) {
		int32_t potential_marker_pos = file_size - MARKER_SIZE;
		if (SdCardSupport::fileSeek(potential_marker_pos)) {
			if (SdCardSupport::fileRead(search_buffer, 0, MARKER_SIZE) == MARKER_SIZE && memcmp(search_buffer, MARKER, MARKER_SIZE) == 0) {
				rle_data_length = (uint32_t)(potential_marker_pos - start_pos);
				SdCardSupport::fileSeek(start_pos);  // Seek back to start position for caller
				return true;
			}
		}
		SdCardSupport::fileSeek(start_pos);  // Always seek back to start position
	}
	// Fallback to buffered search if marker not at end
	int32_t current_pos = start_pos;
	uint16_t overlap = 0;
	while (SdCardSupport::fileAvailable()) {
		uint16_t bytes_to_read = SEARCH_BUFFER_SIZE - overlap;
		int16_t bytes_read = SdCardSupport::fileRead(search_buffer, overlap, bytes_to_read);
		if (bytes_read == 0) break;
		uint16_t search_end = overlap + bytes_read;
		for (uint16_t i = 0; i <= search_end - MARKER_SIZE; i++) {
			if (memcmp(search_buffer + i, MARKER, MARKER_SIZE) == 0) {  // look for '0x00EDED00'
				rle_data_length = (uint32_t)((current_pos + i) - start_pos);
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
	return false;  // Marker not found
}

__attribute__((optimize("-Ofast"))) 
Z80CheckResult checkZ80FileValidity() {
	bool isV1Compressed = false;
	Z80CheckResult result = Z80_CHECK_SUCCESS;
	uint8_t* z80Header_v1 = &packetBuffer[0];
	uint8_t ext_pc_low, ext_pc_high, ext_hw_mode = 0;
	int32_t initial_file_pos = SdCardSupport::filePosition();
	if (initial_file_pos == -1) return Z80_CHECK_ERROR_UNEXPECTED_EOF;  // Error getting position
	int16_t header_result = readZ80Header(z80Header_v1, &ext_pc_low, &ext_pc_high, &ext_hw_mode, &isV1Compressed);

	if (header_result < 0) {
		result = Z80_CHECK_ERROR_READ_HEADER;
	} else if (getMachineDetails(header_result, ext_hw_mode) != MACHINE_48K) {  // Check machine type (48K only for now)
		result = Z80_CHECK_ERROR_UNSUPPORTED_TYPE;
	} else {
		if (header_result >= Z80_VERSION_2) {  // V2 or V3
			while (SdCardSupport::fileAvailable()) {
				uint32_t current_pos = SdCardSupport::filePosition();
				if (SdCardSupport::fileRead(packetBuffer, FILE_READ_BUFFER_OFFSET, 2) != 2) {  // Read 2 bytes for compressed_length
					if (!SdCardSupport::fileAvailable()) break;                                  // After reading a full block, it's fine if EOF
					result = Z80_CHECK_ERROR_BLOCK_STRUCTURE;                                    // Incomplete compressed_length
					break;
				}
				uint16_t compressed_length = (uint16_t)packetBuffer[FILE_READ_BUFFER_OFFSET] + (uint16_t)packetBuffer[FILE_READ_BUFFER_OFFSET + 1] * 0x100;

				if (SdCardSupport::fileRead(packetBuffer, FILE_READ_BUFFER_OFFSET, 1) != 1) {  // Read 1 byte for page_number
					result = Z80_CHECK_ERROR_BLOCK_STRUCTURE;                                    // Incomplete block (missing page number)
					break;
				}
				uint8_t page_number = packetBuffer[FILE_READ_BUFFER_OFFSET];
				if ((page_number != 8) || (page_number != 4) || (page_number != 5)) {
					break;  // unsupported page
				}

				// skip: The 3 bytes are for compressed_length (2) + page_number (1)
				uint32_t target_pos = current_pos + 3 + compressed_length;
				if (!SdCardSupport::fileSeek(target_pos)) {
					result = Z80_CHECK_ERROR_EOF;  // Failed to seek, likely file too short for the stated compressed_length
					break;
				}
			}
		} else {  // V1
			bool isCompressedV1 = (z80Header_v1[Z80_V1_FLAGS1] & 0x20) >> 5;
			if (isCompressedV1) {
				int32_t start_of_rle_data = Z80_V1_HEADERLENGTH;
				uint32_t dummy_length;
				if (!findMarkerOptimized(start_of_rle_data, dummy_length)) {
					result = Z80_CHECK_ERROR_V1_MARKER_NOT_FOUND;
				}

			} else {  // For uncompressed V1, the data is just raw bytes, 48K total.
				int32_t current_pos = SdCardSupport::filePosition();
				if (current_pos == -1) { result = Z80_CHECK_ERROR_UNEXPECTED_EOF; }
				if (SdCardSupport::fileSize() != (0xC000 + Z80_V1_HEADERLENGTH)) { result = Z80_CHECK_ERROR_UNEXPECTED_EOF; }
			}
		}
	}

	if (!SdCardSupport::fileSeek(initial_file_pos)) {  // Restore position to before the check
		return Z80_CHECK_ERROR_SEEK;
	}
	return result;
}

__attribute__((optimize("-Ofast"))) 
static void decodeRLE_core(uint32_t sourceLengthLimit, uint16_t currentAddress) {
	const uint8_t MAX_PAYLOAD_CHUNK_SIZE = COMMAND_PAYLOAD_SECTION_SIZE;  // Max size for 'G' command payload
	uint8_t* commandPayloadPtr = &packetBuffer[SIZE_OF_HEADER];           // Points to the payload area for commands
	uint16_t commandPayloadPos = 0;                                       // Current position within the command payload buffer
	uint8_t* fileReadBufferPtr = &packetBuffer[FILE_READ_BUFFER_OFFSET];  // File read buffer
	uint16_t fileReadBufferCurrentPos = 0;
	uint16_t fileReadBufferBytesAvailable = 0;
	uint32_t bytesReadFromSource = 0;

	auto getNextByteFromFile = [&]() -> uint8_t {
		if (fileReadBufferCurrentPos >= fileReadBufferBytesAvailable) {
			uint16_t bytesToRead = min((uint16_t)FILE_READ_BUFFER_SIZE, (uint16_t)(sourceLengthLimit - bytesReadFromSource));
			if (bytesToRead == 0) return 0;  // Should not happen due to validation

			int16_t bytesJustRead = SdCardSupport::fileRead(fileReadBufferPtr, bytesToRead);
			fileReadBufferBytesAvailable = (uint16_t)bytesJustRead;
			fileReadBufferCurrentPos = 0;
		}
		bytesReadFromSource++;
		return fileReadBufferPtr[fileReadBufferCurrentPos++];
	};

	auto flushCommandPayloadBuffer = [&]() {
		if (commandPayloadPos > 0) {
			START_UPLOAD_COMMAND(packetBuffer, 'G', commandPayloadPos);
			ADDR_UPLOAD_COMMAND(packetBuffer, currentAddress);
			Z80Bus::sendBytes(packetBuffer, SIZE_OF_HEADER + commandPayloadPos);
			currentAddress += commandPayloadPos;
			commandPayloadPos = 0;
		}
	};

	auto addByteToCommandPayloadBuffer = [&](uint8_t byte) {
		commandPayloadPtr[commandPayloadPos++] = byte;
		if (commandPayloadPos >= MAX_PAYLOAD_CHUNK_SIZE) {
			flushCommandPayloadBuffer();
		}
	};

	while (bytesReadFromSource < sourceLengthLimit) {
		uint8_t b1 = getNextByteFromFile();
		if (b1 == 0xED) {
			uint8_t b2 = getNextByteFromFile();
			if (b2 == 0xED) {
				flushCommandPayloadBuffer();  // Flush any previous payload before a fill command
				uint8_t runAmount = getNextByteFromFile();
				uint8_t value = getNextByteFromFile();
				SMALL_FILL_COMMAND(packetBuffer,runAmount, currentAddress, value);
				Z80Bus::sendBytes(packetBuffer, 5);
				currentAddress += runAmount;
			} else {
				addByteToCommandPayloadBuffer(b1);
				addByteToCommandPayloadBuffer(b2);
			}
		} else {
			addByteToCommandPayloadBuffer(b1);
		}
	}
	flushCommandPayloadBuffer();
}

__attribute__((optimize("-Ofast"))) 
BlockReadResult z80_readAndWriteBlock(uint8_t* page_number_out) { 
	uint16_t compressed_length;

	// Read compressed_length (2 bytes, little-endian) into the file read buffer
	if (SdCardSupport::fileRead(packetBuffer, FILE_READ_BUFFER_OFFSET, 2) != 2) {
		*page_number_out = (uint8_t)-1;         // Indicate end of file or read error for page number
		if (!SdCardSupport::fileAvailable()) {  // Check if it's genuinely EOF
			return BLOCK_END_OF_FILE;
		}
		return BLOCK_ERROR_READ_LENGTH;
	}
	compressed_length = (uint16_t)packetBuffer[FILE_READ_BUFFER_OFFSET] + (uint16_t)packetBuffer[FILE_READ_BUFFER_OFFSET + 1] * 0x100;

	// Read page_number into the file read buffer
	if (SdCardSupport::fileRead(packetBuffer, FILE_READ_BUFFER_OFFSET, 1) != 1) {
		*page_number_out = (uint8_t)-1;  // Indicate end of file or read error
		return BLOCK_ERROR_READ_PAGE;
	}
	*page_number_out = packetBuffer[FILE_READ_BUFFER_OFFSET];

	int32_t sna_offset = -1;
	if (*page_number_out == 8) sna_offset = 0x4000;       // Page 8 maps to 0x4000-0x7FFF (first 16K block after header)
	else if (*page_number_out == 4) sna_offset = 0x8000;  // Page 4 maps to 0x8000-0xBFFF 
	else if (*page_number_out == 5) sna_offset = 0xC000;  // Page 5 maps to 0xC000-0xFFFF 
		                                                    // Add more cases for 128K pages (10, 11, etc.) if needed in the future
	if (sna_offset == -1) {
		// looks like the .z80 files I've tried don't need this - but playing it safe
		// Consume the data block using the file read buffer
		uint16_t bytes_consumed = 0;
		while (bytes_consumed < compressed_length) {
			uint16_t bytes_to_read_this_chunk = min(FILE_READ_BUFFER_SIZE, (uint16_t)(compressed_length - bytes_consumed));
			if (SdCardSupport::fileRead(packetBuffer, FILE_READ_BUFFER_OFFSET, bytes_to_read_this_chunk) != (int16_t)bytes_to_read_this_chunk) {
				return BLOCK_ERROR_READ_DATA;  // Failed to consume all expected bytes
			}
			bytes_consumed += bytes_to_read_this_chunk;
		}
		return BLOCK_UNSUPPORTED_PAGE;
	}

	decodeRLE_core(compressed_length, /* bytesWrittenActual, */ (uint16_t)sna_offset);
	return BLOCK_SUCCESS;
}

int16_t convertZ80toSNA_impl() {
	bool isV1Compressed = false;
	uint8_t ext_pc_low = 0, ext_pc_high = 0, ext_hw_mode = 0;
	uint16_t stackOffsetForPushingPC = 0;
	uint8_t* temp_z80Header_v1 = &packetBuffer[0];  // Header will be read into the start of packetBuffer
	uint8_t* snaHeader = &head27_Execute[0 + 1];    // +1 leaving room for 'E' command

	int16_t z80_version = readZ80Header(temp_z80Header_v1, &ext_pc_low, &ext_pc_high, &ext_hw_mode, &isV1Compressed);
	if (z80_version == Z80_VERSION_UNKNOWN) {
		return BLOCK_ERROR_VERSION;
	}

	if (z80_version >= 2) {
		// Patch PC and R from extended header
		temp_z80Header_v1[Z80_V1_PC_LOW] = ext_pc_low;
		temp_z80Header_v1[Z80_V1_PC_HIGH] = ext_pc_high;
		temp_z80Header_v1[Z80_V1_R_7BITS] &= ~0x80;

		stackOffsetForPushingPC = Z802SNA::convertHeaders(temp_z80Header_v1, snaHeader);

		while (true) {
			uint8_t page_number;
			BlockReadResult block_result = z80_readAndWriteBlock(&page_number); //, &uncompressed_length);
			if (block_result == BLOCK_END_OF_FILE) break;
			if (block_result == BLOCK_UNSUPPORTED_PAGE) { continue; } // Skip to the next block
			if (block_result<0) { return block_result; }  // negative critical errors
		}
	} else {                                     // version 1
		const uint32_t DEST_BUFFER_SIZE = 0xC000;  // Expected uncompressed size (48K machine)
		stackOffsetForPushingPC = Z802SNA::convertHeaders(temp_z80Header_v1, snaHeader);

		if (isV1Compressed) {
			int32_t start_of_rle_data = SdCardSupport::filePosition();
			if (start_of_rle_data == -1) return -1;
			uint32_t rle_data_length = 0;
			if (!findMarkerOptimized(start_of_rle_data, rle_data_length) || rle_data_length == 0) {
				return BLOCK_ERROR_NO_V1_MARKER;
			}
			decodeRLE_core(rle_data_length,/* bytesWrittenToOutput,*/ 0x4000);

			// Skip past the marker (findMarkerOptimized leaves us at the start, so we need to skip past data + marker)
			int32_t expected_pos_after_marker = start_of_rle_data + rle_data_length + 4;
			if (SdCardSupport::filePosition() < expected_pos_after_marker) {
				if (!SdCardSupport::fileSeek(expected_pos_after_marker)) {
					return -1;  // Failed to seek past marker
				}
			}
		} else {  	// Uncompressed V1 data
		  decodeRLE_core(DEST_BUFFER_SIZE, /*, bytesWrittenToOutput, */ 0x4000);
		}
	}

	// Fake push 'PC' onto the stack
	// NOTE: We are currently reusing existing ".SNA" functionaliy for loading the final Z80's CPU registers.
	//       This can be changed later to use a more effecient standard when sending to the Speccy.
	uint8_t bytesRead = 2;
	START_UPLOAD_COMMAND(packetBuffer, 'G', bytesRead);
	ADDR_UPLOAD_COMMAND(packetBuffer, stackOffsetForPushingPC);
	packetBuffer[SIZE_OF_HEADER] = ext_pc_low;
	packetBuffer[SIZE_OF_HEADER + 1] = ext_pc_high;
	Z80Bus::sendBytes(packetBuffer, SIZE_OF_HEADER + 2);

	return BLOCK_SUCCESS;
}

int16_t convertZ80toSNA() {

	const uint16_t address = 0x4004;  // Temporary Z80 jump target in screen memory.
	packetBuffer[0] = 'S';            // 'S' command: Set Stack Pointer (SP) on Z80.
	packetBuffer[1] = (uint8_t)(address >> 8);
	packetBuffer[2] = (uint8_t)(address & 0xFF);
	Z80Bus::sendBytes(packetBuffer, 3);  // 3 = character command + 16bit address
	Z80Bus::waitRelease_NMI();           //Synchronize: Z80 knows it must halt after loading SP - Aruindo waits for NMI release.

	Z80CheckResult fileCheck=checkZ80FileValidity();
	if (fileCheck!=Z80_CHECK_SUCCESS) { 
		//SdCardSupport::fileClose();
		return fileCheck;	
	}

 	int16_t conversionResult = convertZ80toSNA_impl();
	//SdCardSupport::fileClose();
	if (conversionResult<0) { return conversionResult; }
	return Z80_CHECK_SUCCESS;
}
