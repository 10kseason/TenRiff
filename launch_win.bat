@echo off
setlocal enabledelayedexpansion

set BASE=%~dp0
set EXE=%BASE%TenRiff.exe
set PROFILE=%BASE%profiles\default
set LOGS=%BASE%logs
set SONGS=%BASE%songs

if not exist "%BASE%assets" (
  echo [E] assets 폴더가 없습니다.
  exit /b 11
)

if not exist "%PROFILE%" mkdir "%PROFILE%"
if not exist "%LOGS%" mkdir "%LOGS%"
if not exist "%SONGS%" (
  echo [W] songs 폴더가 없습니다. 폴더를 생성합니다. 곡을 넣어주세요.
  mkdir "%SONGS%"
)
for /f "delims=" %%i in ('dir /b "%SONGS%" 2^>nul') do set HAS_SONGS=1
if not defined HAS_SONGS echo [W] songs 폴더가 비었습니다. 곡 폴더를 넣어주세요.

if not exist "%PROFILE%\config.json" (
  echo {^"audio^":{^"rate^":48000,^"frames^":128,^"periods^":3,^"exclusive^":true},^"input^":{^"backend^":^"sdl2^"},^"speed^":{^"rate^":1.00,^"hispeed^":3.00},^"judge^":{^"pg^":15.5,^"gr^":31.0,^"gd^":55.0,^"bd^":80.0,^"hold_grace^":20,^"hold_break^":50,^"mask^":30},^"gauge^":{^"auto_shift^":true,^"refill_normal^":20.0,^"refill_easy^":30.0,^"shift_cooldown_ms^":2000,^"delta^":{^"hard^":{^"PG^":0.24,^"GR^":0.16,^"GD^":0.04,^"BD^":-1.125,^"PR^":-2.25},^"normal^":{^"PG^":0.42,^"GR^":0.28,^"GD^":0.07,^"BD^":-0.86,^"PR^":-1.73},^"easy^":{^"PG^":0.60,^"GR^":0.40,^"GD^":0.10,^"BD^":-0.60,^"PR^":-1.20}}},^"video^":{^"vsync^":false}} > "%PROFILE%\config.json"
  echo [I] 기본 config.json을 생성했습니다.
)

if not exist "%PROFILE%\keymap.json" (
  echo {^"bindings^":{^"lane1^":^"D^",^"lane2^":^"F^",^"lane3^":^"J^",^"lane4^":^"K^",^"lane5^":^"Space^",^"lane6^":^"H^",^"lane7^":^"U^",^"lane8^":^"I^",^"lane9^":^"O^",^"lane10^":^"L^"}} > "%PROFILE%\keymap.json"
  echo [I] 기본 keymap.json을 생성했습니다.
)

if not exist "%EXE%" (
  echo [W] TenRiff.exe가 없습니다. 빌드 후 다시 실행하세요.
  exit /b 1
)
if not exist "%BASE%SDL2.dll" echo [W] SDL2.dll 이 폴더에 없으면 실행이 실패할 수 있습니다.

"%EXE%" --songs ./songs --profile default %*
set ERR=%ERRORLEVEL%

if %ERR%==0 goto ok
if %ERR%==10 echo [E] 오디오 초기화 실패 — 오디오 장치를 점유하는 프로그램을 닫거나 드라이버/패키지를 확인하세요.
if %ERR%==11 echo [E] 자원 누락 — assets/profiles/songs 경로를 확인하세요.
if %ERR%==12 echo [E] BMS 파싱 치명 오류 — 로그를 확인해 주세요.
if %ERR%==13 echo [E] 렌더러 초기화 실패 — 그래픽 드라이버나 환경을 점검하세요.
if %ERR% GEQ 14 echo [E] 알 수 없는 오류 (코드 %ERR%) — logs/를 첨부해 주세요.

if exist "%LOGS%\run.log" type "%LOGS%\run.log"
exit /b %ERR%

:ok
echo [OK] 정상 종료.
exit /b 0
