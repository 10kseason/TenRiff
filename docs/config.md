# TenRiff Config Schema (current)

이 문서는 현재 `config/config.json`, `profiles/<name>/config.json`, `profiles/<name>/keymap.json` 기준으로 실제 동작하는 설정 구조를 정리합니다.

## Load Order
1. code defaults
2. global config: `config/config.json`
3. profile config: `profiles/<name>/config.json`
4. CLI
5. menu/runtime save

프로필이 없으면 첫 실행 시 자동 생성됩니다.

## `config.json`

### `audio`
- `rate` (int)
  - 기본 샘플레이트
- `frames` (int)
  - 버퍼 프레임
- `periods` (int)
  - period 수
- `exclusive` (bool)
  - WASAPI exclusive 시도 여부
- `use_mmcss` (bool)
- `affinity` (int)
  - `-1`이면 기본
- `preset` (string)
  - `basic | high`
- `bms_keysound_policy` (string)
  - `follow | autoplay | ignore`
- `volume` (double)
  - master volume
- `bgm_volume` (double)
- `keysound_volume` (double)

### `input`
- `backend` (string)
  - `polling | rawinput`
- `rawinput` (bool)
- `use_qpc` (bool)
- `grab` (bool)
  - 현재 Linux preview 성격
- `queue_size` (int)
- `polling_hz` (int)
  - `1000 | 2000 | 4000 | 8000`

### `judge`
- `pg`, `gr`, `gd`, `bd` (double, ms)
- 기본 `bd`는 `200ms`
- `indirect_miss` (double, ms)
  - 입력이 전혀 들어오지 않았을 때 노트를 자동 미스로 처리하는 간접 미스 기준
  - 기본값은 `500ms`
  - 직접 입력 판정 폭(`bd`)과는 별도로 동작하지만, 내부적으로는 항상 `bd` 이상으로 유지됨
- `hold_grace` (double, ms)
- `hold_break` (double, ms)
- `mask` (double, ms)

### `speed`
- `rate` (double)
- `hispeed` (double)
- `target_scroll_bps` (double)

### `gauge`
- `auto_shift` (bool)
- `hard_to_normal_threshold` (double)
  - Hard 게이지가 이 값 이하로 내려가면 즉시 Normal로 한 단계 시프트
- `normal_to_easy_threshold` (double)
  - Normal 게이지가 이 값 이하로 내려가면 즉시 Easy로 한 단계 시프트
- 한 judgement에서 최대 한 단계만 내려가고, 다시 위 단계로 복귀하지 않음
- `delta`
  - `hard`, `normal`, `easy`
  - 각 안에 `PG`, `GR`, `GD`, `BD`, `PR`

### `graphics`
- `display_mode` (string)
  - `borderless | windowed | fullscreen`
  - `windowed`는 제목줄이 있는 고정 크기 창이며 이동 가능
- `resolution` (string)
  - `native | 720p | 1080p | qhd`
- `vsync` (bool)
- `refresh_hz` (int)
  - `60..1050` 범위로 clamp
  - 기본값은 `1050`
  - `vsync=false`일 때만 직접적인 FPS cap 역할을 함
  - `vsync=false`면 menu는 effective cap `300`, gameplay는 configured target을 최대 `1050`까지 사용
  - `vsync=true`면 present refresh는 active monitor Hz를 따르고, render pacing은 `monitor_hz * 2`를 목표로 함 (`1050` clamp)
- `performance_overlay` (bool)

### `mode`
- `format` (string)
  - 기본은 차트 필터와 같이 씀
  - `bms | osu | auto`
  - `auto`는 실질적으로 `All`
- `key_mode` (string)
  - `auto | 4k | 5k | 6k | 7k | 8k | 9k | 10k`
- `gauge` (string)
  - `normal | hard | easy`
- `random` (string)
  - `off | fr | sr`
- `random_seed` (int)
- `enable_osu_charts` (bool)

### `ui`
- `result_tail_ms` (double)
- `require_enter_to_exit` (bool)
- `active_song_source` (string)
  - 마지막으로 연 곡 루트
- `recent_song_sources` (array of string)
  - 최근 외부/내부 song source 목록

### `skin`
- `note_shape` (string)
  - `rect | circle`
- `note_border_enabled` (bool)
- `judgement_line_position` (double)
  - gameplay 판정선의 세로 위치 비율
  - `0.55..0.86` 범위로 clamp
  - 기본값은 `0.82`
- `combo_position` (double)
  - gameplay 필드 내부 콤보 표시의 세로 위치 비율
  - `0.10..0.78` 범위로 clamp
  - 기본값은 `0.24`
- `note_width_scale` (double)
  - 노트 머리/꼬리 가로 배율
  - `0.50..1.40` 범위로 clamp
- `hold_body_width_scale` (double)
  - 롱노트 몸통 가로 배율
  - `0.50..1.20` 범위로 clamp
  - 실제 렌더 계산은 `max(4.0f, note_width * 0.5f * scale)` 기준
- `note_height_scale` (double)
  - 노트 머리/꼬리 세로 배율
  - `0.50..2.00` 범위로 clamp
- `lane_colors` (object)
  - key mode별 lane 색상 팔레트
  - 현재 기본/저장 대상 mode는 `4k..10k`, UI 편집 화면은 `5k..10k`
  - 각 mode 값은 lane 수만큼의 string array
  - 지원 토큰:
    `ice`, `azure`, `gold`, `mint`, `rose`, `violet`, `orange`, `teal`

### `offsets`
- `input` (double)
- `visual` (double)
  - `-500..500` 범위로 clamp

## `keymap.json`

### Shape
- `layout` (string)
- `bindings`
  - legacy 10K compatibility
- `modes`
  - `4k`, `5k`, `6k`, `7k`, `8k`, `9k`, `10k`
  - 각 mode 아래 lane id -> key token

### Notes
- old single-layout keymaps는 런타임에서 10K map으로 마이그레이션됩니다.
- runtime은 최종 차트 lane count 기준으로 해당 mode binding을 선택합니다.

## Runtime Migration Notes
- stale profile은 일부 값이 자동 교정됩니다.
- 특히 BMS-first default, osu key-mode mismatch, keysound policy 관련 값은 런타임 migration 대상입니다.
- config 파일이 없으면 defaults로 시작하고 즉시 profile이 저장됩니다.
