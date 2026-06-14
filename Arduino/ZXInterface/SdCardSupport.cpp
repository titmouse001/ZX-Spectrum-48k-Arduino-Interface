#include <stdint.h>
#include "SdCardSupport.h"
#include "BufferManager.h"
#include "menu.h"
#include "Pin.h"

#include "SdFat.h"

SdFat32 SdCardSupport::sd;
FatFile SdCardSupport::root;
FatFile SdCardSupport::file;
static char fileNameBuf[ZX_FILENAME_MAX_DISPLAY_LEN + 1];  // +1 null

// NOTE: For SD cards
// SPI_HALF_SPEED looks best for NANO (tried 'SPI_DIV6_SPEED' gave me same read times)
__attribute__((optimize("-Os")))
bool SdCardSupport::init() { //uint8_t csPin) {
  closeRootIfOpen();
  sd.end();
  if (!sd.begin(Pin::SD_CARD_CS, SPI_HALF_SPEED)) return false;
  return root.open("/");
}

__attribute__((optimize("-Os"))) 
void SdCardSupport::openFileByIndex(uint8_t searchIndex) {
  root.rewind();
  uint8_t index = 0;
  while (closeFileIfOpen().openNext(&root, O_RDONLY)) {
    //if (!file.isHidden() &&  (file.isFile() || file.isDir())) {
    if (!file.isHidden()) {
      if (index++ == searchIndex) return;
    }
  }
}

__attribute__((optimize("-Os"))) // Changed from Ofast to Os to save space
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
    if (attr & (FAT_ATTRIB_LONG_NAME | FS_ATTRIB_HIDDEN | FS_ATTRIB_SYSTEM | FAT_ATTRIB_LABEL)) {
        if ((attr & 0x3F) != FAT_ATTRIB_LONG_NAME) continue; 
        // Logic check: if it's LFN, we skip - count just "8.3" filenames
        continue;
    }
    totalFiles++;
  }
  return totalFiles;
}

__attribute__((optimize("-Os"), noinline))
uint8_t SdCardSupport::getFileName(FatFile* pFile , char* pFileNameBuffer) {
  uint8_t len = pFile->getName7(pFileNameBuffer, MAX_FILENAME_LEN);
  if (len == 0) {
    len = pFile->getSFN(pFileNameBuffer, 13); // 8.3 fallback e.g. "filename.sna"
  }
  return len;
}

__attribute__((optimize("-Os"), noinline))
char* SdCardSupport::getFileName(FatFile* pFile) {
  getFileName(pFile,fileNameBuf);
  return fileNameBuf;
}

__attribute__((optimize("-Os")))
char* SdCardSupport::getFileNameWithSlash(FatFile* pFile) {
  getFileName(pFile,&fileNameBuf[1]);
  fileNameBuf[0]='/';
  return fileNameBuf;
}

__attribute__((optimize("-Os"), noinline))
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


__attribute__((optimize("-Os")))
bool SdCardSupport::isInserted() {
  cid_t cid; // 16-byte struct to hold the response
  
  // sd.card() gets the underlying hardware driver. 
  // readCID() asks the card to identify itself.
  if (!sd.card() || !sd.card()->readCID(&cid)) {
    return false; // Card is gone or SPI communication failed!
  }
  return true; // Card is alive and well
}

