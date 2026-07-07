#include <stdint.h>
#include "SdCardSupport.h"
#include "BufferManager.h"
#include "menu.h"
#include "Pin.h"
#include "Utils.h"
#include "src/fatlib/SdFat.h"

SdFat32 SdCardSupport::sd;
FatFile SdCardSupport::root;
FatFile SdCardSupport::file;

bool SdCardSupport::init() { //uint8_t csPin) {
  // NOTE: For SD cards
  // SPI_HALF_SPEED looks best for NANO (tried 'SPI_DIV6_SPEED' gave me same read times)
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

uint16_t SdCardSupport::countSnapshotFiles() {
  closeFileIfOpen();
  root.rewind();
  DirFat_t dir;
  uint16_t totalFiles = 0;
  while (root.readDir(&dir) > 0) {
    uint8_t firstChar = dir.name[0];   
    if (firstChar == FAT_NAME_FREE) break;  // nothing left
    if (firstChar == '.' || firstChar == FAT_NAME_DELETED) continue; 
    // As was are peeking at internal name[11], we must filter to skip Hidden, System, Label, and Long File Names
    if (dir.attributes & (FAT_ATTRIB_LONG_NAME | FS_ATTRIB_HIDDEN | FS_ATTRIB_SYSTEM | FAT_ATTRIB_LABEL)) {
      continue;
    }
    totalFiles++;
  }
  return totalFiles;
}

char* SdCardSupport::getDisplayFileName( char* pFileNameBuffer) {
  file.getName7(pFileNameBuffer, ZX_FILENAME_MAX_DISPLAY_LEN);
  return pFileNameBuffer;
}

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