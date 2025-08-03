#ifndef SDCARD_H
#define SDCARD_H

#include "SdFat.h"  // "SdFatConfig.h" options, I'm using "USE_LONG_FILE_NAMES 1"

namespace SdCardSupport {
static SdFat32 sd;
static FatFile root;
static FatFile file;
//static FatFile fileOut;  // runnig out of mem forthis test !!!

static constexpr uint16_t SNAPSHOT_FILE_SIZE = (1024UL * 48) + 27;  //49179 (.sna files)
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

int fileAvailable() { return file.available(); }
int fileRead() {  return file.read(); }
void fileRead(uint8_t* b) { file.read(b, 1);  }
int fileRead(uint8_t* buf, uint16_t count) {  return file.read((void*)buf, (size_t)count); }

//int fileRead(uint8_t* buf, uint16_t offset, uint16_t count) {  return file.read((void*)(buf + offset), (size_t)count); }

int filePosition() {  return file.curPosition(); }
bool fileSeek(uint32_t pos) {  return file.seekSet(pos); }

__attribute__((optimize("-Ofast")))
void openFileByIndex(uint8_t searchIndex) {
  root.rewind();
  uint8_t index = 0;
  while (file.openNext(&root, O_RDONLY)) {
    if (file.isFile()) {
      //  if ((file.fileSize() == SNAPSHOT_FILE_SIZE) || (file.fileSize() == 6912)) {
      if (index == searchIndex) {
        break;
      }
      index++;
      //    }
    }
    file.close();
  }
}

bool openFileByName(const char* filename) {
  if (file.isOpen()) {
    file.close();
  }
  // return file.open(&root, filename, O_READ);
  return file.open(filename, O_READ);
}

uint16_t fileSize() {  return file.fileSize(); }
bool fileClose() {  return file.close(); }


__attribute__((optimize("-Ofast")))
uint16_t countSnapshotFiles() {
  uint16_t totalFiles = 0;
  root.rewind();
  while (file.openNext(&root, O_RDONLY)) {
    if (file.isFile()) {
      //   if (file.fileSize() == 49179 || (file.fileSize() == 6912)  ) {
      totalFiles++;
      //   }
    }
    file.close();
  }
  return totalFiles;
}


// // fileOut works, sna load in emulator... using root to convert over to pump into speccy ram... not needed from now on, removing soon.
// bool openFileByNameForWrite(const char* filename) {
// //   if (fileOut.isOpen()) {
// //  fileOut.close();
// //   }
//  // return fileOut.open(&root, filename, O_WRITE | O_CREAT );
//    return root.open( filename, O_WRITE | O_CREAT );
// }

// bool fileCloseForWrite(){
//    return root.close();
// }

// bool fileWriteSeek(uint32_t pos) {
// 		return  root.seekSet(pos);
// }

// size_t fileWrite(uint8_t data) {
// 		return root.write(data);
// }

// size_t fileWrite(const uint8_t* buffer, size_t len) {
// 		return root.write(buffer, len);
// }
// int fileWritePosition() {
//  	return root.curPosition();
// }


}

#endif
