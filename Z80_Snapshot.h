#include "Constants.h"
#include "Z802SNA.h"
#include "SdCardSupport.h"

extern boolean bootFromSnapshot_z80_end();
extern int readZ80Header( uint8_t* v1_header, uint8_t* pc_low, uint8_t* pc_high, uint8_t* hw_mode, bool* isV1Compressed);


MachineType getMachineDetails(int z80_version, uint8_t Z80_EXT_HW_MODE) {
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

// Function to perform a quick pre-check of the Z80 file's validity
Z80CheckResult checkZ80FileValidity() {
	bool isV1Compressed = false;
	Z80CheckResult result = Z80_CHECK_SUCCESS;
	uint8_t* z80Header_v1 = &packetBuffer[0];
	uint8_t ext_pc_low, ext_pc_high, ext_hw_mode = 0;
	int header_result = readZ80Header(z80Header_v1, &ext_pc_low, &ext_pc_high, &ext_hw_mode, &isV1Compressed);

	if (header_result < 0) {
		result = Z80_CHECK_ERROR_READ_HEADER;
	} else if (getMachineDetails(header_result, ext_hw_mode) != MACHINE_48K) {  // Check machine type (48K only for now)
		result = Z80_CHECK_ERROR_UNSUPPORTED_TYPE;
	} else {
		if (header_result >= Z80_VERSION_2) {  // V2 or V3
			while (SdCardSupport::fileAvailable()) {
				uint16_t current_pos = SdCardSupport::filePosition();
				int byte1 = SdCardSupport::fileRead();
				int byte2 = SdCardSupport::fileRead();
				if (byte1 == EOF || byte2 == EOF) {
					if (byte1 == EOF && byte2 == EOF) break;   // After reading a full block, it's fine
					result = Z80_CHECK_ERROR_BLOCK_STRUCTURE;  // Incomplete compressed_length
					break;
				} else {
					uint16_t compressed_length = (unsigned int)byte1 + (unsigned int)byte2 * 0x100;
					int page_number = SdCardSupport::fileRead();
					if (page_number == EOF) {
						result = Z80_CHECK_ERROR_BLOCK_STRUCTURE;  // Incomplete block (missing page number)
						break;
					} else { // skip: The 3 bytes are for compressed_length (2) + page_number (1)
						uint16_t target_pos = current_pos + 3 + compressed_length;
						if (!SdCardSupport::fileSeek(target_pos)) {
							result = Z80_CHECK_ERROR_EOF;  // Failed to seek, likely file too short for the stated compressed_length
							break;
						}
					}
				}
			}
		} else {  // V1
			bool isCompressedV1 = (z80Header_v1[Z80_V1_FLAGS1] & 0x20) >> 5;
			if (isCompressedV1) {

				int16_t current_byte;
				uint8_t marker_buffer[4] = { 0, 0, 0, 0 };
				bool marker_found = false;

				while (SdCardSupport::fileAvailable()) {
					current_byte = SdCardSupport::fileRead();
					if (current_byte == -1) { break; }  // Should not happen if file is valid - now marker not found

					marker_buffer[0] = marker_buffer[1];
					marker_buffer[1] = marker_buffer[2];
					marker_buffer[2] = marker_buffer[3];
					marker_buffer[3] = (uint8_t)current_byte;
					if (marker_buffer[0] == 0x00 && marker_buffer[1] == 0xED && marker_buffer[2] == 0xED && marker_buffer[3] == 0x00) {
						marker_found = true;
						break;
					}
				}
				if (!marker_found) { result = Z80_CHECK_ERROR_V1_MARKER_NOT_FOUND; }

			} else {  // For uncompressed V1, the data is just raw bytes, 48K total.
				int16_t current_pos = SdCardSupport::filePosition();
				if (current_pos == -1) { result = Z80_CHECK_ERROR_UNEXPECTED_EOF; }
				if (current_pos != Z80_V1_HEADERLENGTH) { result = Z80_CHECK_ERROR_READ_HEADER; }  // at least 30bytes header
				if (SdCardSupport::fileSize() != (0xc000 + Z80_V1_HEADERLENGTH)) { result = Z80_CHECK_ERROR_UNEXPECTED_EOF; }
			}
		}
	}

	SdCardSupport::fileSeek(0);
	return result;
}

static RLEDecodeResult decodeRLE_core(unsigned sourceLengthLimit, unsigned& bytesWrittenToOutput, uint16_t currentAddress) {
	const uint8_t MAX_CHUNK_SIZE = PAYLOAD_BUFFER_SIZE;  // Adjust based on your packetBuffer size
	uint8_t* bufferPtr = &packetBuffer[SIZE_OF_HEADER];
	uint16_t bufferPos = 0;
	int b1 = 0, b2 = 0;
	unsigned bytesReadFromSource = 0;
	bytesWrittenToOutput = 0;

	FatFile& file = (SdCardSupport::file);

	auto flushBuffer = [&]() {
		if (bufferPos > 0) {
			START_UPLOAD_COMMAND(packetBuffer, 'G', bufferPos);  //size
			END_UPLOAD_COMMAND(packetBuffer, currentAddress);
			Z80Bus::sendBytes(packetBuffer, SIZE_OF_HEADER + bufferPos);
			currentAddress += bufferPos;
			bytesWrittenToOutput += bufferPos;
			bufferPos = 0;
		}
	};

	auto addByteToBuffer = [&](uint8_t byte) {
		bufferPtr[bufferPos++] = byte;
		if (bufferPos >= MAX_CHUNK_SIZE) {
			flushBuffer();
		}
	};

	while (bytesReadFromSource < sourceLengthLimit) {
		b1 = file.read();
		bytesReadFromSource++;
		if (b1 == 0xED) {
			b2 = file.read();
			bytesReadFromSource++;
			if (b2 == 0xED) {
				flushBuffer();
				int run = file.read();
				int value = file.read();
				bytesReadFromSource += 2;
				const uint16_t amount = run;
				packetBuffer[0] = 'F';                           // 'Fill' command for Z80 memory operation.
				packetBuffer[1] = (amount >> 8) & 0xFF;          // High byte of fill amount.
				packetBuffer[2] = amount & 0xFF;                 // Low byte of fill amount.
				packetBuffer[3] = (currentAddress >> 8) & 0xFF;  // High byte of fill start address.
				packetBuffer[4] = currentAddress & 0xFF;         // Low byte of fill start address.
				packetBuffer[5] = value;                         // Value to fill
				Z80Bus::sendBytes(packetBuffer, 6);              // Sends fill command.
				currentAddress += amount;
				bytesWrittenToOutput += amount;
			} else {
				addByteToBuffer((uint8_t)b1);
				addByteToBuffer((uint8_t)b2);
			}
		} else {
			addByteToBuffer((uint8_t)b1);
		}
	}
	flushBuffer();
	return RLE_OK;
}

int readZ80Header(uint8_t* v1_header, uint8_t* pc_low, uint8_t* pc_high, uint8_t* hw_mode, bool* isV1Compressed) {

	for (int i = 0; i < Z80_V1_HEADERLENGTH; i++) {
		int b = SdCardSupport::fileRead();
		if (b == EOF) return -1;
		v1_header[i] = b;
	}

	if (v1_header[Z80_V1_PC_LOW] || v1_header[Z80_V1_PC_HIGH]) {
		*pc_low = v1_header[Z80_V1_PC_LOW];
		*pc_high = v1_header[Z80_V1_PC_HIGH];
		*isV1Compressed = (v1_header[Z80_V1_FLAGS1] & 0x20) >> 5;
		return Z80_VERSION_1;  // ... here we know PC must hold a valid address
	} else {    // (PC==0) check for a valid V2/V3
		int len_lo = SdCardSupport::fileRead();
		int len_hi = SdCardSupport::fileRead();
		if (len_lo == EOF || len_hi == EOF) return -1;  // Extended header length (v2/v3)
		unsigned ext_len = len_lo | (len_hi << 8);

		for (unsigned i = 0; i < ext_len; i++) {
			int b = SdCardSupport::fileRead();
			if (b == EOF) return Z80_VERSION_UNKNOWN;
			switch (i) {  // Extracting only the needed bytes from extended header
				case Z80_EXT_PC_LOW: *pc_low = b; break;
				case Z80_EXT_PC_HIGH: *pc_high = b; break;
				case Z80_EXT_HW_MODE: *hw_mode = b; break;
			}
		}
		return (ext_len == Z80_V2_HEADERLENGTH) ? Z80_VERSION_2 : Z80_VERSION_3;
	}
}

BlockReadResult z80_readAndWriteBlock(  int* page_number_out, unsigned* uncompressed_length_out) {
	unsigned int compressed_length;
	const unsigned int BLOCK_SIZE = 0x4000;  // 16 KB for memory pages (65536 bytes / 4 = 16384 bytes)

	// Read compressed_length (2 bytes, little-endian)
	int byte1 = SdCardSupport::fileRead();
	int byte2 = SdCardSupport::fileRead();

	if (byte1 == EOF || byte2 == EOF) {
		*page_number_out = -1;                  // Indicate end of file or read error for page number
		if (!SdCardSupport::fileAvailable()) {  // Check if it's genuinely EOF
			return BLOCK_END_OF_FILE;
		}
		return BLOCK_ERROR_READ_LENGTH;
	}
	compressed_length = (unsigned int)byte1 + (unsigned int)byte2 * 0x100;
	*page_number_out = SdCardSupport::fileRead();  // Read page_number
	if (*page_number_out == EOF) {
		*page_number_out = -1;  // Indicate end of file or read error
		return BLOCK_ERROR_READ_PAGE;
	}

	long sna_offset = -1;
	if (*page_number_out == 8) sna_offset = 0x4000;           // Page 8 maps to 0x4000-0x7FFF (first 16K block after header)
	else if (*page_number_out == 4) sna_offset = 0 + 0x8000;  // Page 4 maps to 0x8000-0xBFFF
	else if (*page_number_out == 5) sna_offset = 0 + 0xC000;  // Page 5 maps to 0xC000-0xFFFF
	                                                          // Add more cases for 128K pages (10, 11, etc.) if needed in the future
	if (sna_offset == -1) {
		for (unsigned int i = 0; i < compressed_length; ++i) {
			SdCardSupport::fileRead();  // Consume the bytes
		}
		return BLOCK_ERROR_UNSUPPORTED_PAGE;
	}

	unsigned bytesWrittenActual = 0;
	RLEDecodeResult result = decodeRLE_core(compressed_length, bytesWrittenActual,sna_offset);

	*uncompressed_length_out = bytesWrittenActual;  // Update the output parameter
	if (result != RLE_OK) { return BLOCK_ERROR_READ_DATA; }
	if (*uncompressed_length_out > BLOCK_SIZE) { return BLOCK_ERROR_READ_DATA; }
	return BLOCK_SUCCESS;
}

int convertZ80toSNA_impl() {

	bool isV1Compressed=false;
	uint8_t ext_pc_low = 0, ext_pc_high = 0, ext_hw_mode = 0;
	uint16_t stackOffsetForPushingPC = 0;
	uint8_t* temp_z80Header_v1 = &packetBuffer[0];
	uint8_t* snaHeader = &head27_Execute[0 + 1];  // +1 leaving room for 'E' command

	int z80_version = readZ80Header(temp_z80Header_v1, &ext_pc_low, &ext_pc_high, &ext_hw_mode, &isV1Compressed);
	if (z80_version==-1) {
		return -1;
	}

	if (z80_version >= 2) {
		// Patch PC and R from extended header
		temp_z80Header_v1[Z80_V1_PC_LOW] = ext_pc_low;
		temp_z80Header_v1[Z80_V1_PC_HIGH] = ext_pc_high;
		temp_z80Header_v1[Z80_V1_R_7BITS] &= ~0x80;

		stackOffsetForPushingPC = Z802SNA::convertHeaders(temp_z80Header_v1, snaHeader);

		while (true) {
			int page_number;
			unsigned uncompressed_length = 0;
			BlockReadResult block_result = z80_readAndWriteBlock(&page_number, &uncompressed_length);
			if (block_result == BLOCK_END_OF_FILE) break;
			if (block_result != BLOCK_SUCCESS) return -1;
		}
	} else {  // version 1
		const unsigned DEST_BUFFER_SIZE = 0xC000;
		stackOffsetForPushingPC = Z802SNA::convertHeaders(temp_z80Header_v1, snaHeader);
		unsigned bytesWrittenToOutput = 0;
		if (isV1Compressed) {
			long start_of_rle_data = SdCardSupport::filePosition();
			if (start_of_rle_data == -1) return -1;
			uint8_t marker_buffer[4] = { 0, 0, 0, 0 };
			unsigned rle_data_length = 0;
			long current_pos = start_of_rle_data;
			while (SdCardSupport::fileAvailable()) {
				int b = SdCardSupport::fileRead();
				current_pos++;
				if (b == EOF) break;
				marker_buffer[0] = marker_buffer[1];
				marker_buffer[1] = marker_buffer[2];
				marker_buffer[2] = marker_buffer[3];
				marker_buffer[3] = b;
				if (marker_buffer[0] == 0x00 && marker_buffer[1] == 0xED && marker_buffer[2] == 0xED && marker_buffer[3] == 0x00) {
					rle_data_length = current_pos - start_of_rle_data - 4;
					break;
				}
			}

			if (rle_data_length == 0 || !SdCardSupport::fileSeek(start_of_rle_data)) {
				return -1;
			}
			RLEDecodeResult result = decodeRLE_core(rle_data_length, bytesWrittenToOutput, 0x4000);
			if (result != RLE_OK || bytesWrittenToOutput != DEST_BUFFER_SIZE) {
				return -1;
			}
			for (int i = 0; i < 4; ++i) {
				if (SdCardSupport::fileRead() == EOF) return -1;
			}
		} else {
			RLEDecodeResult result = decodeRLE_core(DEST_BUFFER_SIZE, bytesWrittenToOutput, 0x4000);
			if (result != RLE_OK || bytesWrittenToOutput != DEST_BUFFER_SIZE) {
				return -1;
			}
		}
	}

	uint8_t bytesRead = 2;
	START_UPLOAD_COMMAND(packetBuffer, 'G', bytesRead);
	END_UPLOAD_COMMAND(packetBuffer, stackOffsetForPushingPC);
	packetBuffer[SIZE_OF_HEADER] =  ext_pc_low;
	packetBuffer[SIZE_OF_HEADER + 1] = ext_pc_high;
	Z80Bus::sendBytes(packetBuffer, SIZE_OF_HEADER + 2);

	SdCardSupport::fileClose();

	return 0;
}



// The outer wrapper function for resource setup and teardown
int convertZ80toSNA() {  // const std::string& fileName) {

	const uint16_t address = 0x4004;  // Temporary Z80 jump target in screen memory.
	packetBuffer[0] = 'S';            // 'S' command: Set Stack Pointer (SP) on Z80.
	packetBuffer[1] = (uint8_t)(address >> 8);
	packetBuffer[2] = (uint8_t)(address & 0xFF);
	Z80Bus::sendBytes(packetBuffer, 3);  // 3 = character command + 16bit address
	Z80Bus::waitRelease_NMI();           //Synchronize: Z80 knows it must halt after loading SP - Aruindo waits for NMI release.


	switch (checkZ80FileValidity()) {
		case Z80_CHECK_SUCCESS: break;  //  oled.println("Z80 FILE OK"); break;
		case Z80_CHECK_ERROR_OPEN_FILE: return 1;
		case Z80_CHECK_ERROR_READ_HEADER: return 2;
		case Z80_CHECK_ERROR_V1_MARKER_NOT_FOUND: return 3;
		case Z80_CHECK_ERROR_BLOCK_STRUCTURE: return 4;
		case Z80_CHECK_ERROR_UNEXPECTED_EOF: return 5;
		case Z80_CHECK_ERROR_UNSUPPORTED_TYPE: return 6;
		default: return 7;
	}

	int result = convertZ80toSNA_impl();

	bootFromSnapshot_z80_end();

	return result;
}
