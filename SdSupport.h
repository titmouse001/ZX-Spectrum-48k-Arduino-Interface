#ifndef SDSUPPORT_H
#define SDSUPPORT_H

#include "SdFat.h"  // "SdFatConfig.h" options, I'm using "USE_LONG_FILE_NAMES 1"

extern SdFat32 sd;
extern FatFile root;
extern FatFile file;

namespace Sd {

__attribute__((optimize("-Ofast"))) 
void openFileByIndex(uint8_t searchIndex) {
  root.rewind();
  uint8_t index = 0;
  while (file.openNext(&root, O_RDONLY)) {
    if (file.isFile()) {
      if (file.fileSize() == 49179) {
        if (index == searchIndex) {
          break;
        }
        index++;
      }
    }
    file.close();
  }
}

__attribute__((optimize("-Ofast")))
uint16_t countSnapshotFiles() {
  uint16_t totalFiles = 0;
  root.rewind();
  while (file.openNext(&root, O_RDONLY)) {
    if (file.isFile()) {
      if (file.fileSize() == 49179) {
        totalFiles++;
      }
    }
    file.close();
  }
  return totalFiles;
}

uint8_t Initialize(uint16_t* totalFiles) {
  if (*totalFiles > 0) {
    root.close();
    sd.end();
    *totalFiles = 0;
  }
  if (!sd.begin()) {}
  if (root.open("/")) {
    *totalFiles = countSnapshotFiles();
    if (*totalFiles > 0) {
      return 0;
    } else {
      return 1; // no files found
    }
  } else {
    root.close();
    sd.end();
    return 2;  // no sd card found
  }
}

}

#endif
