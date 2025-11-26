#include <stdint.h>
#include "SdCardSupport.h"
#include "BufferManager.h"
#include "SdFat.h"

SdFat32 SdCardSupport::sd;
FatFile SdCardSupport::root;
FatFile SdCardSupport::file;

 __attribute__((optimize("-Os")))
boolean SdCardSupport::init(uint8_t csPin) {
  if (root.isOpen()) {
    root.close();
    sd.end();
  }
  if (!sd.begin(csPin,SPI_QUARTER_SPEED )) {
    return false;
  }
  if (!root.open("/")) {
    sd.end();
    return false;
  }
  return true;
}

__attribute__((optimize("-Os"))) 
void SdCardSupport::openFileByIndex(uint8_t searchIndex) {
  root.rewind();
  uint8_t index = 0;
  while (file.openNext(&root, O_RDONLY)) {
    if (!file.isHidden() &&  (file.isFile() || file.isDir())) {
      if (index == searchIndex) {
        break;
      }
      index++;
    }
    file.close();
  }
}


uint16_t SdCardSupport::countSnapshotFiles() {

  if (file.isOpen()) {
    file.close();
  }

  uint16_t totalFiles = 0;
  root.rewind();
  DirFat_t dir;

  // readDir: see FsStructs.h for struct layout and defines
  while (root.readDir(&dir) > 0) {
    if (dir.name[0] == FAT_NAME_FREE) {
      break;  // as in "Entry is free", no more entries exist in this dir.
    }
    if (dir.name[0] == '.'  ||  // Hide . and .. invisible links
        dir.name[0] == FAT_NAME_DELETED) {
      continue;
    }
    uint8_t attr = dir.attributes;
    if ((attr & 0x3F) == FAT_ATTRIB_LONG_NAME) {
      continue;  // skip LFN - count just "8.3" filenames
    }
    if (attr & (FS_ATTRIB_HIDDEN | FS_ATTRIB_SYSTEM | FAT_ATTRIB_LABEL)) {
      continue;
    }
    totalFiles++;


    // Serial.print(totalFiles,HEX);
    // Serial.print("-");
    // if (attr & FS_ATTRIB_DIRECTORY) {
    //   Serial.print(" DIR:");
    // }else {
    //    Serial.print("FILE:");
    // }
    // for (int i = 0; i < 8; i++) {
    //   Serial.print((char)dir.name[i]);
    // }
    // Serial.print(".");
    // Serial.print((char)dir.name[8]);
    // Serial.print((char)dir.name[9]);
    // Serial.print((char)dir.name[10]);

    // Serial.print(", SIZE:");
    // Serial.print(dir.fileSize[3], HEX);
    // Serial.print(dir.fileSize[2], HEX);
    // Serial.print(dir.fileSize[1], HEX);
    // Serial.println(dir.fileSize[0], HEX);
  }
  return totalFiles;
}


// __attribute__((optimize("-Ofast")))
// uint16_t SdCardSupport::countSnapshotFiles() {

//   if (file.isOpen()) {
//     file.close();
//   }

//   uint16_t totalFiles = 0;
//   root.rewind();
//   while (file.openNext(&root, O_RDONLY)) {
//     if (!file.isHidden() && (file.isFile() || file.isDir()) ) {
//       totalFiles++;
//     }
//     file.close();
//   }
//   return totalFiles;
// }

// This version of getFileName is used for the file browser view, keeping this one optimised for speed.
__attribute__((optimize("-Ofast")))
uint8_t SdCardSupport::getFileName(FatFile* pFile , char* pFileNameBuffer) {
  uint8_t len = pFile->getName7(pFileNameBuffer, MAX_FILENAME_LEN);
  if (len == 0) {
    len = pFile->getSFN(pFileNameBuffer, 13); // fallback, XXXXXXXX.XXX
  }
  return len;
}

char* SdCardSupport::getFileName(FatFile* pFile) {
  char* buffer = (char*)&BufferManager::packetBuffer[FILE_READ_BUFFER_OFFSET];
  getFileName(pFile,buffer);
  return buffer;
}

char* SdCardSupport::getFileNameWithSlash(FatFile* pFile) {
  char* buffer = (char*)&BufferManager::packetBuffer[FILE_READ_BUFFER_OFFSET];
  getFileName(pFile,&buffer[1]);
  buffer[0]='/';
  return buffer;
}
