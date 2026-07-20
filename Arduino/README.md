# Arduino Firmware (ZXInterface.ino)

This Arduino sketch handles the SD card, menu system, and snapshot transfer to the ZX Spectrum.

Required 3rd-party libraries: SdFat and SSD1306.

**UPDATE:** This project includes a modified copy of the SdFat library:
https://github.com/greiman/SdFat   *(SdFat version 2.3.1)*

 The bundled version contains project-specific additions and must be used.
 Remove or rename any globally installed SdFat library
 (e.g. "<Arduino>/libraries/SdFat") before compiling to avoid conflicts.

- FatFile::getNameLength()                            *- Returns filename length (both short & long)*
- FatFile::getExtension(char* extBuf, size_t extSize) *- Returns 3 char file extension (from the 8.3 format)*
- FatFile::getDisplayName7(char* name, size_t size)   *- Returns a truncated name for limited-width displays*

Modifications are located in:
- src/FatLib/FatFile.h.h
- src/FatLib/FatName.cpp

## ZX SPECTRUM STORAGE TIPS ##
- Because ZX Spectrum snapshot files (.sna, .z80) are tiny, you can store 
  thousands of games on a single card. However, root directory performance 
  and menu load times can degrade if too many files sit in a single folder.
- Organize your files into named subfolders (e.g. alphabetically or by genre) 
  to keep directory scanning fast and make navigation much easier.
---
## Development Notes ##

This project can be built with:
- Visual Studio Code
- Arduino Community Edition extension

Default build settings (arduino.json):
   Board         : Arduino Nano (ATmega328P)
   Sketch        : ZXInterface/ZXInterface.ino
   C++ Standard  : GNU++11
   Optimisation  : -Os
   Link Time Opt : Enabled (-flto)
   Linker Flags  : --relax, generate output.map

Source formatting:
   .clang-format
     BasedOnStyle     : Google
     BreakBeforeBraces: Attach

A copy of the recommended arduino.json and .clang-format files is included with this project.
