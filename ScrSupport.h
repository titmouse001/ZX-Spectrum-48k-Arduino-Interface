#ifndef SCRSUPPORT_H
#define SCRSUPPORT_H

#include "pin.h"
#include "Buffers.h"
#include "Z80Bus.h"
#include "SdFat.h" 

namespace ScrSupport {

void DemoScrFilesxxxx(FatFile& root, FatFile& file, byte* packetBuffer) {
  // while (1) {
  //   root.rewind();
  //   while (file.openNext(&root, O_RDONLY)) {
  //     if (file.isFile()) {
  //       if (file.fileSize() == 6912) {
  //         uint16_t currentAddress = 0x4000;  // screen start
  //         while (file.available()) {
  //           byte bytesRead = (byte)file.read(&packetBuffer[SIZE_OF_HEADER], COMMAND_PAYLOAD_SECTION_SIZE);
  //           START_UPLOAD_COMMAND(packetBuffer, 'C', bytesRead);
  //           ADDR_UPLOAD_COMMAND(packetBuffer, currentAddress);
  //           Z80Bus::sendBytes(packetBuffer, SIZE_OF_HEADER + bytesRead);
  //           currentAddress += bytesRead;
  //         }
  //         delay(2000);
  //         Z80Bus::fillScreenAttributes(0);   // setup the whole screen
  //       }
  //     }
  //     file.close();
  //   }
  // }
}

}

#endif