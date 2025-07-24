#include "Constants.h"
#include "Z802SNA.h"
#include "SdCardSupport.h"

extern boolean bootFromSnapshot_z80_end();
extern int readZ80Header( uint8_t* v1_header, uint8_t* pc_low, uint8_t* pc_high, uint8_t* hw_mode);


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
Z80CheckResult checkZ80FileValidity(const char* fileName) { 
	Z80CheckResult result = Z80_CHECK_SUCCESS;

//	if (!SdCardSupport::openFileByName(fileName)) {
	//	oled.println("Failed open file!");
	//}

	uint8_t* z80Header_v1 = &packetBuffer[0];
	uint8_t ext_pc_low, ext_pc_high, ext_hw_mode = 0;
	//unsigned int extHeaderLength = 0;
	int header_result = readZ80Header(  z80Header_v1, &ext_pc_low, &ext_pc_high, &ext_hw_mode);

	if (header_result < 0) {
		result = Z80_CHECK_ERROR_READ_HEADER;
	} else if (getMachineDetails(header_result, ext_hw_mode) != MACHINE_48K) {  // Check machine type (48K only for now)
		result = Z80_CHECK_ERROR_UNSUPPORTED_TYPE;
	} else {

		if (header_result >= Z80_VERSION_2) {  // V2 or V3

			while (SdCardSupport::fileAvailable()) {  
//				uint16_t current_pos = SdCardSupport::filePosition(); 
//  			if (current_pos == -1) {
//					result = Z80_CHECK_ERROR_UNEXPECTED_EOF;
//				}
				uint16_t current_pos = SdCardSupport::filePosition(); 
				int byte1 = SdCardSupport::fileRead();
				int byte2 = SdCardSupport::fileRead();
				if (byte1 == EOF || byte2 == EOF) {
					// If EOF is reached exactly after reading a full block, it's fine.
					// If we read 0 or 1 byte and then EOF, it's an incomplete block header.
					if (byte1 == EOF && byte2 == EOF) break;   // Reached EOF before reading any bytes for next block
					result = Z80_CHECK_ERROR_BLOCK_STRUCTURE;  // Incomplete compressed_length
					break;
				} else {
					uint16_t compressed_length = (unsigned int)byte1 + (unsigned int)byte2 * 0x100;
					int page_number = SdCardSupport::fileRead();
					if (page_number == EOF) {
						result = Z80_CHECK_ERROR_BLOCK_STRUCTURE;  // Incomplete block (missing page number)
						break;
					} else {
						// Skip the compressed data for this block
						// The 3 bytes are for compressed_length (2) + page_number (1)
				
						uint16_t target_pos = current_pos + 3 + compressed_length;
						if (!SdCardSupport::fileSeek(target_pos)) {
							result = Z80_CHECK_ERROR_EOF;	// Failed to seek, likely file too short for the stated compressed_length
							break;
						}
					}
				}
			}
		} else {  // V1
			bool isCompressedV1 = (z80Header_v1[Z80_V1_FLAGS1] & 0x20) >> 5;
			if (isCompressedV1) {
				int16_t start_of_rle_data = SdCardSupport::filePosition();
				if (start_of_rle_data == -1) { /*z80_file_stream.close();*/
					result = Z80_CHECK_ERROR_UNEXPECTED_EOF;
				} else {
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
				}
			} else {
				// For uncompressed V1, the data is just raw bytes, 48K total.
				// Check if the file is at least 30 (header) + 49152 (memory) bytes long.
				//int current_pos = ftell(z80_file_stream.f);
				int16_t current_pos = SdCardSupport::filePosition();  //    z80_file_stream.position();

				if (current_pos == -1) { result = Z80_CHECK_ERROR_UNEXPECTED_EOF; }
				if (current_pos != Z80_V1_HEADERLENGTH) { result = Z80_CHECK_ERROR_READ_HEADER; }
				if (SdCardSupport::fileSize() != (0xc000 + Z80_V1_HEADERLENGTH)) { result = Z80_CHECK_ERROR_UNEXPECTED_EOF; }
			}
		}
	}

	//SdCardSupport::fileClose();
	SdCardSupport::fileSeek(0);
	return result;
}


//uint16_t currentAddress = 0x4000;

static RLEDecodeResult decodeRLE_core(  unsigned maxOutputSize, unsigned sourceLengthLimit, unsigned& bytesWrittenToOutput, uint16_t currentAddress) {

	bytesWrittenToOutput = 0;
	int b1 = 0, b2 = 0;
	unsigned bytesReadFromSource = 0;

	while (bytesReadFromSource < sourceLengthLimit) {
		b1 = SdCardSupport::fileRead();
		bytesReadFromSource++;
		if (b1 == 0xED) {
			b2 = SdCardSupport::fileRead();
			bytesReadFromSource++;
			if (b2 == 0xED) {
				int run = SdCardSupport::fileRead();
				int value = SdCardSupport::fileRead();
				bytesReadFromSource += 2;

				for (int i = 0; i < run; ++i) {
					uint8_t bytesRead = 1;
					START_UPLOAD_COMMAND(packetBuffer, 'G', bytesRead);
					END_UPLOAD_COMMAND(packetBuffer, currentAddress);
					packetBuffer[SIZE_OF_HEADER] = value;
					Z80Bus::sendBytes(packetBuffer, SIZE_OF_HEADER + 1);
					currentAddress += 1;

					bytesWrittenToOutput++;
					if (bytesWrittenToOutput > maxOutputSize) {
						oled.print("RLE_ERROR");
						return RLE_ERROR_OUTPUT_OVERFLOW;
					}
				}
			} else {
				uint8_t bytesRead = 2;
				START_UPLOAD_COMMAND(packetBuffer, 'G', bytesRead);
				END_UPLOAD_COMMAND(packetBuffer, currentAddress);
				packetBuffer[SIZE_OF_HEADER] = b1;
				packetBuffer[SIZE_OF_HEADER + 1] = b2;
				Z80Bus::sendBytes(packetBuffer, SIZE_OF_HEADER + 2);
				currentAddress += 2;

				bytesWrittenToOutput += 2;
			}
		} else {
			uint8_t bytesRead = 1;
			START_UPLOAD_COMMAND(packetBuffer, 'G', bytesRead);
			END_UPLOAD_COMMAND(packetBuffer, currentAddress);
			packetBuffer[SIZE_OF_HEADER] = b1;
			Z80Bus::sendBytes(packetBuffer, SIZE_OF_HEADER + 1);
			currentAddress += 1;

			bytesWrittenToOutput++;
		}
	}

	// Eat remaining
	while (bytesReadFromSource < sourceLengthLimit) {
		SdCardSupport::fileRead();
		bytesReadFromSource++;
	}

	return RLE_SUCCESS;
}



int readZ80Header( uint8_t* v1_header, uint8_t* pc_low, uint8_t* pc_high, uint8_t* hw_mode) {

	for (int i = 0; i < Z80_V1_HEADERLENGTH; i++) {
		int b = SdCardSupport::fileRead();
		if (b == EOF) return -1;
		v1_header[i] = b;
	}

	// Check if V1 (PC != 0)
	if (v1_header[Z80_V1_PC_LOW] || v1_header[Z80_V1_PC_HIGH]) return Z80_VERSION_1;

	// Read extended header length
	int len_lo = SdCardSupport::fileRead();
	int len_hi = SdCardSupport::fileRead();
	if (len_lo == EOF || len_hi == EOF) return -1;

	unsigned ext_len = len_lo | (len_hi << 8);

	// Extract only needed bytes from extended header
	for (unsigned i = 0; i < ext_len; i++) {
		int b = SdCardSupport::fileRead();
		if (b == EOF) return Z80_VERSION_UNKNOWN;
		switch (i) {
			case Z80_EXT_PC_LOW: *pc_low = b; break;
			case Z80_EXT_PC_HIGH: *pc_high = b; break;
			case Z80_EXT_HW_MODE: *hw_mode = b; break;
		}
	}

	return (ext_len == Z80_V2_HEADERLENGTH) ? Z80_VERSION_2 : Z80_VERSION_3;
}

// Renamed and refactored to read block data and write directly to the SNA file.
// No longer allocates a buffer for the entire block.
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

	oled.print("p:");
	oled.print(*page_number_out);


	long sna_offset = -1;
	if (*page_number_out == 8) sna_offset = 0x4000;           // Page 8 maps to 0x4000-0x7FFF (first 16K block after header)
	else if (*page_number_out == 4) sna_offset = 0 + 0x8000;  // Page 4 maps to 0x8000-0xBFFF
	else if (*page_number_out == 5) sna_offset = 0 + 0xC000;  // Page 5 maps to 0xC000-0xFFFF
	                                                          // Add more cases for 128K pages (10, 11, etc.) if needed in the future

	oled.print("o:");
	oled.print(sna_offset);

	if (sna_offset == -1) {
		for (unsigned int i = 0; i < compressed_length; ++i) {
			SdCardSupport::fileRead();  // Consume the bytes
		}
		return BLOCK_ERROR_UNSUPPORTED_PAGE; 
	}

	unsigned bytesWrittenActual = 0;
	// Call the fixed-length RLE decoder to read from Z80 stream and write to SNA stream
	RLEDecodeResult result = decodeRLE_core(BLOCK_SIZE, compressed_length, bytesWrittenActual,sna_offset);

	*uncompressed_length_out = bytesWrittenActual;  // Update the output parameter
	if (result != RLE_SUCCESS) { return BLOCK_ERROR_READ_DATA; }
	if (*uncompressed_length_out > BLOCK_SIZE) { return BLOCK_ERROR_READ_DATA; }
	return BLOCK_SUCCESS;
}




int convertZ80toSNA_impl(const char* fileNameIn) {

//	if (!SdCardSupport::openFileByName(fileNameIn)) {
//		oled.println("Failed to open in file!");
//	}

	uint8_t* z80Header_v1 = &packetBuffer[0];
	uint8_t ext_pc_low = 0, ext_pc_high = 0, ext_hw_mode = 0;
	int z80_version = readZ80Header(  z80Header_v1, &ext_pc_low, &ext_pc_high, &ext_hw_mode);
	uint8_t* snaHeader = &head27_Execute[0 + 1];  // +1 for "E" command
	uint16_t stackOffsetForPushingPC = 0;

	if (z80_version >= 2) {
		// Patch PC and R from extended header
		z80Header_v1[Z80_V1_PC_LOW] = ext_pc_low;
		z80Header_v1[Z80_V1_PC_HIGH] = ext_pc_high;
		z80Header_v1[Z80_V1_R_7BITS] &= ~0x80;

		stackOffsetForPushingPC = Z802SNA::convertHeaders(z80Header_v1, snaHeader);

		while (true) {
			int page_number;
			unsigned uncompressed_length = 0;
			BlockReadResult block_result = z80_readAndWriteBlock(&page_number, &uncompressed_length);
			oled.print("r:");
			oled.println(block_result);

			if (block_result == BLOCK_END_OF_FILE) break;
			if (block_result != BLOCK_SUCCESS) return -1;
		}
	} else {  // version 1
		const unsigned DEST_BUFFER_SIZE = 0xC000;
		bool isCompressedV1 = (z80Header_v1[Z80_V1_FLAGS1] & 0x20) >> 5;
		stackOffsetForPushingPC = Z802SNA::convertHeaders(z80Header_v1, snaHeader);
		unsigned bytesWrittenToOutput = 0;
		if (isCompressedV1) {
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
			RLEDecodeResult result = decodeRLE_core(DEST_BUFFER_SIZE, rle_data_length, bytesWrittenToOutput, 0x4000);
			if (result != RLE_SUCCESS || bytesWrittenToOutput != DEST_BUFFER_SIZE) {
				return -1;
			}
			for (int i = 0; i < 4; ++i) {
				if (SdCardSupport::fileRead() == EOF) return -1;
			}
		} else {
			RLEDecodeResult result = decodeRLE_core(DEST_BUFFER_SIZE, DEST_BUFFER_SIZE, bytesWrittenToOutput, 0x4000);
			if (result != RLE_SUCCESS || bytesWrittenToOutput != DEST_BUFFER_SIZE) {
				return -1;
			}
		}
	}

	uint8_t bytesRead = 2;
	START_UPLOAD_COMMAND(packetBuffer, 'G', bytesRead);
	END_UPLOAD_COMMAND(packetBuffer, stackOffsetForPushingPC);

	packetBuffer[SIZE_OF_HEADER] = z80Header_v1[Z80_V1_PC_LOW];
	packetBuffer[SIZE_OF_HEADER + 1] = z80Header_v1[Z80_V1_PC_HIGH];
	Z80Bus::sendBytes(packetBuffer, SIZE_OF_HEADER + 2);

	oled.print("PC:");
	oled.print(z80Header_v1[Z80_V1_PC_LOW], HEX);
	oled.print(";");
	oled.print(z80Header_v1[Z80_V1_PC_HIGH], HEX);

	oled.setCursor(0, 0);
	oled.print(stackOffsetForPushingPC, HEX);

	SdCardSupport::fileClose();

	return 0;
}



// The outer wrapper function for resource setup and teardown
int convertZ80toSNA(const char* fileNameIn) {  // const std::string& fileName) {

	const uint16_t address = 0x4004;  // Temporary Z80 jump target in screen memory.
	packetBuffer[0] = 'S';            // 'S' command: Set Stack Pointer (SP) on Z80.
	packetBuffer[1] = (uint8_t)(address >> 8);
	packetBuffer[2] = (uint8_t)(address & 0xFF);
	Z80Bus::sendBytes(packetBuffer, 3);  // 3 = character command + 16bit address
	Z80Bus::waitRelease_NMI();           //Synchronize: Z80 knows it must halt after loading SP - Aruindo waits for NMI release.


	switch (checkZ80FileValidity(fileNameIn)) {
		case Z80_CHECK_SUCCESS: break;  //  oled.println("Z80 FILE OK"); break;
		case Z80_CHECK_ERROR_OPEN_FILE: return 1;
		case Z80_CHECK_ERROR_READ_HEADER: return 2;
		case Z80_CHECK_ERROR_V1_MARKER_NOT_FOUND: return 3;
		case Z80_CHECK_ERROR_BLOCK_STRUCTURE: return 4;
		case Z80_CHECK_ERROR_UNEXPECTED_EOF: return 5;
		case Z80_CHECK_ERROR_UNSUPPORTED_TYPE: return 6;
		default: return 7;
	}

	int result = convertZ80toSNA_impl(fileNameIn);

	bootFromSnapshot_z80_end();

	return result;
}
