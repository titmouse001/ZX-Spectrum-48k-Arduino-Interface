#ifndef SDCARD_H
#define SDCARD_H

#include "SdFat.h"  // "SdFatConfig.h" options, I'm using "USE_LONG_FILE_NAMES 1"

SdFat32 sd;
FatFile root;
FatFile file;

namespace SdCardSupport {

static constexpr uint16_t SNAPSHOT_FILE_SIZE = (1024*48)+27;  //49179
uint16_t countSnapshotFiles();

enum struct Status : uint8_t {
  OK = 0,
  NO_FILES = 1,
  NO_SD_CARD = 2
};

boolean init() {
  if (root.isOpen()) {
    root.close();
    sd.end();
  }
  if (!sd.begin()) {
    return false;
  }
  if (!root.open("/")) {
    sd.end();
    return false;
  }
  return true;
}

__attribute__((optimize("-Ofast"))) 
void openFileByIndex(uint8_t searchIndex) {
  root.rewind();
  uint8_t index = 0;
  while (file.openNext(&root, O_RDONLY)) {
    if (file.isFile()) {
      if ((file.fileSize() == SNAPSHOT_FILE_SIZE) || (file.fileSize() == 6912)) {
        if (index == searchIndex) {
          break;
        }
        index++;
      }
    }
    file.close();
  }
}

// ---------------------------------
// Supporting Section - internal use
// ---------------------------------

__attribute__((optimize("-Ofast")))
uint16_t countSnapshotFiles() {
  uint16_t totalFiles = 0;
  root.rewind();
  while (file.openNext(&root, O_RDONLY)) {
    if (file.isFile()) {
      if (file.fileSize() == 49179 || (file.fileSize() == 6912)  ) {
        totalFiles++;
      }
    }
    file.close();
  }
  return totalFiles;
}


}

#endif
