#!/usr/bin/env bash
set -euo pipefail

BASE="$(cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P)"
EXE="$BASE/TenRiff"
LOG_DIR="$BASE/logs"
PROFILE_DIR="$BASE/profiles/default"
SONG_DIR="$BASE/songs"

print_exit_hint() {
  case "$1" in
    0)   echo "[OK] 정상 종료." ;;
    10)  echo "[E] 오디오 초기화 실패 — 오디오 장치를 점유하는 프로그램을 닫거나 드라이버/패키지를 확인하세요." ;;
    11)  echo "[E] 자원 누락 — assets/profiles/songs 경로를 확인하세요." ;;
    12)  echo "[E] BMS 파싱 치명 오류 — 로그를 확인해 주세요." ;;
    13)  echo "[E] 렌더러 초기화 실패 — 그래픽 드라이버나 환경을 점검하세요." ;;
    *)   echo "[E] 알 수 없는 오류 (코드 $1) — logs/를 첨부해 주세요." ;;
  esac
}

check_sdl2() {
  if [[ ! -x "$EXE" ]]; then
    echo "[W] 실행 파일(${EXE##*/})이 아직 없습니다. 빌드 후 다시 시도하세요."
    return
  fi
  if command -v ldd >/dev/null 2>&1; then
    if ! ldd "$EXE" | grep -qi "SDL2"; then
      echo "[W] SDL2 런타임이 감지되지 않았습니다. 배포 패키지를 설치하거나 LD_LIBRARY_PATH를 확인하세요."
    fi
  fi
}

# 1) 폴더/필수 자원 점검
cd "$BASE"
[[ -d assets ]] || { echo "[E] assets 폴더 없음"; exit 11; }
mkdir -p "$PROFILE_DIR" "$LOG_DIR"
if [[ ! -d "$SONG_DIR" ]]; then
  echo "[W] songs 폴더가 없습니다. 폴더를 생성합니다. 곡을 넣어주세요."
  mkdir -p "$SONG_DIR"
fi
if [[ -z "$(find "$SONG_DIR" -mindepth 1 -print -quit)" ]]; then
  echo "[W] songs 폴더가 비었습니다. 곡 폴더를 넣어주세요."
fi

# 2) 기본 설정/키맵 생성
if [[ ! -f "$PROFILE_DIR/config.json" ]]; then
  cat > "$PROFILE_DIR/config.json" <<'JSON'
{ "audio":{"rate":48000,"frames":128,"periods":3,"exclusive":true},
  "input":{"backend":"sdl2"},
  "speed":{"rate":1.00,"hispeed":3.00},
  "judge":{"pg":15.5,"gr":31.0,"gd":55.0,"bd":80.0,"hold_grace":20,"hold_break":50,"mask":30},
  "gauge":{"auto_shift":true,"refill_normal":20.0,"refill_easy":30.0,"shift_cooldown_ms":2000,
    "delta":{"hard":{"PG":0.24,"GR":0.16,"GD":0.04,"BD":-1.125,"PR":-2.25},
             "normal":{"PG":0.42,"GR":0.28,"GD":0.07,"BD":-0.86,"PR":-1.73},
             "easy":{"PG":0.60,"GR":0.40,"GD":0.10,"BD":-0.60,"PR":-1.20}} },
  "video":{"vsync":false} }
JSON
  echo "[I] 기본 config.json을 생성했습니다."
fi

if [[ ! -f "$PROFILE_DIR/keymap.json" ]]; then
  cat > "$PROFILE_DIR/keymap.json" <<'JSON'
{ "bindings":{
  "lane1":"D","lane2":"F","lane3":"J","lane4":"K","lane5":"Space",
  "lane6":"H","lane7":"U","lane8":"I","lane9":"O","lane10":"L"} }
JSON
  echo "[I] 기본 keymap.json을 생성했습니다."
fi

check_sdl2

# 3) 실행
chmod +x "$EXE" 2>/dev/null || true
"$EXE" --songs "./songs" --profile default "$@"
ERR=$?

if [[ $ERR -ne 0 ]]; then
  print_exit_hint "$ERR"
  tail -n 200 "$LOG_DIR/run.log" 2>/dev/null || true
else
  echo "[OK] 정상 종료."
fi
exit $ERR
