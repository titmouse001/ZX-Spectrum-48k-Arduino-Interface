#include <stdint.h>
#include "SdCardSupport.h"  
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
    if (file.isFile()) {
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
  uint16_t totalFiles = 0;
  root.rewind();
  while (file.openNext(&root, O_RDONLY)) {
    if (file.isFile()) {
      totalFiles++;
    }
    file.close();
  }
  return totalFiles;
}

boolean SdCardSupport::init(uint8_t csPin);
int SdCardSupport::fileAvailable() {
  return file.available();
}
int SdCardSupport::fileRead() {
  return file.read();
}
void SdCardSupport::fileRead(uint8_t* b) {
  file.read(b, 1);
}
int SdCardSupport::fileRead(uint8_t* buf, uint16_t count) {
  return file.read((void*)buf, (size_t)count);
}
int SdCardSupport::filePosition() {
  return file.curPosition();
}
bool SdCardSupport::fileSeek(uint32_t pos) {
  return file.seekSet(pos);
}
uint16_t SdCardSupport::fileSize() {
  return file.fileSize();
}
bool SdCardSupport::fileClose() {
  return file.close();
}


// bool openFileByName(const char* filename) {
//   if (file.isOpen()) {
//     file.close();
//   }
//   // return file.open(&root, filename, O_READ);
//   return file.open(filename, O_READ);
// }


