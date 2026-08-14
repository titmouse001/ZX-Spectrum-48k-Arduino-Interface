#include <stdint.h>
#include "SdCardSupport.h"
#include "BufferManager.h"
#include "menu.h"
#include "Pin.h"
#include "Utils.h"

SdFat32 SdCardSupport::sd;
FatFile SdCardSupport::root;
FatFile SdCardSupport::file;

uint16_t SdCardSupport::menuPathHistory[FOLDER_NAV_DEPTH];
uint8_t  SdCardSupport::menuPathDepth = 0;

namespace {
  constexpr uint32_t SD_PRESENCE_CACHE_MS = 250;
  bool cachedCardPresent = false;
  bool cardPresenceValid = false;
  uint32_t lastCardPresenceCheck = 0;
}

bool SdCardSupport::init() { //uint8_t csPin) {
  sd.end();
  cardPresenceValid = false;

  // NOTE: SPI_HALF_SPEED looks best for NANO (tried 'SPI_DIV6_SPEED' gave me same read times)
  if (!sd.begin(Pin::SD_CARD_CS, SPI_HALF_SPEED)) return false;
  root.close();
  const bool opened = root.openRoot(sd.vol());
  cachedCardPresent = opened;
  cardPresenceValid = true;
  lastCardPresenceCheck = millis();
  return opened;
}

void SdCardSupport::syncRootToDepth() {
  reopenRoot(); // Reset to "SD:/" top-level root
  for (uint8_t i = 0; i < menuPathDepth; i++) {
    // note: openFileByIndex opens using a global 'file' 
    FatFile* foundFile = SdCardSupport::openFileByIndex(menuPathHistory[i]);
    if (foundFile && foundFile->isDir()) {
      root.move(foundFile); // transfer folder context
    } else {
      menuPathDepth = 0;
      reopenRoot();
      break;  // fallback
    }
  }
  Menu::inSubFolder = (menuPathDepth > 0);
}

FatFile* SdCardSupport::openFileByIndex(uint16_t searchIndex) {
  uint16_t index = 0; 
  root.rewind();
  file.close(); 
  
  while (file.openNext(&root, O_RDONLY)) {
    if (!file.isHidden()) { 
      if (index == searchIndex) return &file; // Found it! Leave it open
      index++; 
    }
    file.close(); // Not a match
  }
  return nullptr; // nothing found
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
  file.close();
  root.close(); 
  root.openRoot(sd.vol());
  return root;
}

bool SdCardSupport::findFreeFilename(FatFile& dir, char* fileName) {
  fileName[4] = '0';
  fileName[5] = '0';
  fileName[6] = '0';
  fileName[7] = '0';
  for (uint16_t i = 0; i < 9999; i++) {
    uint8_t idx = 7;
    while (++fileName[idx] > '9') {   // pre ++ start at "0001"
      fileName[idx--] = '0';
    }
    if (!dir.exists(fileName)) {
      return true;  // found an empty slot!
    }
  }
  return false; // All 9999 slots full
}

bool SdCardSupport::copyScratchTo(FatFile& dir, const char* toFileName) {
  file.close();
  if (!file.open(&root, SCRATCH_FILE, O_READ)) {
    return false;
  }

  FatFile destFile;
  if (!destFile.open(&dir, toFileName, O_CREAT | O_WRITE | O_TRUNC)) {
    file.close();
    return false;
  }

  uint16_t mark = BufferManager::getMark();
  uint8_t* buf = BufferManager::allocate(FILE_READ_BUFFER_SIZE);

  bool success = true;
  uint8_t bytesRead; // buffer size is of type uint8_t
  // NOTE: open() puts the pointer at byte 0
  while ((bytesRead = file.read(buf, FILE_READ_BUFFER_SIZE)) > 0) {
    if (destFile.write(buf, bytesRead) != bytesRead) {
      success = false;
      break;
    }
  }

  BufferManager::freeToMark(mark);
  destFile.close();
  file.close();

  return success;
}

bool SdCardSupport::openOrCreateDirectory(FatFile& dir, const char* folderName) {
  FatFile& root = SdCardSupport::reopenRoot(); //
  return dir.open(&root, folderName, O_READ) || dir.mkdir(&root, folderName);
}

bool SdCardSupport::isInserted() {
  const uint32_t now = millis();
  
  if (cardPresenceValid && (now - lastCardPresenceCheck < SD_PRESENCE_CACHE_MS)) {
    return cachedCardPresent;
  }

  cid_t cid;
  cachedCardPresent = sd.card() && sd.card()->readCID(&cid);
  lastCardPresenceCheck = now;
  // valid for the next 250ms, whether the card is actually present or not
  cardPresenceValid = true; 
  return cachedCardPresent;
}