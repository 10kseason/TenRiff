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
- `background_sound_enabled` (bool)
  - 메뉴 BGM과 차트 배경음을 켜고 끔
- `volume` (double)
  - master volume
- `bgm_volume` (double)
- `keysound_volume` (double)

### `input`
- `backend` (string)
  - `polling | rawinput`
  - 현재 `1.1.2` 릴리스 라인 기본값은 `rawinput`
  - 저장값은 그대로 유지되며 자동으로 `polling`으로 강제 저장되지는 않음
  - gameplay 세션은 `rawinput=true`일 때 RawInput을 먼저 띄우고, bound-key polling shadow와 시작 실패 시 Polling fallback으로 입력 인식 끊김을 막음
- `rawinput` (bool)
  - `backend`와 함께 저장되는 편의 필드
  - 현재 `1.1.2` 릴리스 라인에서는 저장 시에도 현재 값이 유지됨
  - `true`이면 menu/gameplay 모두 RawInput을 우선 사용하되 입력 인식 유지를 위한 polling shadow/fallback을 허용
- `use_qpc` (bool)
- `grab` (bool)
  - 현재 Linux preview 성격
- `queue_size` (int)
- `polling_hz` (int)
  - `1000 | 2000 | 4000 | 8000`
  - 폴링 백엔드가 키보드 상태를 읽는 빈도
  - 기본값은 `1000` (`1ms`)
- `judgement_hz` (int)
  - `1000 | 2000 | 4000 | 8000`
  - 호환성용으로 남아 있는 입력 설정 필드
  - 현재 `1.1.2` 런타임에서는 별도 오디오 판정 서브루프를 이 값으로 구동하지 않음
  - 기본값은 `4000` (`0.25ms`)
- `debounce_ms` (double)
  - 같은 키에서 매우 짧게 튀는 up/down 채터링을 런타임 전에 걸러내는 입력 디바운스 시간
  - `0..25` 범위로 clamp
  - 기본값은 `8ms`

### `judge`
- `pg`, `gr`, `gd`, `bd` (double, ms)
- 기본 `gd`는 `75ms`
- 기본 `bd`는 `340ms`
- `indirect_miss` (double, ms)
  - 입력이 전혀 들어오지 않았을 때 노트를 자동 미스로 처리하는 간접 미스 기준
  - 현재 런타임에서는 저장값과 무관하게 항상 `bd`와 같은 값으로 접힘
- `hold_grace` (double, ms)
  - 롱노트 tail release를 `PG`로 보는 전용 허용창
  - 기본값은 `80ms`
- `hold_break` (double, ms)
  - 롱노트 tail release를 `GR`까지 허용하는 마지막 창
  - 이 범위를 벗어나면 `BD`
  - 내부적으로 항상 `hold_grace` 이상으로 유지됨
  - 기본값은 `200ms`
- `mask` (double, ms)

### `speed`
- `rate` (double)
- `hispeed` (double)
- `target_scroll_bps` (double)

### `gauge`
- 자동 gauge shift는 없습니다. 선택한 gauge 타입은 곡이 끝나거나 실패할 때까지 유지됩니다.
- Hard / Normal / Easy는 모두 `100%`에서 시작하고 `0%`가 되면 즉시 실패합니다.
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
  - 기본값은 `300`
  - `vsync=false`일 때만 직접적인 FPS cap 역할을 함
  - `vsync=false`면 menu는 effective cap `300`, gameplay render pacing은 `min(configured target, max(300, monitor_hz * 2))`로 safety clamp됨
  - `vsync=true`면 present refresh는 active monitor Hz를 따르고, render pacing은 `monitor_hz * 2`를 목표로 함 (`1050` clamp)
- `performance_overlay` (bool)

### `mode`
- `format` (string)
  - 기본은 차트 필터와 같이 씀
  - `bms | osu | auto`
  - `auto`는 실질적으로 `All`
- `key_mode` (string)
  - `none | auto | 4k | 5k | 6k | 7k | 8k | 9k | 10k | 16k`
  - `none`은 차트 원래 키 수를 그대로 사용
  - `10k` 변환은 standalone BMS key converter의 krrcream식 10K preset과 맞춰 `max_keys=10`, `min_keys=1`, `transform_speed_slot=5`, `seed=0`으로 적용
- `gauge` (string)
  - `normal | hard | easy`
- `random` (string)
  - `off | fr | sr`
- `random_seed` (int)
- `enable_osu_charts` (bool)
- `ghost_battle_enabled` (bool)
  - `true`면 선택한 차트의 최고 호환 replay를 자동 ghost 비교 대상으로 불러옴
  - `false`면 일반 플레이를 단일 필드로 유지
- `autoplay_enabled` (bool)
  - QA용 assist 모드
  - `true`면 판정 가능한 노트 입력을 자동으로 처리하고 결과에는 `ASSIST` clear status가 붙음
  - 기본 ghost/replay 비교 대상에서는 제외되는 쪽으로 사용됨
- `practice_no_fail_enabled` (bool)
  - QA용 assist 모드
  - `true`면 gauge 기반 조기 실패를 막고 차트 끝까지 판정/결과 저장을 유지함
  - 결과에는 `ASSIST` clear status가 붙음
- `song_index_profile` (string)
  - `safe | fast`
  - `safe`는 대형 라이브러리에서 RAM high-water를 우선 줄이는 기본값
  - `fast`는 32GB+ 환경에서 더 높은 worker/batch budget으로 재스캔 속도를 높이는 선택값

### `ui`
- `language` (string)
  - `en | ko`
  - 잘못된 값은 로드 시 `en`으로 정규화
  - Graphics Settings의 Language row와 연결됨
- `result_tail_ms` (double)
- `require_enter_to_exit` (bool)
- `active_song_source` (string)
  - 마지막으로 연 곡 루트
- `recent_song_sources` (array of string)
  - 최근 외부/내부 song source 목록

### `skin`
- `source` (string)
  - `native | osu | lr2`
- `osu_skin_name` (string)
  - imported osu!mania skin name
- `lr2_skin_name` (string)
  - imported LR2 playskin name
- `lr2_resolution_mode` (string)
  - `auto | sd | hd | fhd`
  - LR2 playskin의 해상도 override 토큰
  - `auto`는 asset 파일명 대신 LR2 playskin `#DST_NOTE` 레이아웃 좌표를 기준으로 SD/HD/FHD family를 판정
- `visual_preset` (string)
  - `classic | neon | minimal | tenriff`
  - Skins 메뉴에서 변경하면 아래 visual opacity/glow/key-label 옵션 묶음을 preset 값으로 즉시 재설정
- `note_shape` (string)
  - `rect | circle`
  - `rect`는 native renderer에서 둥근 박스형 note head/tail로 그려짐
- `note_border_enabled` (bool)
- `lane_background_opacity` (double)
  - lane별 반투명 배경 alpha
  - `0.00..0.45` 범위로 clamp
- `visual_opacity` (double)
  - note/receptor/key-label 계열 전체 opacity 배율
  - `0.20..1.00` 범위로 clamp
- `note_outline_opacity` (double)
  - native note thin outline alpha
  - `0.00..1.00` 범위로 clamp
- `hold_body_opacity` (double)
  - 롱노트 body alpha
  - `0.05..0.60` 범위로 clamp
- `judgement_line_glow_enabled` (bool)
  - 판정선 주변 glow 표시
- `key_pulse_enabled` (bool)
  - 입력 순간 판정선 근처에 짧은 lane pulse 표시
- `key_label_position` (string)
  - `bottom | top | off`
  - gameplay lane 안쪽에 현재 keymap의 키 이름을 작게 표시
- `judgement_line_position` (double)
  - gameplay 판정선의 세로 위치 비율
  - `0.55..0.86` 범위로 clamp
  - 기본값은 `0.82`
- `combo_position` (double)
  - gameplay 필드 내부 콤보 표시의 세로 위치 비율
  - `0.10..0.78` 범위로 clamp
  - 기본값은 `0.24`
- `lane_width_scales` (object)
  - key mode별 개별 lane 폭 배율 배열
  - 각 mode 값은 lane 수만큼의 number array
  - 각 값은 `0.50..1.75` 범위로 clamp
- `note_width_scale` (double)
  - 노트 머리/꼬리 가로 배율
  - `0.50..1.40` 범위로 clamp
- `lane_spacing_scales` (object)
  - key mode별 lane 사이 빈 간격 배율 배열
  - 각 mode 값은 `(lane_count - 1)` 길이의 number array
  - 각 값은 `0.00..2.00` 범위로 clamp
- `note_height_scale` (double)
  - 노트 머리/꼬리 세로 배율
  - `0.50..4.00` 범위로 clamp
- `lane_divider_width_scale` (double)
  - lane 사이 흰 separator 선의 공용 배율
  - `0.00..2.00` 범위로 clamp
  - 모든 key mode에 동일하게 적용됨
  - native skin은 기본 `1px` divider에 곱하고, osu/lr2 skin은 가져온 divider 폭이 있을 때 그 값에도 곱함
- `lane_center_gap_scale` (double)
  - 16K 필드의 좌우 블록 사이 중앙 간격 배율
  - `0.00..2.00` 범위로 clamp
  - 현재는 `16k` 레이아웃에서만 적용됨
- `hold_body_width_scale` (double)
  - 롱노트 몸통 가로 배율
  - `0.50..1.20` 범위로 clamp
  - 실제 렌더 계산은 `max(4.0f, note_width * 0.5f * scale)` 기준
- `note_width_scales` (object)
  - key mode별 `note_width_scale` override
- `note_height_scales` (object)
  - key mode별 `note_height_scale` override
- `lane_divider_width_scales` (object)
  - 레거시 호환용 필드
  - 현재 런타임은 공용 `lane_divider_width_scale`만 사용함
- `lane_center_gap_scales` (object)
  - key mode별 `lane_center_gap_scale` override
- `lane_colors` (object)
  - key mode별 lane 색상 팔레트
  - 현재 기본/저장 대상 mode는 `4k..10k`, `16k`
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
- key rebinding은 성공 즉시 `keymap.json`에 저장되고 별도의 최종 저장 단계를 요구하지 않습니다.
- Song Select에서 키맵 편집을 열면 현재 선택된 차트의 lane count를 우선 사용하고, 그 다음 `mode.key_mode`, 마지막으로 `10k`를 기본 편집 대상으로 삼습니다.

## Runtime Migration Notes
- stale profile은 일부 값이 자동 교정됩니다.
- 특히 BMS-first default, osu key-mode mismatch, keysound policy 관련 값은 런타임 migration 대상입니다.
- config 파일이 없으면 defaults로 시작하고 즉시 profile이 저장됩니다.
