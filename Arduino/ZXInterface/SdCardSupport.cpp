#include <stdint.h>
#include "SdCardSupport.h"
#include "BufferManager.h"
#include "menu.h"
#include "Pin.h"
#include "Utils.h"
#include "src/fatlib/SdFat.h" // SdFat version 2.3.1 

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
  FatFile& root = SdCardSupport::reopenRoot();
//  root.open("/");

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
        // Now you can safely use getName() into the allocated space
        size_t len = strlen(localPath);
        localPath[len] = '/';
        file->getName(localPath + len + 1, totalPathLen - len - 1);
      }

      root.close();
      if (!root.open(localPath)) {
        menuPathDepth = 0;
        SdCardSupport::reopenRoot();
        //root.open("/");
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
  do {
    file.close(); 
    if (!file.openNext(&root, O_RDONLY)) {
      return nullptr; // Directory empty or end of directory reached
    }
  } while (file.isHidden() || index++ != searchIndex);
  return &file; // The loop only exits naturally when we find the exact file
}

uint16_t SdCardSupport::countSnapshotFiles() {
  //closeFileIfOpen();
  file.close();
  root.rewind();
  DirFat_t dir;
  uint16_t totalFiles = 0;
  while (root.readDir(&dir) > 0) {
    uint8_t firstChar = dir.name[0];   
    if (firstChar == FAT_NAME_FREE) break;  // nothing left
    if (firstChar == '.' || firstChar == FAT_NAME_DELETED) continue; 
    // As was are peeking at internal name[11], we must filter to skip Hidden, System, Label, and Long File Names
    if (dir.attributes & (FAT_ATTRIB_LONG_NAME | FS_ATTRIB_HIDDEN | FS_ATTRIB_SYSTEM | FAT_ATTRIB_LABEL)) {
      continue;
    }
    totalFiles++;
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

  // caller has this passed in from a static - need to reset each time.
  fileName[4]='0';
  fileName[5]='0';
  fileName[6]='0';
  fileName[7]='0';

  file.close();
  // happy to let "SHOT9999" roll over to "SHOU0000"
  for (uint16_t i = 0; i < 9999; i++) {
    // Ripple-carry "0000" -> "9999"
    uint8_t idx = 7;
    while (++fileName[idx] > '9') {
      fileName[idx--] = '0';
    }

    if (!file.open(&dir, fileName, O_READ)) {
      return true;  // found one
    }
    file.close();
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


