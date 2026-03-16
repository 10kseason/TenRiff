@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "BASE=%~dp0"
set "EXE="
set "PROFILE_NAME=default"
set "PROFILE_DIR=profiles\%PROFILE_NAME%"
set "LOG_DIR=logs"
set "LOG_FILE=%LOG_DIR%\run.log"
set "SONGS_DIR=songs"
set "SONGS_ARG=.\songs"

pushd "%BASE%" >nul 2>&1
if errorlevel 1 (
  echo [E] Failed to enter working directory: %BASE%
  exit /b 1
)

if not exist "config" mkdir "config" >nul 2>&1
if not exist "profiles" mkdir "profiles" >nul 2>&1
if not exist "%PROFILE_DIR%" mkdir "%PROFILE_DIR%" >nul 2>&1
if not exist "%LOG_DIR%" mkdir "%LOG_DIR%" >nul 2>&1
if not exist "%SONGS_DIR%" (
  echo [W] songs folder is missing. Creating it now.
  mkdir "%SONGS_DIR%" >nul 2>&1
)

set "HAS_SONGS="
for /f "delims=" %%i in ('dir /b /a "%SONGS_DIR%" 2^>nul') do set "HAS_SONGS=1"
if not defined HAS_SONGS echo [W] songs folder is empty. Add chart folders before playing.

if not exist "config\config.json" (
  echo [W] config\config.json is missing. Runtime defaults will be used.
)
if not exist "%PROFILE_DIR%\config.json" (
  echo [I] %PROFILE_DIR%\config.json will be created on first launch.
)
if not exist "%PROFILE_DIR%\keymap.json" (
  echo [I] %PROFILE_DIR%\keymap.json will be created on first launch.
)

rem Locate executable (package root -> current build outputs)
if exist "TenRiff.exe" set "EXE=%CD%\TenRiff.exe"
if not defined EXE if exist "tenriff.exe" set "EXE=%CD%\tenriff.exe"
if not defined EXE if exist "build-dist\Release\TenRiff.exe" set "EXE=%CD%\build-dist\Release\TenRiff.exe"
if not defined EXE if exist "build-dist\Debug\TenRiff.exe" set "EXE=%CD%\build-dist\Debug\TenRiff.exe"
if not defined EXE if exist "build-dist\Release\tenriff.exe" set "EXE=%CD%\build-dist\Release\tenriff.exe"
if not defined EXE if exist "build-dist\Debug\tenriff.exe" set "EXE=%CD%\build-dist\Debug\tenriff.exe"
if not defined EXE if exist "build\Release\TenRiff.exe" set "EXE=%CD%\build\Release\TenRiff.exe"
if not defined EXE if exist "build\Debug\TenRiff.exe" set "EXE=%CD%\build\Debug\TenRiff.exe"
if not defined EXE if exist "build\Release\tenriff.exe" set "EXE=%CD%\build\Release\tenriff.exe"
if not defined EXE if exist "build\Debug\tenriff.exe" set "EXE=%CD%\build\Debug\tenriff.exe"
if not defined EXE if exist "build-win\Release\TenRiff.exe" set "EXE=%CD%\build-win\Release\TenRiff.exe"
if not defined EXE if exist "build-win\Debug\TenRiff.exe" set "EXE=%CD%\build-win\Debug\TenRiff.exe"
if not defined EXE if exist "build-win\Release\tenriff.exe" set "EXE=%CD%\build-win\Release\tenriff.exe"
if not defined EXE if exist "build-win\Debug\tenriff.exe" set "EXE=%CD%\build-win\Debug\tenriff.exe"

if not defined EXE (
  echo [W] TenRiff.exe was not found. Build the Windows target first.
  echo     Example: cmake -S . -B build-dist -G "Visual Studio 17 2022" -A x64
  echo         cmake --build build-dist --config Release --target tenriff
  popd >nul
  exit /b 1
)

if defined TENRIFF_DRY_RUN (
  echo [I] launch_win.bat dry-run
  echo [I] EXE: %EXE%
  echo [I] SONGS: %SONGS_ARG%
  echo [I] PROFILE: %PROFILE_NAME%
  echo [I] LOG: %LOG_FILE%
  popd >nul
  exit /b 0
)

"%EXE%" --songs "%SONGS_ARG%" --profile "%PROFILE_NAME%" %* > "%LOG_FILE%" 2>&1
set "ERR=%ERRORLEVEL%"
call :print_exit_hint %ERR%
if not "%ERR%"=="0" if exist "%LOG_FILE%" type "%LOG_FILE%"
popd >nul
exit /b %ERR%

:print_exit_hint
set "ERR_CODE=%~1"
if "%ERR_CODE%"=="0" (
  echo [OK] Exited normally.
  goto :eof
)
if "%ERR_CODE%"=="10" echo [E] Audio initialization failed. Check device access and drivers.
if "%ERR_CODE%"=="11" echo [E] Missing runtime resources. Check config, profiles, and songs paths.
if "%ERR_CODE%"=="12" echo [E] Fatal BMS parse error. Check the log file.
if "%ERR_CODE%"=="13" echo [E] Renderer initialization failed. Check the GPU driver or environment.
if %ERR_CODE% GEQ 14 echo [E] Unknown error code %ERR_CODE%. Attach logs\run.log for debugging.
goto :eof
