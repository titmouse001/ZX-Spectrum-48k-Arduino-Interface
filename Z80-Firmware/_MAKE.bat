@echo off
echo -------------------------------------
echo.
cd ZxSpectrum16K_OriginalASM
call _make.bat
cd ..
echo -------------------------------------
echo.

@echo off
setlocal enabledelayedexpansion
color 0A

set ASM_FILE=.\SnaLauncher.asm
set BIN_FILE=.\output\SnaLauncher.bin
set LST_FILE=.\SnaLauncher.lst
set ASM_TOOL=..\tools\pasmo-0.5.4.beta2\pasmo.exe

set ROM_FILE=48.rom
set EXTRA_BIN=.\ZxSpectrum16K_OriginalASM\48KROM.bin

:: Uncomment to enable debug list output
:: set DEBUG_LST=1

if not exist .\output mkdir .\output
del /Q .\output\*.bin >nul 2>&1
if defined DEBUG_LST del /Q "%LST_FILE%" >nul 2>&1

echo Assembling %ASM_FILE%...
if defined DEBUG_LST (
    "%ASM_TOOL%" --bin -d "%ASM_FILE%" "%BIN_FILE%" > "%LST_FILE%"
) else (
    "%ASM_TOOL%" --bin "%ASM_FILE%" "%BIN_FILE%"
)

echo.

if not exist "%BIN_FILE%" (
    echo ERROR: %BIN_FILE% was not created.
    pause
    exit /b 1
) else (
    echo OK: %BIN_FILE% created.
)

if defined DEBUG_LST if exist "%LST_FILE%" (
    echo OK: %LST_FILE% created.
)

for %%A in ("%BIN_FILE%") do (
    echo Filename: %%~nxA
    echo Size: %%~zA bytes
)

echo Building EPROM image...
::copy /b "%BIN_FILE%"+%EXTRA_BIN% .\output\EPROM2.bin >nul
::copy /b "%BIN_FILE%"+%ROM_FILE% .\output\EPROM.bin >nul

copy /b "%BIN_FILE%"+%EXTRA_BIN% .\output\EPROM_PAIR.bin >nul

del /Q "%BIN_FILE%" >nul

echo.
echo Build complete!
echo.
echo -------------------------------------
pause

endlocal
