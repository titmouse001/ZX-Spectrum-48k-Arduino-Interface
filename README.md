# ZX Spectrum 48k Game Loading Interface using an Arduino

Arduino-based ZX Spectrum 48K game loader - Load .SNA, .Z80, .SCR, and .TXT files from SD card. Fast game loading, on-screen game menu, selectable with Spectrum keyboard or Kempston joystick. New PCB (v0.21) with 90° cartridge design.
  
#### Latest Ver0.21
<div style="float:left;margin:0 10px 10px 0" markdown="1">
<img src="/Documents/Pictures0.21/20250823_105518.jpg" width="24%" >
<img src="/Documents/Pictures0.21/20250823_103559.jpg" width="24%" >
<img src="/Documents/Pictures0.21/20250823_103330.jpg" width="24%" >
<img src="/Documents/Pictures0.21/20250714_210834.jpg" width="24%">
<div>

### Interface Features
<img align="right" src="/Documents/Pictures0.14/Robocop_screenshot.jpg" width="26%" >
<img align="right" src="/Documents/Pictures0.14/Robocop_fileSelector.jpg" width="32%" >

The Interface includes an SD card slot (hidden at the back) for loading games from an SD card in around a second.
The ZX Spectrum display shows 24 games per page, scrollable via the Spectrum's keyboard, joystick or interface menu button.
Pressing the interface menu button during a game will return you to the game selection screen.
Currently, games must be in .sna or .z80 format and placed in the root directory of a FAT16-formatted SD card.
The interface can start up in the standard Spectrum ROM if you hold down the menu button during power-up.

## Hardware Design (Ver 0.21)
I've been using JLCPCB with EasyEDA for my PCB design and fabrication, as EasyEDA is a free and simple-to-use circuit designer

<img src="/Documents/Pictures0.21/PCB_Unpopulated_v0.21.png" alt="Front" width="23%">
<img src="/Documents/Pictures0.21/PCB_Back_v0.21.png" alt="Back" width="28%" >
<img src="/Documents/Pictures0.21/PCB_Front_v0.21.png" alt="Photo view" width="26%" >

## PCB Created With EasyEDA (images from older Ver0.14)

<img src="/Documents/Pictures0.14/Back3DView-ZX-Spectrum-Interface_2024-09-05.png" alt="Back" width="18%" >
<img src="/Documents/Pictures0.14/ZX-Spectrum-Interface_2024-09-05.png" alt="Photo view" width="20%" >
<img src="/Documents/Pictures0.14/Font3DView-ZX-Spectrum-Interface_2024-09-05.png" alt="Front" width="25%">

## Hardware Design (Ver 0.14)
<img align="right" src="Documents/Schematic/Schematic_ZX-Spectrum-Interface-v0.14.png" alt="Circuit Diagram" width="50%" height="50%">
The hardware design uses a minimal chip count. It includes a 27C256 EPROM, which holds the Z80 machine code for accepting data and restoring the snapshot state. The Arduino manages data transfer to the ZX Spectrum, coordinating the interface. The interface primarily utilizes the data bus, with glue logic enabling the external ROM’s function.

To address issues caused by returning to the original internal ROM at game start, the setup uses a duplicated Spectrum ROM. The first half of the external EPROM contains the .SNA loading and launch code, allowing the ZX Spectrum to use the external ROM during startup. Once a game is loaded, the second half of the EPROM provides the stock ROM for normal operation. The Arduino Nano, with its limited pin count, employs a 74HC165D shift register to support a joystick. The 74HC245D transceiver allows the interface to enter a high-impedance (Z) state to avoid conflicts between the Arduino and the data bus. The 74HC32 provides the necessary glue logic to manage and monitor I/O signals.

https://oshwlab.com/titmouse001/zx-spectrum-interface (<i>link to older Ver 0.14</i>)

## Initial Prototypes...
##### Ver0.14 
<div style="float:left;margin:0 10px 10px 0" markdown="1">
<img src="/Documents/Pictures0.14/setupview3_Version0_14.jpg" width="32%" height="32%">
<img src="/Documents/Pictures0.14/UnitView2_Version0_14.jpg" width="28%" height="29%">
<img src="/Documents/Pictures0.14/setupView2_Version0_14.jpg" width="33%" height="33%">
<div>
  
#### Early Breadboard Prototype
<div style="float:left;margin:0 10px 10px 0" markdown="1">
<img src="/Documents/Initial Prototype.jpg" width="32%" height="32%">
<img src="/Documents/Initial Prototype close-up.jpg" width="32%" height="32%">
<img src="/Documents/Initial Prototype output example.jpg" width="32%" height="32%">
<div>

#### Prototype Evolving...
<img src="/Documents/Prototype Evolving zoomed.jpg" width="32%" height="32%">
<img src="/Documents/Prototype Evolving.jpg" width="32%" height="32%">
<img src="/Documents/Prototype Evolving with output view.jpg" width="32%" height="32%">

#### Next Step - The ROM
<img src="/Documents/Next step - The ROM.jpg" width="60%" height="60%">

#### Warning: No EPROMs were harmed in the making of this project… but they were definitely burned a few times!
<div style="float:left;margin:0 10px 10px 0" markdown="1">
<img src="/Documents/Setup with ROM prototype.jpg" width="32%" height="32%">
<img src="/Documents/No EPROMs Were Harmed.jpg" width="32%" height="32%">
<img src="/Documents/Burned, Not Harmed.jpg" width="32%" height="32%">
<div>
  
#### Making Progress ROM On Stripboard
<div style="float:left;margin:0 10px 10px 0" markdown="1">
<img src="/Documents/Making Progress ROM on Stripboard.jpg" width="48%" height="48%">
<img src="/Documents/Stripboard Prototype in two sections.jpg" width="48%" height="48%">
<div>

#### First PCB - Mistakes Were Made!
<img src="/Documents/First PCB - mistakes were made - Back View.jpg" width="48%" height="48%">
<img src="/Documents/First PCB - mistakes were made - Front View.jpg" width="48%" height="48%">
