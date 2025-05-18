#ifndef SDSUPPORT_H
#define SDSUPPORT_H

#include "SdFat.h"  // "SdFatConfig.h" options, I'm using "USE_LONG_FILE_NAMES 1"

extern SdFat32 sd;
extern FatFile root;
extern FatFile file;

namespace Sd {

enum struct Status : uint8_t {
  OK = 0,
  NO_FILES = 1,
  NO_SD_CARD = 2
};

__attribute__((optimize("-Ofast"))) void openFileByIndex(uint8_t searchIndex) {
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
uint16_t
countSnapshotFiles() {
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

// Attempt to initialize SD and count files.
Status init(uint16_t& fileCount) {
 
  fileCount=0;
//  if (fileCount > 0) {  
    // already initialized, clean up
    if (root.isOpen()) {
      root.close();
      sd.end();
    }
//    fileCount = 0;
//  }
  if (!sd.begin()) {
    return Status::NO_SD_CARD;
  }
  if (!root.open("/")) {
    sd.end();
    return Status::NO_SD_CARD;
  }
  fileCount = countSnapshotFiles();  // implement this yourself
  if (fileCount > 0) {
    return Status::OK;
  } else {
    root.close();
    sd.end();
    return Status::NO_FILES;
  }
}

}

#endif
