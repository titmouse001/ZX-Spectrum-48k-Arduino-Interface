del /Q .\output\*.bin 
del /Q .\output\*.tap 
if not exist .\output mkdir .\output

..\..\pasmo-0.5.4.beta2\pasmo -v --tapbas Dredd48kRomTest.asm  .\output\Dredd48kRomTest.tap

..\..\pasmo-0.5.4.beta2\pasmo --tapbas TestRig.asm .\output\TestRig.tap


pause

