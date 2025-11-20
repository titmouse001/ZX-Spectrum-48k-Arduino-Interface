#include <stdint.h>
#include "SdCardSupport.h"  
#include "BufferManager.h" 
#include "SdFat.h" 

SdFat32 SdCardSupport::sd;
FatFile SdCardSupport::root;
FatFile SdCardSupport::file;

 __attribute__((optimize("-Os")))
boolean SdCardSupport::init(uint8_t csPin) {
  if (root.isOpen()) {
    root.close();
    sd.end();
  }
  if (!sd.begin(csPin,SPI_QUARTER_SPEED )) {
    return false;
  }
  if (!root.open("/")) {
    sd.end();
    return false;
  }
  return true;
}

__attribute__((optimize("-Os"))) 
void SdCardSupport::openFileByIndex(uint8_t searchIndex) {
  root.rewind();
  uint8_t index = 0;
  while (file.openNext(&root, O_RDONLY)) {
    if (!file.isHidden() &&  (file.isFile() || file.isDir())) {
      if (index == searchIndex) {
        break;
      }
      index++;
    }
    file.close();
  }
}

__attribute__((optimize("-Ofast")))
uint16_t SdCardSupport::countSnapshotFiles() {

  if (file.isOpen()) {
    file.close();
  }

  uint16_t totalFiles = 0;
  root.rewind();
  while (file.openNext(&root, O_RDONLY)) {
    if (!file.isHidden() && (file.isFile() || file.isDir()) ) {
      totalFiles++;
    }
    file.close();
  }
  return totalFiles;
}

// This version of getFileName is used for the file browser view, keeping this one optimised for speed.
__attribute__((optimize("-Ofast")))
uint8_t SdCardSupport::getFileName(FatFile* pFile , char* pFileNameBuffer) {
  uint8_t len = pFile->getName7(pFileNameBuffer, MAX_FILENAME_LEN);
  if (len == 0) {
    len = pFile->getSFN(pFileNameBuffer, 13); // fallback, XXXXXXXX.XXX
  }
  return len;
}

char* SdCardSupport::getFileName(FatFile* pFile) {
  char* buffer = (char*)&BufferManager::packetBuffer[FILE_READ_BUFFER_OFFSET];
  getFileName(pFile,buffer);
  return buffer;
}

char* SdCardSupport::getFileNameWithSlash(FatFile* pFile) {
  char* buffer = (char*)&BufferManager::packetBuffer[FILE_READ_BUFFER_OFFSET];
  getFileName(pFile,&buffer[1]);
  buffer[0]='/';
  return buffer;
}
