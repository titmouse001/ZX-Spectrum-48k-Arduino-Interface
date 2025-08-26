#ifndef SDCARD_SUPPORT_H
#define SDCARD_SUPPORT_H

#include <stdint.h>
#include "SdFat.h"  // "SdFatConfig.h" options, I'm using "USE_LONG_FILE_NAMES 1"

class SdCardSupport {

//extern SdFat32 sd;
//extern FatFile root;
//extern FatFile file;


public:
static boolean init(uint8_t csPin) ;
static uint16_t countSnapshotFiles();
static void openFileByIndex(uint8_t searchIndex);
static char* getFileName(FatFile* pFile);
static char* getFileNameWithSlash(FatFile* pFile,char* buffer);

static FatFile& getRoot() { return root; }
static FatFile& getFile() { return file; }

private:
static SdFat32 sd;
static FatFile root;
static FatFile file;

};

#endif
