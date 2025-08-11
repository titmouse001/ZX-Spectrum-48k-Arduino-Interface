#ifndef SDCARD_SUPPORT_H
#define SDCARD_SUPPORT_H

#include <stdint.h>
#include "SdFat.h"  // "SdFatConfig.h" options, I'm using "USE_LONG_FILE_NAMES 1"

namespace SdCardSupport {

extern SdFat32 sd;
extern FatFile root;
extern FatFile file;

boolean init(uint8_t csPin) ;
int fileAvailable() ;
int fileRead();
void fileRead(uint8_t* b) ;
int fileRead(uint8_t* buf, uint16_t count);
int filePosition() ;
bool fileSeek(uint32_t pos) ;
uint16_t fileSize();
bool fileClose();
void openFileByIndex(uint8_t searchIndex);
uint16_t countSnapshotFiles();

}

#endif
