# ZX Spectrum 48k Arduino Interface - 48k .sna Loader

This project aims to load .SNA files into the 48k ZX Spectrum using a low-cost hardware interface built around an Arduino. The interface features an SD card slot (hidden at the back) for loading .SNA snapshots directly from an SD card. An OLED display provides basic status information, while the game list is shown on the ZX Spectrum's screen. It also includes a port for a Kempston joystick.

<div style="float:left;margin:0 10px 10px 0" markdown="1">
<img src="/Documents/Font3DView-ZX-Spectrum-Interface_2024-09-05.png" alt="Front" width="34%" height="34%">
<img src="/Documents/Back3DView-ZX-Spectrum-Interface_2024-09-05.png" alt="Bavk" width="25%" height="25%">
<img src="/Documents/ZX-Spectrum-Interface_2024-09-05.png" alt="Photo view" width="27%" height="27%">
<div>
The ZX Spectrum screen will display a list of 24 games per page, which can be scrolled through using the joystick or the buttons on the unit. The interface can load a full game in around one half seconds.  The Unit has file selector buttons with forward, backward, and play. 
Pressing the play button in-game will return you to the game selection screen.  
Currently games must be in .sna format on the root of a FAT32 SD card.
<br>
The hardware design uses a minimal chip count. It includes a 27C256 EPROM, which holds the Z80 machine code for accepting data and restoring the snapshot state. The Arduino manages data transfer to the ZX Spectrum, coordinating the interface. The interface mainly uses the data bus, using glue logic to enable the external ROM’s role.
<img align="right" src="/Documents/Schematic_ZX-Spectrum-Interface_2024-09-05.png" alt="Circuit Diagram" width="38%" height="38%">
<br>
The setup uses a duplicated Spectrum ROM to address issues caused by returning to the original internal ROM at game start. To work around this, the first half of the external EPROM contains the .SNA loading and launch code, allowing the ZX Spectrum to use the external ROM during startup. Once a game is loaded, the second half of the EPROM provides the stock ROM for normal operation. The Arduino Nano, with its limited pin count, uses a 74HC165D shift register to allow for a joystick. The 74HC245D transceiver allows the interface to enter a high-impedance (Z) state to avoid conflicts with Arduino and the data bus. The 74HC32 provides the necessary glue logic to manage and monitor I/O signals.
<br>
  
## Initial Prototype
<div style="float:left;margin:0 10px 10px 0" markdown="1">
<img src="/Documents/Initial Prototype.jpg" width="32%" height="32%">
<img src="/Documents/Initial Prototype close-up.jpg" width="32%" height="32%">
<img src="/Documents/Initial Prototype output example.jpg" width="32%" height="32%">
<div>

## Prototype Evolving
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

