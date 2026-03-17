#!/usr/bin/env bash
set -euo pipefail

BASE="$(cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P)"
EXE="$BASE/TenRiff"
LOG_DIR="$BASE/logs"
CONFIG_DIR="$BASE/config"
SONG_DIR="$BASE/songs"

print_exit_hint() {
  case "$1" in
    0)   echo "[OK] 정상 종료." ;;
    10)  echo "[E] 오디오 초기화 실패 — 오디오 장치를 점유하는 프로그램을 닫거나 드라이버/패키지를 확인하세요." ;;
    11)  echo "[E] 자원 누락 — assets/profiles/songs 경로를 확인하세요." ;;
    12)  echo "[E] BMS 파싱 치명 오류 — 로그를 확인해 주세요." ;;
    13)  echo "[E] 현재 Linux/WSL 빌드는 프리뷰 상태입니다. GUI 메뉴/오디오/입력 백엔드가 아직 없습니다." ;;
    *)   echo "[E] 알 수 없는 오류 (코드 $1) — logs/를 첨부해 주세요." ;;
  esac
}

# 1) 폴더 점검
cd "$BASE"
mkdir -p "$CONFIG_DIR" "$LOG_DIR" "$SONG_DIR"
if [[ ! -d "$SONG_DIR" ]]; then
  echo "[W] songs 폴더가 없습니다. 폴더를 생성합니다. 곡을 넣어주세요."
  mkdir -p "$SONG_DIR"
fi
if [[ -z "$(find "$SONG_DIR" -mindepth 1 -print -quit)" ]]; then
  echo "[W] songs 폴더가 비었습니다. 곡 폴더를 넣어주세요."
fi

# 2) 기본 설정 생성
if [[ ! -f "$CONFIG_DIR/config.json" ]]; then
  cat > "$CONFIG_DIR/config.json" <<'JSON'
{
  "audio": {
    "rate": 44100,
    "frames": 256,
    "periods": 3,
    "exclusive": true,
    "use_mmcss": true,
    "affinity": -1,
    "preset": "high",
    "bms_keysound_policy": "follow",
    "volume": 1.0,
    "bgm_volume": 0.75,
    "keysound_volume": 1.0
  },
  "input": {
    "backend": "polling",
    "rawinput": true,
    "use_qpc": true,
    "grab": false,
    "queue_size": 2048,
    "polling_hz": 1000
  },
  "judge": {
    "pg": 15.5,
    "gr": 31.0,
    "gd": 75.0,
    "bd": 340.0,
    "hold_grace": 80,
    "hold_break": 200,
    "mask": 30
  },
  "speed": {
    "rate": 1.0,
    "hispeed": 3.0,
    "target_scroll_bps": 380
  },
  "gauge": {
    "auto_shift": true,
    "refill_normal": 15.911,
    "refill_easy": 23.866,
    "shift_cooldown_ms": 2000,
    "delta": {
      "hard": {"PG": 0.191, "GR": 0.127, "GD": 0.032, "BD": -1.414, "PR": -2.828},
      "normal": {"PG": 0.334, "GR": 0.223, "GD": 0.056, "BD": -1.081, "PR": -2.175},
      "easy": {"PG": 0.477, "GR": 0.318, "GD": 0.080, "BD": -0.754, "PR": -1.508}
    }
  },
  "graphics": {
    "display_mode": "borderless",
    "vsync": true,
    "fps_limit": 1250,
    "performance_overlay": false
  },
  "mode": {
    "format": "bms",
    "key_mode": "10k",
    "gauge": "normal",
    "random": "off",
    "random_seed": 0,
    "enable_osu_charts": false
  },
  "ui": {
    "result_tail_ms": 500,
    "require_enter_to_exit": true
  },
  "offsets": {
    "input": 0.0,
    "visual": 0.0
  }
}
JSON
  echo "[I] 기본 global config.json을 생성했습니다."
fi

if [[ ! -f "$EXE" ]]; then
  echo "[W] Linux 실행 파일(${EXE##*/})이 현재 패키지에 포함되어 있지 않습니다."
  echo "[I] 현재 TenRiff는 Windows GUI/D3D11/WASAPI 기반이며 Linux 전용 렌더러/오디오/입력 백엔드가 아직 없습니다."
  exit 13
fi

# 3) 실행
chmod +x "$EXE" 2>/dev/null || true
set +e
"$EXE" --songs "./songs" "$@" > "$LOG_DIR/run.log" 2>&1
ERR=$?
set -e

if [[ $ERR -ne 0 ]]; then
  print_exit_hint "$ERR"
  tail -n 200 "$LOG_DIR/run.log" 2>/dev/null || true
else
  echo "[OK] 정상 종료."
fi
exit $ERR
