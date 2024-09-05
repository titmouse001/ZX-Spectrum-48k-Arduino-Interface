# ZX Spectrum 48k Arduino Interface - 48k .sna Loader
This project aims to load .SNA files into the 48k ZX Spectrum using a low-cost hardware interface built around an Arduino. The interface features an SD card slot (hidden at the back) for loading .SNA snapshots directly from an SD card. An OLED display provides basic status information, while the game list is shown on the ZX Spectrum's screen. It also includes a port for a Kempston joystick.

<img src="/Documents/Font3DView-ZX-Spectrum-Interface_2024-09-05.png" alt="Front" width="25%" height="25%">

The ZX Spectrum screen will display a list of 24 games per page, which can be scrolled through using the joystick or the buttons on the unit. The interface can load a full game in around one half seconds.  The Unit has file selector buttons with forward, backward, and play. 
Pressing the play button in-game will return you to the game selection screen.  
Currently games must be in .sna format on the root of a FAT32 SD card.

<img src="/Documents/Back3DView-ZX-Spectrum-Interface_2024-09-05.png" alt="Bavk" width="25%" height="25%">

The hardware design uses a minimal chip count. It includes a 27C256 EPROM holding the Z80 machine code to accept data transfers and restore the snapthot state. The Arduino does the data transfer to the ZX Spectrum, coordinating the interface. The system works mainly with the data bus, using glue logic to enable the external ROM’s role.

<img src="/Documents/ZX-Spectrum-Interface_2024-09-05.png" alt="Photo view" width="25%" height="25%">

The 27C256 EPROM is set up with a duplicated internal Spectrum ROM. The first half of the EPROM contains the .SNA loading and launch code, allowing the ZX Spectrum to use the external ROM during startup. Once a game is loaded, the second half of the EPROM provides the stock ROM for normal operation. The Arduino Nano, with its limited pin count, uses a 74HC165D shift register for joystick and button integration. The 74HC245D transceiver enables the interface to enter high-impedance (Z) state to avoid conflicts with the data bus when not in use by the Z80 CPU. The 74HC32 provides the necessary glue logic to manage and monitor I/O request signals.

<img src="/Documents/Schematic_ZX-Spectrum-Interface_2024-09-05.png" alt="Circuit Diagram" width="60%" height="60%">
