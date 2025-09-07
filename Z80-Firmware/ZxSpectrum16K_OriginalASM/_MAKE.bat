@echo off
color 0A
setlocal enabledelayedexpansion

del /Q *.bin >nul 2>&1
del /Q *.lst >nul 2>&1

set ASM_FILE=48KROM.asm
set BIN_FILE=48KROM.bin
set ASM_TOOL=..\..\tools\pasmo-0.5.4.beta2\pasmo.exe
set REF_FILE=..\48.rom

:: Uncomment this if you want the .lst debug file
REM set DEBUG_LST=48KROM.lst
:: -------------------------------

echo Assembling modified stock ROM: %ASM_FILE%
echo.

if defined DEBUG_LST (
    "%ASM_TOOL%" --bin -d "%ASM_FILE%" "%BIN_FILE%" > "%DEBUG_LST%"
) else (
    "%ASM_TOOL%" --bin "%ASM_FILE%" "%BIN_FILE%"
)

if exist "%BIN_FILE%"   echo file:%BIN_FILE% created OK
if defined DEBUG_LST if exist "%DEBUG_LST%" echo file:%DEBUG_LST% created OK

echo.
::pause
endlocal
