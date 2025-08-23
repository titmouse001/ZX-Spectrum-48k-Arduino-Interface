## Overview

- External 27C256 EPROM serves dual purposes: loader and ROM passthrough
- Schematics and Gerbers are located under the docs folder
  - PCB with minimal logic: 74HC165D, 74HC245D, 74HC32, SN74HC125D / 74LVC125APW

## Build
- Assemble the firmware in the Z80-Firmware folder:
  - Run _MAKE.bat to assemble the firmware
  - Output folder: Z80-Firmware\output
    - EPROM.bin (32KB)
- Burn EPROM.bin to a 27C256 EPROM
  - This ROM will hold the assembled Z80 firmware binary
    - The first half contains the firmware loader; the second half contains the Spectrum ROM
  - Any cheap universal USB EPROM programmer will work




