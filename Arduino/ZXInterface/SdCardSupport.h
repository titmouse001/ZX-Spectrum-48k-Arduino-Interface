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

static SdFat32& getSd() { return sd; }
static FatFile& getRoot() { return root; }
static FatFile& getFile() { return file; }
static FatFile& closeFile();
static FatFile& reopenRoot() ;

static bool isInserted();
static bool copyFile(FatFile& root, FatFile& dir, const char* fromFileName,const char* toFileName);
static bool findFreeFilename(FatFile& dir,  char* filename);

static uint16_t countSnapshotFiles();
static FatFile*  openFileByIndex(uint16_t searchIndex);

};

#endif

