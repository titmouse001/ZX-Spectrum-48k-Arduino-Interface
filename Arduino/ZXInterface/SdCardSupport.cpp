#include <stdint.h>
#include "SdCardSupport.h"  
#include "BufferManager.h" 
#include "SdFat.h" 

SdFat32 SdCardSupport::sd;
FatFile SdCardSupport::root;
FatFile SdCardSupport::file;

boolean SdCardSupport::init(uint8_t csPin) {
  if (root.isOpen()) {
    root.close();
    sd.end();
  }
  if (!sd.begin(csPin)) {
    return false;
  }
  if (!root.open("/")) {
    sd.end();
    return false;
  }
  return true;
}

__attribute__((optimize("-Ofast"))) 
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

char* SdCardSupport::getFileName(FatFile* pFile) {
  char* fileName = (char*)&BufferManager::packetBuffer[FILE_READ_BUFFER_OFFSET];
  pFile->getName7(fileName, 64);
  return fileName;
}

char* SdCardSupport::getFileNameWithSlash(FatFile* pFile,char* buffer) {
  pFile->getName7(&buffer[1], 64);
  buffer[0]='/';
  return buffer;
}
