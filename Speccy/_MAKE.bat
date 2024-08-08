
del /Q *.tap 
del /Q *.bin 

pasmo-0.5.4.beta2\pasmo --bin SnaLauncher.asm .\SnaLauncher.bin

@echo off
for %%A in ("*.bin") do (
    echo Filename: %%~nxA
    echo Size: %%~zA bytes
)

copy /b SnaLauncher.bin+48.rom EPROM.bin