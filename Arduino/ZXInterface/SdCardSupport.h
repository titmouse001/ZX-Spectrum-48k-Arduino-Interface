#ifndef SDCARD_SUPPORT_H
#define SDCARD_SUPPORT_H

#include <stdint.h>

#include "src/fatlib/SdFat.h"


// "SdFatConfig.h" options, I'm using "USE_LONG_FILE_NAMES 1"
// Leaving SdFatConfig.h as default will do this already, see note at EOF to free up some flash memory.

class SdCardSupport {

private:
static SdFat32 sd;
static FatFile root;
static FatFile file;

public:

static uint16_t menuPathHistory[]; 
static uint8_t  menuPathDepth;

static bool init(); //uint8_t csPin) ;

static void syncRootToDepth();

static FatFile& getRoot() { return root; }
static FatFile& getFile() { return file; }
static FatFile& closeFile();
static FatFile& reopenRoot() ;

// static FatFile& closeFileIfOpen();
// static FatFile& closeRootIfOpen();
static bool isInserted();
static bool copyFile(FatFile& root, FatFile& dir, const char* fromFileName,const char* toFileName);
static bool findFreeFilename(FatFile& dir,  char* filename);

static uint16_t countSnapshotFiles();
static FatFile*  openFileByIndex(uint16_t searchIndex);
//static char* getDisplayFileName(char* buff);

};

#endif

// NOTE: 1738 bytes can be saved if needed
// see "SdFatConfig.h" for minimum flash size use these settings:
//#define USE_FAT_FILE_FLAG_CONTIGUOUS 0
//#define ENABLE_DEDICATED_SPI 0
//#define USE_LONG_FILE_NAMES 0
//#define SDFAT_FILE_TYPE 1
//#define CHECK_FLASH_PROGRAMMING 0  // May cause SD to sleep at high current.
