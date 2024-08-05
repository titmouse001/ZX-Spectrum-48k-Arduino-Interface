del /Q *.tap 
del /Q *.bin 

pasmo-0.5.4.beta2\pasmo --bin SnaLauncher.asm .\SnaLauncher.bin

::pasmo-0.5.4.beta2\pasmo --tapbas Dredd48kRomTest.asm .\Dredd48kRomTest.tap
::pasmo-0.5.4.beta2\pasmo --bin Dredd48kRomTest.asm .\Dredd48kRomTest.bin

:: pasmo-0.5.4.beta2\pasmo --bin Spectrum48.asm .\48.rom
::pasmo-0.5.4.beta2\pasmo --bin 1.asm .\1.bin
::pasmo-0.5.4.beta2\pasmo --bin 2.asm .\2.bin

@echo off
for %%A in ("*.bin") do (
    echo Filename: %%~nxA
    echo Size: %%~zA bytes
)

copy /b SnaLauncher.bin+48.rom EPROM.bin


REM pasmo-0.5.4.beta2\pasmo --bin hi.asm .\hi.tap

rem creating BIN as an easy way to keep a note of the filesize