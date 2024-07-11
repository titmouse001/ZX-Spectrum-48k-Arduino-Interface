
del /Q *.tap 
del /Q *.bin 
pasmo-0.5.4.beta2\pasmo --tapbas screenin.asm .\screenIN.tap
pasmo-0.5.4.beta2\pasmo --bin screenin.asm .\screenIN.bin

pasmo-0.5.4.beta2\pasmo --tapbas screenTest.asm .\screenTest.tap
pasmo-0.5.4.beta2\pasmo --bin screenTest.asm .\screenTest.bin

@echo off
for %%A in ("*.bin") do (
    echo Filename: %%~nxA
    echo Size: %%~zA bytes
)
pause

rem creating BIN as an easy way to keep a note of the filesize