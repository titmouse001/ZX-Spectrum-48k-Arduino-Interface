
del /Q *.tap 
del /Q *.bin 
pasmo-0.5.4.beta2\pasmo -v --tapbas screenin.asm .\screenIN.tap
pasmo-0.5.4.beta2\pasmo -v --bin screenin.asm .\screenIN.bin

pause

REM pasmo-0.5.4.beta2\pasmo -v --tapbas Test16384.asm .\screen16384.tap
REM pasmo-0.5.4.beta2\pasmo -v --tapbas screenTest.asm output\screenTest.tap
REM pasmo-0.5.4.beta2\pasmo -v --bin screenin.asm output\screenin.bin
rem pasmo-0.5.4.beta2\pasmo -v --tapbas screeninBW.asm output\screeninBW.tap
rem pasmo-0.5.4.beta2\pasmo -v --tapbas hi.asm output\hi.tap


