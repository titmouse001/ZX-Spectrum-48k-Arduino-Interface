# ZX Spectrum 48k Arduino Interface - 48k .sna Game Loader

<i>Latest quick update: new PCB (v0.21) now uses a right-angle cartridge (90°). No pics just yet! 
Code has been updated to support .z80 snapshots (now loads .sna, .z80, and .scr files).  
For the PCB, see "Gerbers" under the docs section.  
Currently, you need to compile the Speccy ASM for the ROM manually – I'll include a prebuilt image at some point.
</i>

#### This project aims to load .SNA files into the 48k ZX Spectrum using a low-cost hardware interface built around an Arduino.
<div style="float:left;margin:0 10px 10px 0" markdown="1">
<img src="/Documents/Pictures0.14/setupview3_Version0_14.jpg" width="32%" height="32%">
<img src="/Documents/Pictures0.14/UnitView2_Version0_14.jpg" width="28%" height="28%">
<img src="/Documents/Pictures0.14/setupView2_Version0_14.jpg" width="33%" height="33%">
<div>
  
### Interface Features
  
<img align="right" src="/Documents/Pictures0.14/Robocop_fileSelector.jpg" width="36%" >

The Snapshot Game Interface for the ZX Spectrum 48K includes an SD card slot (hidden at the back) for loading .SNA snapshots directly from an SD card. An OLED display provides basic status information, while the game list is shown on the ZX Spectrum's screen. It also features a port for a Kempston joystick.

The ZX Spectrum display shows 24 games per page, scrollable via the joystick or interface buttons, with full game loading taking about half a second using the forward, backward, and play file selector buttons.

Pressing the play button during a game will return you to the game selection screen.

Currently, games must be in .sna format and placed in the root directory of a FAT32-formatted SD card.

<img align="right" src="/Documents/Pictures0.14/Robocop_screenshot.jpg" width="30%" >

### Additional Features
The interface offers some additional features. If you hold down the left button on power-up, it will display all the .SCR files on the SD card. (.SCR files are screen dumps from the ZX Spectrum)

_.SNA files, on the other hand, are snapshot files that capture the entire state of the ZX Spectrum’s memory and registers at a specific moment allowing you to resume a game from the exact point it was saved. Currently, the interface cannot save to .SNA files._

Additionally, the interface can start up in the standard Spectrum ROM if you hold down the middle play button during power-up.



## PCB Created With EasyEDA

<img src="/Documents/Pictures0.14/Back3DView-ZX-Spectrum-Interface_2024-09-05.png" alt="Back" width="18%" >
<img src="/Documents/Pictures0.14/ZX-Spectrum-Interface_2024-09-05.png" alt="Photo view" width="20%" >
<img src="/Documents/Pictures0.14/Font3DView-ZX-Spectrum-Interface_2024-09-05.png" alt="Front" width="25%">

I've been using JLCPCB with EasyEDA for my PCB design and fabrication, as EasyEDA is a free and simple-to-use circuit designer

## Hardware Design (Ver 0.14)
<img align="right" src="Documents/Schematic/Schematic_ZX-Spectrum-Interface-v0.14.png" alt="Circuit Diagram" width="50%" height="50%">
The hardware design uses a minimal chip count. It includes a 27C256 EPROM, which holds the Z80 machine code for accepting data and restoring the snapshot state. The Arduino manages data transfer to the ZX Spectrum, coordinating the interface. The interface primarily utilizes the data bus, with glue logic enabling the external ROM’s function.

To address issues caused by returning to the original internal ROM at game start, the setup uses a duplicated Spectrum ROM. The first half of the external EPROM contains the .SNA loading and launch code, allowing the ZX Spectrum to use the external ROM during startup. Once a game is loaded, the second half of the EPROM provides the stock ROM for normal operation. The Arduino Nano, with its limited pin count, employs a 74HC165D shift register to support a joystick. The 74HC245D transceiver allows the interface to enter a high-impedance (Z) state to avoid conflicts between the Arduino and the data bus. The 74HC32 provides the necessary glue logic to manage and monitor I/O signals.

https://oshwlab.com/titmouse001/zx-spectrum-interface

## Initial Prototypes...
<div style="float:left;margin:0 10px 10px 0" markdown="1">
<img src="/Documents/Initial Prototype.jpg" width="32%" height="32%">
<img src="/Documents/Initial Prototype close-up.jpg" width="32%" height="32%">
<img src="/Documents/Initial Prototype output example.jpg" width="32%" height="32%">
<div>

## Prototype Evolving...
<img src="/Documents/Prototype Evolving zoomed.jpg" width="32%" height="32%">
<img src="/Documents/Prototype Evolving.jpg" width="32%" height="32%">
<img src="/Documents/Prototype Evolving with output view.jpg" width="32%" height="32%">

## Next Step - The ROM
<img src="/Documents/Next step - The ROM.jpg" width="60%" height="60%">

### Warning: No EPROMs were harmed in the making of this project… but they were definitely burned a few times!
<div style="float:left;margin:0 10px 10px 0" markdown="1">
<img src="/Documents/Setup with ROM prototype.jpg" width="32%" height="32%">
<img src="/Documents/No EPROMs Were Harmed.jpg" width="32%" height="32%">
<img src="/Documents/Burned, Not Harmed.jpg" width="32%" height="32%">
<div>
  
## Making Progress ROM On Stripboard
<div style="float:left;margin:0 10px 10px 0" markdown="1">
<img src="/Documents/Making Progress ROM on Stripboard.jpg" width="48%" height="48%">
<img src="/Documents/Stripboard Prototype in two sections.jpg" width="48%" height="48%">
<div>

## First PCB - Mistakes Were Made!
<img src="/Documents/First PCB - mistakes were made - Back View.jpg" width="48%" height="48%">
<img src="/Documents/First PCB - mistakes were made - Front View.jpg" width="48%" height="48%">
