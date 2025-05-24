#ifndef SDSUPPORT_H
#define SDSUPPORT_H

#include "SdFat.h"  // "SdFatConfig.h" options, I'm using "USE_LONG_FILE_NAMES 1"

extern SdFat32 sd;
extern FatFile root;
extern FatFile file;

namespace Sd {

static constexpr uint16_t SNAPSHOT_FILE_SIZE = (1024u*48u) + 27u;  //49179

uint16_t countSnapshotFiles();

enum struct Status : uint8_t {
  OK = 0,
  NO_FILES = 1,
  NO_SD_CARD = 2
};

struct FileCountResult {
  Status status;
  uint16_t totalFiles;
};


// Attempt to initialize SD and count files.
FileCountResult init() {
  FileCountResult result;
  if (root.isOpen()) {
    root.close();
    sd.end();
  }
  if (!sd.begin()) {
    result.status = Status::NO_SD_CARD;
    return result;
  }
  if (!root.open("/")) {
    sd.end();
    result.status = Status::NO_SD_CARD;
    return result;
  }
  result.totalFiles = countSnapshotFiles(); 
  if (result.totalFiles > 0) {
    result.status = Status::OK;
  } else {
    root.close();
    sd.end();
    result.status = Status::NO_FILES;
  }
  return result;
}

__attribute__((optimize("-Ofast"))) 
void openFileByIndex(uint8_t searchIndex) {
  root.rewind();
  uint8_t index = 0;
  while (file.openNext(&root, O_RDONLY)) {
    if (file.isFile()) {
      if (file.fileSize() == SNAPSHOT_FILE_SIZE) {
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
      if (file.fileSize() == 49179) {
        totalFiles++;
      }
    }
    file.close();
  }
  return totalFiles;
}


}

#endif
