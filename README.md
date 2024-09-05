# ZX Spectrum 48k Arduino Interface - 48k .sna Loader
This project aims to load .SNA files into the 48k ZX Spectrum using a low-cost hardware interface built around an Arduino. The interface features an SD card slot (hidden at the back) for loading .SNA snapshots directly from an SD card. An OLED display provides basic status information, while the game list is shown on the ZX Spectrum's screen. It also includes a port for a Kempston joystick.

The ZX Spectrum screen will display a list of 24 games per page, which can be scrolled through using the joystick or the buttons on the unit. The interface can load a full game in around one half seconds.  The Unit has file selector buttons with forward, backward, and play. 
Pressing the play button in-game will return you to the game selection screen.  
Currently games must be in .sna format on the root of a FAT32 SD card.

<img src="/Documents/ZX-Spectrum-Interface_2024-09-05.png" alt="Description" width="35%" height="35%">
<img src="/Documents/Schematic_ZX-Spectrum-Interface_2024-09-05.png" alt="Description" width="65%" height="65%">
