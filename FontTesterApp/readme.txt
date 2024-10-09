FontTester.sln

This project renders a font on a PC, serving as a proof of concept for displaying text on the 48k ZX Spectrum. With this, 
I can now implement the same approach on an Arduino and directly on a real Spectrum, without too much head-scratching.

(The font is taken from the Arduino OLED display library and neededs to be rotated 90 degrees and packed
into a 6-pixel-wide character format across bytes)

I've use the OneLoneCoder library for this:-

This code uses the pixel lbrary: https://github.com/OneLoneCoder/olcPixelGameEngine
