#ifndef SDCARD_SUPPORT_H
#define SDCARD_SUPPORT_H

#include <stdint.h>
#include "SdFat.h" 
// "SdFatConfig.h" options, I'm using "USE_LONG_FILE_NAMES 1"
// Leaving SdFatConfig.h as default will do this already, see note at EOF to free up some flash memory.

class SdCardSupport {

public:
static boolean init(uint8_t csPin) ;
static uint16_t countSnapshotFiles();
static void openFileByIndex(uint8_t searchIndex);

static uint8_t getFileName(FatFile* pFile, char* pFileNameBuffer);
static char* getFileName(FatFile* pFile);
static char* getFileNameWithSlash(FatFile* pFile);

static FatFile& getRoot() { return root; }
static FatFile& getFile() { return file; }

private:
static SdFat32 sd;
static FatFile root;
static FatFile file;

};

#endif

// NOTE: 1738 bytes can be saved if needed
// see "SdFatConfig.h" for minimum flash size use these settings:
//#define USE_FAT_FILE_FLAG_CONTIGUOUS 0
//#define ENABLE_DEDICATED_SPI 0
//#define USE_LONG_FILE_NAMES 0
//#define SDFAT_FILE_TYPE 1
//#define CHECK_FLASH_PROGRAMMING 0  // May cause SD to sleep at high current.
