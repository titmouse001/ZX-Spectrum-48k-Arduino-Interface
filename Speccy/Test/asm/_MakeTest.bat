del /Q *.tap 
del /Q .\output\*.bin 
if not exist .\output mkdir .\output


..\..\pasmo-0.5.4.beta2\pasmo -v --tapbas Dredd48kRomTest.asm  .\output\Dredd48kRomTest.tap

..\..\pasmo-0.5.4.beta2\pasmo --bin Test_SnaLauncher.asm .\output\Test_SnaLauncher.bin

copy /b .\output\Test_SnaLauncher.bin .\output\ZXS48.rom
pause

