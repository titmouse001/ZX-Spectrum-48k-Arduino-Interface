#include <stdint.h>
#include "SdCardSupport.h"
#include "BufferManager.h"
#include "menu.h"
#include "Pin.h"
#include "Utils.h"
//#include "src/fatlib/SdFat.h" // SdFat version 2.3.1 

SdFat32 SdCardSupport::sd;
FatFile SdCardSupport::root;
FatFile SdCardSupport::file;

uint16_t SdCardSupport::menuPathHistory[FOLDER_NAV_DEPTH]; 
uint8_t  SdCardSupport::menuPathDepth = 0;


bool SdCardSupport::init() { //uint8_t csPin) {
  sd.end();
  // NOTE: SPI_HALF_SPEED looks best for NANO (tried 'SPI_DIV6_SPEED' gave me same read times)
  if (!sd.begin(Pin::SD_CARD_CS, SPI_HALF_SPEED)) return false;
  root.close();
  return root.openRoot(sd.vol());
}




void SdCardSupport::syncRootToDepth() {
  reopenRoot();

  if (menuPathDepth > 0) {
    size_t totalPathLen = 1;  // Start with root '/'
    for (uint8_t i = 0; i < menuPathDepth; i++) {
      FatFile* file = SdCardSupport::openFileByIndex(menuPathHistory[i]);
      if (file) {
        totalPathLen += (file->getNameLength() + 1);  //  + 1 for the slash
      }
    }
    totalPathLen += 1;  // +1 for null terminator

    uint16_t mark = BufferManager::getMark();
    char* localPath = (char*)BufferManager::allocate(totalPathLen);

    localPath[0] = '/';
    localPath[1] = '\0';

    for (uint8_t i = 0; i < menuPathDepth; i++) {
      FatFile* file = SdCardSupport::openFileByIndex(menuPathHistory[i]);
      if (file) {
        size_t len = strlen(localPath);
        localPath[len] = '/';
        file->getName(localPath + len + 1, totalPathLen - len - 1);
      }

      root.close();
      if (!root.open(localPath)) {
        menuPathDepth = 0;
        reopenRoot();
        break;
      }
    }
    BufferManager::freeToMark(mark);
  }
  Menu::inSubFolder = (menuPathDepth > 0);
}


FatFile* SdCardSupport::openFileByIndex(uint16_t searchIndex) {
  uint16_t index = 0;
  root.rewind();
  while (true) {
    file.close(); // Close the previous file before opening the next
    if (!file.openNext(&root, O_RDONLY)) {
      return nullptr; // Reached the end of the directory without finding the index
    }
    if (!file.isHidden()) {
      if (index == searchIndex) {
        return &file; // Found the exact match!
      }
      index++; // Only increment our counter if the file is actually visible
    }
  }
}

uint16_t SdCardSupport::countSnapshotFiles() {
  file.close();
  root.rewind();
  uint16_t totalFiles = 0;
  while (file.openNext(&root, O_RDONLY)) {
    if (!file.isHidden()) {
      totalFiles++;
    }
    file.close();
  }
  return totalFiles;
}

FatFile& SdCardSupport::closeFile() { 
  file.close(); 
  return file;
}

FatFile& SdCardSupport::reopenRoot() { 
  root.close(); 
  root.openRoot(sd.vol());
  return root;
}

bool SdCardSupport::isInserted() {
  cid_t cid; // 16-byte struct to hold the response
  // underlying hardware driver / card to identify itself.
  if (!sd.card() || !sd.card()->readCID(&cid)) {
    return false; // Card is gone or SPI communication failed!
  }
  return true; // Card is alive and well
}

bool SdCardSupport::findFreeFilename(FatFile& dir, char* fileName) {
  fileName[4] = '0';
  fileName[5] = '0';
  fileName[6] = '0';
  fileName[7] = '0';
  for (uint16_t i = 0; i < 9999; i++) {
    uint8_t idx = 7;
    while (++fileName[idx] > '9') {
      fileName[idx--] = '0';
    }
    if (!dir.exists(fileName)) {
      return true;  // found an empty slot!
    }
  }
  return false; // All 9999 slots full
}

bool SdCardSupport::copyFile(FatFile& root, FatFile& dir, const char* fromFileName,const char* toFileName) {
  uint16_t mark = BufferManager::getMark();
  uint8_t* buf = BufferManager::allocate(FILE_READ_BUFFER_SIZE);
  bool success = false;

  file.close();
  if (file.open(&root, fromFileName, O_READ)) {
    FatFile destFile;
    if (destFile.open(&dir, toFileName, O_CREAT | O_WRONLY)) {
      int bytesRead;
      while ((bytesRead = file.read(buf, FILE_READ_BUFFER_SIZE)) > 0) {
        destFile.write(buf, bytesRead);
      }
      destFile.close();
      success = true;
    }
    file.close();
  }

  BufferManager::freeToMark(mark);
  return success;
}


