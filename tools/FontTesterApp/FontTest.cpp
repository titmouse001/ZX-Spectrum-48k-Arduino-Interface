#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"

#include "font.h"

/* debug
#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c\n"
#define BYTE_TO_BINARY(byte)  \
  ((byte) & 0x80 ? '#' : '.'), \
  ((byte) & 0x40 ? '#' : '.'), \
  ((byte) & 0x20 ? '#' : '.'), \
  ((byte) & 0x10 ? '#' : '.'), \
  ((byte) & 0x08 ? '#' : '.'), \
  ((byte) & 0x04 ? '#' : '.'), \
  ((byte) & 0x02 ? '#' : '.'), \
  ((byte) & 0x01 ? '#' : '.') 
*/

static const int FONT_WIDTH = 5;
static const int FONT_HEIGHT = 7;
static const int FONT_GAP = 1;
static const int BUFFER_SIZE = 32;

byte finalOutput[BUFFER_SIZE * FONT_HEIGHT] = { 0 };
int bitPosition[FONT_HEIGHT] = { 0 };

class Example : public olc::PixelGameEngine
{

public:
	Example() {
		sAppName = "ZX Spectrum Font Tester";
	}

public:
	bool OnUserCreate() override {
		return true;
	}

	
	bool OnUserUpdate(float fElapsedTime) override {
		
		memset(finalOutput, 0, static_cast<size_t>(32L) * FONT_HEIGHT);

		char tx[] = "ABCDEFGHIqazwsx";
		if (strlen(tx) > 32) {
			strncpy_s(tx, tx, 32);
		}

		for (int i = 0; i < FONT_HEIGHT; i++) {
			bitPosition[i] = (BUFFER_SIZE * i) * 8;
		}

		for (int i = 0; tx[i] != '\0'; i++) {
			uint8_t* ptr = &fudged_Adafruit5x7[(tx[i] - 0x20) * FONT_WIDTH];
			for (int row = 0; row < FONT_HEIGHT; row++) {
				uint8_t transposedRow = 0;
				for (int col = 0; col < FONT_WIDTH; col++) {
					transposedRow |= ((ptr[col] >> row) & 0x01) << (FONT_WIDTH - 1 - col);
				}
				joinBits(transposedRow, FONT_WIDTH + FONT_GAP, row);  // Merge transposeMatrix into main loop
			}
		}

		int xend = bitPosition[0];
		for (int row = 0; row < FONT_HEIGHT; row++) {
			uint8_t* ptr = &finalOutput[row * BUFFER_SIZE];
			for (int x = 0; x < xend; x++) {
				if (ptr[x / 8] & (0x80 >> (x & 7)) ) {
					Draw(x, row, olc::Pixel(200, 200,200));
					if (row > 4)
						Draw(x, row, olc::Pixel(0, 200, 0));
					else
						Draw(x, row, olc::Pixel(255, 255, 255));
				}		
			}
		}

		return true;

	}

	void joinBits(uint8_t input, byte bitWidth, byte k) {
		int bitPos = bitPosition[k];     
		int byteIndex = bitPos / 8;         
		int bitIndex = bitPos % 8;          
		// Using a WORD to allow for a boundary crossing
		uint16_t alignedInput = (uint16_t)input << (8 - bitWidth);
		finalOutput[byteIndex] |= alignedInput >> bitIndex;
		if (bitIndex + bitWidth > 8) {  // spans across two bytes
			finalOutput[byteIndex + 1] |= alignedInput << (8 - bitIndex);
		}
		bitPosition[k] += bitWidth;
	}


};


int main() {
	Example demo;
	if (demo.Construct(128, 32, 4, 4)) {
		demo.Start();
	}
	return 0;
}


/* DEBUG - moved out , keeping here

for (int i = 0; i < strlen(tx); i++) {
	for (int k = 0; k < FONT_HEIGHT; k++) {
		uint8_t b = finalOutput[(32 * k) + i];
		printf(BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(b));
	}
}
printf("\n");

*/