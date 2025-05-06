del /Q *.tap 
del /Q .\output\*.bin 
if not exist .\output mkdir .\output

..\..\pasmo-0.5.4.beta2\pasmo -v --tapbas Dredd48kRomTest.asm  .\output\Dredd48kRomTest.tap

pause

