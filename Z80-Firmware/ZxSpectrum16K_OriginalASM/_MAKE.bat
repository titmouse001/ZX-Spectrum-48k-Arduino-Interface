@echo off
color 0A
setlocal enabledelayedexpansion

del /Q *.bin 
del /Q *.lst

set ASM_FILE=48KROM.asm
set BIN_FILE=48KROM.bin
set DEBUG_LST=48KROM.lst
set ASM_TOOL=..\..\tools\pasmo-0.5.4.beta2\pasmo.exe
set REF_FILE=..\48.rom

echo Assembling file: %ASM_FILE%
echo.

"%ASM_TOOL%" --bin -d "%ASM_FILE%" "%BIN_FILE%" > "%DEBUG_LST%"

if exist "%DEBUG_LST%" echo file:%DEBUG_LST% created OK
if exist "%BIN_FILE%"   echo file:%BIN_FILE% created OK
echo.

pause

endlocal
