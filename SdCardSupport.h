#ifndef SDCARD_H
#define SDCARD_H

#include <Arduino.h>
#include "SdFat.h"  // "SdFatConfig.h" options, I'm using "USE_LONG_FILE_NAMES 1"

namespace SdCardSupport {
  
static SdFat32 sd;
static FatFile root;
static FatFile file;

boolean init(uint8_t csPin) {
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

int fileAvailable() { return file.available(); }
int fileRead() {  return file.read(); }
void fileRead(uint8_t* b) { file.read(b, 1);  }
int fileRead(uint8_t* buf, uint16_t count) {  return file.read((void*)buf, (size_t)count); }
int filePosition() {  return file.curPosition(); }
bool fileSeek(uint32_t pos) {  return file.seekSet(pos); }
uint16_t fileSize() {  return file.fileSize(); }
bool fileClose() {  return file.close(); }

__attribute__((optimize("-Ofast")))
void openFileByIndex(uint8_t searchIndex) {
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


// bool openFileByName(const char* filename) {
//   if (file.isOpen()) {
//     file.close();
//   }
//   // return file.open(&root, filename, O_READ);
//   return file.open(filename, O_READ);
// }

__attribute__((optimize("-Ofast")))
uint16_t countSnapshotFiles() {
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

}

#endif
