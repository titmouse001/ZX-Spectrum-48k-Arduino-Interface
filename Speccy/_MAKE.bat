REM del /Q *.tap 
del /Q .\output\*.bin 
if not exist .\output mkdir .\output

pasmo-0.5.4.beta2\pasmo --bin SnaLauncher.asm .\output\SnaLauncher.bin
::pasmo-0.5.4.beta2\pasmo --bin -d SnaLauncher.asm .\output\SnaLauncher.bin


@echo off
REM Check if the .bin file was created
if not exist ".\output\SnaLauncher.bin" (
    echo Warning: .\output\SnaLauncher.bin was not created.
    pause
    exit /b 1
) else (
    echo .\output\SnaLauncher.bin was successfully created.
)

@echo off 
for %%A in ("*.bin") do (
    echo Filename: %%~nxA
    echo Size: %%~zA bytes
)

copy /b .\output\SnaLauncher.bin+48.rom .\output\EPROM.bin

::pause