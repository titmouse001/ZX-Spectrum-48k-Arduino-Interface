@if not exist "C:\skoolkit-10.0\snapinfo.py" (
    @echo SKOOLKIT IS MISSING! Please install skoolkit-10.0
    @pause
    @exit /b
)

for %%i in (*) do python C:\skoolkit-10.0\snapinfo.py %%i

pause