# Z80 Firmware Build Instructions

## Overview

The external **27C256 EPROM** (32KB) serves a dual purpose: acting as a custom firmware loader in its lower half, and providing the original ZX Spectrum 48K ROM passthrough in its upper half.

* **Hardware Reference:** Schematics and Gerber files are located in the `docs/` folder.
* **PCB Logic Components:** 74HC165D, 74HC245D, 74HC32, SN74HC125D / 74LVC125APW.

---

## Firmware Build Process

1. Open the `Z80-Firmware/` folder.
2. Run `_MAKE.bat` to assemble and concatenate the binaries.

The build script compiles and joins two 16KB sections into a single 32KB image:

* `SnaLauncher.asm` — Custom Z80 loader firmware (first 16KB)
* `ZxSpectrum16K_OriginalASM/48KROM.asm` — Original ZX Spectrum 48K ROM (second 16KB)

---

## EPROM Programming

Flash the resulting **32KB binary** (`EPROM_PAIR.bin`) to a **27C256 EPROM** (256Kbit, e.g., M27C256B or any standard 27C256 variant).

> **Note:** Any standard, low-cost universal USB/eBay programmer (such as a TL866 series) will work for flashing this IC.

---

## File Reference

| File / Path | Description |
| --- | --- |
| `_MAKE.bat` | Build script that assembles source files and joins ROMs |
| `output/EPROM_PAIR.bin` | **Final 32KB image** to flash to the EPROM |
| `output/SnaLauncher.bin` | Intermediate compiled loader file (16KB) - safe to ignore |
| `ZxSpectrum16K_OriginalASM/48.rom` | Stock ZX Spectrum 48K ROM source/binary (16KB) |