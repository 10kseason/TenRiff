@echo off
setlocal

set "ROOT=%~dp0..\.."
set "BUILD=%ROOT%\build-bms-key-converter"

cmake -S "%ROOT%\tools\bms-key-converter" -B "%BUILD%" -G "Visual Studio 17 2022" -A x64
if errorlevel 1 exit /b %errorlevel%

cmake --build "%BUILD%" --config Release --target bms_key_converter
if errorlevel 1 exit /b %errorlevel%

cmake --build "%BUILD%" --config Release --target bms_key_converter_gui
if errorlevel 1 exit /b %errorlevel%

echo.
echo Built:
echo   %BUILD%\Release\bms_key_converter.exe
echo   %BUILD%\Release\bms_key_converter_gui.exe
