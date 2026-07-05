#include <stdint.h>
#include "SdCardSupport.h"
#include "BufferManager.h"
#include "menu.h"
#include "Pin.h"
#include "Utils.h"

#include "SdFat.h"

SdFat32 SdCardSupport::sd;
FatFile SdCardSupport::root;
FatFile SdCardSupport::file;
static char fileNameBuf[ZX_FILENAME_MAX_DISPLAY_LEN + 1 + 1];  // +1 null, +1 "/"

// NOTE: For SD cards
// SPI_HALF_SPEED looks best for NANO (tried 'SPI_DIV6_SPEED' gave me same read times)

bool SdCardSupport::init() { //uint8_t csPin) {
  closeRootIfOpen();
  sd.end();
  if (!sd.begin(Pin::SD_CARD_CS, SPI_HALF_SPEED)) return false;
  return root.open("/");
}

FatFile*  SdCardSupport::openFileByIndex(uint16_t searchIndex) {
  root.rewind();
  uint16_t index = 0;
  while (closeFileIfOpen().openNext(&root, O_RDONLY)) {
    //if (!file.isHidden() &&  (file.isFile() || file.isDir())) {
    if (!file.isHidden()) {
      if (index++ == searchIndex) { 
        return &file;
      }
    }
  }
  return nullptr;
}

 // Changed from Ofast to Os to save space
uint16_t SdCardSupport::countSnapshotFiles() {
  closeFileIfOpen();
  root.rewind();
  DirFat_t dir;

  uint16_t totalFiles = 0;
  while (root.readDir(&dir) > 0) {
    uint8_t firstChar = dir.name[0];
    if (firstChar == FAT_NAME_FREE) break;
    if (firstChar == '.' || firstChar == FAT_NAME_DELETED) continue;

    uint8_t attr = dir.attributes;
    // if (attr & (FAT_ATTRIB_LONG_NAME | FS_ATTRIB_HIDDEN | FS_ATTRIB_SYSTEM | FAT_ATTRIB_LABEL)) {
    //     if ((attr & 0x3F) != FAT_ATTRIB_LONG_NAME) continue; 
    //     // Logic check: if it's LFN, we skip - count just "8.3" filenames
    //     continue;
    // }

    // Skip Hidden, System, Label, and Long File Names
    if (attr & (FAT_ATTRIB_LONG_NAME | FS_ATTRIB_HIDDEN | FS_ATTRIB_SYSTEM | FAT_ATTRIB_LABEL)) {
        continue;
    }
    totalFiles++;
  }
  return totalFiles;
}

char* SdCardSupport::getDisplayFileName(FatFile* pFile) {
  getDisplayFileName(pFile,fileNameBuf);
  return fileNameBuf;
}

uint8_t SdCardSupport::getDisplayFileName(FatFile* pFile , char* pFileNameBuffer) {
  return pFile->getName7(pFileNameBuffer, ZX_FILENAME_MAX_DISPLAY_LEN);
}

// uint8_t SdCardSupport::getFileName(FatFile* pFile , char* pFileNameBuffer) {
//   uint8_t len = pFile->getName7(pFileNameBuffer, MAX_FILENAME_LEN);
//   // looking at fat lib source code, it does give an internal fallback to getSFN! 
//   // if (len == 0) {
//   //   len = pFile->getSFN(pFileNameBuffer, 13); // 8.3 fallback e.g. "filename.sna"
//   // }
//   return len;
// }

char* SdCardSupport::getFileName(FatFile* pFile) {
  uint8_t len = pFile->getName7(fileNameBuf, MAX_FILENAME_LEN);  // does internal fallback to getSFN! 
  //getFileName(pFile,fileNameBuf);
  return fileNameBuf;
}


// char* SdCardSupport::getFileNameWithSlash(FatFile* pFile) {
//   getFileName(pFile,&fileNameBuf[1]);
//   fileNameBuf[0]='/';
//   return fileNameBuf;
// }

FatFile& SdCardSupport::closeFileIfOpen() { 
  if (file.isOpen()) {
    file.close(); 
  }
  return file;
}

__attribute__((optimize("-Os"), noinline)) 
FatFile& SdCardSupport::closeRootIfOpen() { 
  if (root.isOpen()) {
    root.close(); 
  }
  return root;
}



bool SdCardSupport::isInserted() {
  cid_t cid; // 16-byte struct to hold the response
  
  // sd.card() gets the underlying hardware driver. 
  // readCID() asks the card to identify itself.
  if (!sd.card() || !sd.card()->readCID(&cid)) {
    return false; // Card is gone or SPI communication failed!
  }
  return true; // Card is alive and well
}


bool SdCardSupport::findFreeFilename(FatFile& dir, char* fileName) {
  FatFile file;
  // happy to let "SHOT9999" roll over to "SHOU0000"
  for (uint16_t i = 0; i < 9999; i++) {
    // Ripple-carry "0000" -> "9999"
    uint8_t idx = 7;
    while (++fileName[idx] > '9') {
      fileName[idx--] = '0';
    }

    if (!file.open(&dir, fileName, O_READ)) {
      return true;  // found one
    }
    file.close();
  }
  
  return false; // All 9999 slots full
}


bool SdCardSupport::copyFile(FatFile& root, FatFile& dir, const char* fromFileName,const char* toFileName) {
  uint16_t mark = BufferManager::getMark();
  uint8_t* buf = BufferManager::allocate(FILE_READ_BUFFER_SIZE);
  bool success = false;
  FatFile srcFile;

  if (srcFile.open(&root, fromFileName, O_READ)) {
    FatFile destFile;
    if (destFile.open(&dir, toFileName, O_CREAT | O_WRONLY)) {
      int bytesRead;
      while ((bytesRead = srcFile.read(buf, FILE_READ_BUFFER_SIZE)) > 0) {
        destFile.write(buf, bytesRead);
      }
      destFile.close();
      success = true;
    }
    srcFile.close();
  }

  BufferManager::freeToMark(mark);
  return success;
}