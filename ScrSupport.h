#ifndef SCRSUPPORT_H
#define SCRSUPPORT_H

#include "pins.h"
#include "Buffers.h"
#include "Z80Bus.h"

namespace ScrSupport {

void DemoScrFiles(FatFile& root, FatFile& file, byte* packetBuffer) {
  while (1) {
    root.rewind();
    while (file.openNext(&root, O_RDONLY)) {
      if (file.isFile()) {
        if (file.fileSize() == 6912) {
          uint16_t currentAddress = 0x4000;  // screen start
          while (file.available()) {
            byte bytesRead = (byte)file.read(&packetBuffer[SIZE_OF_HEADER], PAYLOAD_BUFFER_SIZE);
            ASSIGN_16BIT_COMMAND(packetBuffer, 'C', bytesRead);
            ADDR_16BIT_COMMAND(packetBuffer, currentAddress);
            Z80Bus::sendBytes(packetBuffer, SIZE_OF_HEADER + bytesRead);
            currentAddress += bytesRead;
          }
          delay(2000);
        }
      }
      file.close();
    }
  }
}

}

#endif