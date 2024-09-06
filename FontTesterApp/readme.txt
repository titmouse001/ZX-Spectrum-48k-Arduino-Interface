FontTester.sln

This project is designed as a test rig for displaying text on the 48k ZX Spectrum.

The objective is to adapt the Arduino font, originally used for OLED display code, for use on the Spectrum. This will enable formatted text to be sent one scan line at a time. This code serves as a proof of concept for that.

The font used for the OLED needs to be rotated 90 degrees, and a text formatting routine must be implemented to pack each character across bytes, as each character is only 6 pixels wide.
