## Overview
- Arduino **Nano**
- External **27C256 EPROM** serves dual purposes: loader and ROM passthrough
- Minimal logic: **74HC165D**, **74HC245D**, **74HC32**, **SN74HC125D/74LVC125APW**
- Schematics and Gerbers located under 'docs'

## Build
- **Arduino**: Use the '.ino' sketch in the root
- **Z80 Loader**: Assemble from the 'Z80-Firmware' folder; upload to EPROM.
- **PCB files**: Check 'docs/Gerbers'






