# TenRiff Config Schema (current)

확인 기준: [Config.h](../src/config/Config.h), [Config.cpp](../src/config/Config.cpp), [기본 JSON](../config/config.json), [키맵](../src/config/Keymap.cpp). 생략된 값은 코드 기본값을 사용하며, 아래 범위는 로드·저장 시 정규화 규칙입니다.

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

- `backend` (string)
  - `wasapi | asio`; 기본 `wasapi`. 게임플레이와 선곡 미리듣기의 출력 백엔드. 메뉴·결과 배경음악은 별도 Windows MCI 경로를 유지.
- `asio_driver` (string)
  - 설치된 64비트 ASIO 드라이버의 CLSID. 빈 문자열은 Auto이며 이름순 첫 드라이버를 선택. Audio 화면에서 드라이버를 고르고 F5로 목록을 새로 읽음.
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
  - 메뉴, 결과, 곡 미리듣기 음악만 켜고 끔; 게임플레이 차트 BGM은 유지
- `volume` (double)
  - master volume
- `bgm_volume` (double)
- `normalize_audio` (bool)
  - 인게임 전체 스테레오 믹스의 RMS 음량 조절. limiter/master volume 전에 적용하며 기본값 false. 메뉴 음악·선곡 미리듣기는 그대로 유지.
- `keysound_volume` (double)

ASIO 설정은 [장치 설정 안내](asio-audio.md)를 참고하세요. 선택 샘플레이트를 고정하고 차트 오디오를 리샘플링합니다. `frames`는 요청값이며 드라이버 허용 크기로 협상됩니다. ASIO에서는 프리셋이 버퍼 크기를 덮어쓰지 않으며 `exclusive`·`periods`는 WASAPI 전용입니다. ASIO 오류 시 WASAPI로 자동 전환하지 않습니다.

### `input`

- `backend` (string)
  - `polling | rawinput`
  - 현재 `1.7.2` 릴리스 라인의 기본값은 `rawinput`
  - `Options -> Input Settings -> Backend` 또는 `Options -> Profile Setup -> Input Backend`에서 프로필별로 RawInput/Polling을 직접 선택 가능
  - 저장값은 런타임 fallback 때문에 자동으로 `polling`으로 덮어쓰지 않음
  - RawInput 시작 실패, 등록 대상 손실, 메시지 창 종료가 확인되면 현재 앱 실행 동안 메뉴와 다음 gameplay 세션 모두 Polling을 유지
  - 앱 재시작 또는 Input Settings의 명시적인 Backend 변경 시 선택한 백엔드를 다시 시도
- `rawinput` (bool)
  - `backend`와 함께 저장되는 편의 필드
  - `true`이면 menu/gameplay가 RawInput을 우선 사용
  - gameplay는 같은 `InputThread`에서 노트/control 키를 bound-key polling shadow로 항상 보조 감시
- `use_qpc` (bool)
- `grab` (bool)
  - 현재 Linux preview 성격
- `queue_size` (int)
- `polling_hz` (int)
  - `1000 | 2000 | 4000 | 8000`
  - Polling backend와 gameplay polling shadow가 키 상태를 읽는 빈도
  - 기본값은 `1000` (`1ms`)
- `judgement_hz` (int)
  - `1000 | 2000 | 4000 | 8000`
  - 호환성용으로 남아 있는 입력 설정 필드
  - 현재 `1.7.2` runtime은 별도 오디오 판정 서브루프를 이 값으로 구동하지 않음
  - 기본값은 `4000` (`0.25ms`)
- `debounce_ms` (double)
  - 실제 Press/Release 전환은 버리지 않고 같은 상태의 중복 이벤트만 상태 추적에서 제거
  - `0..25` 범위로 clamp
  - 기본값은 `8ms`
### `judge`
- `pg`, `gr`, `gd`, `bd` (double, ms)
- 기본 `pg / gr / gd`는 각각 `20ms / 65ms / 115ms`
- 기본 `bd`는 `210ms`
- `Judge Easy`는 기존 `1.25x` 배율로 `bd=262.5ms`, `Judge Hard`는 `bd=340ms`를 사용함; PG/GR/GD와 LN tail 창은 Hard에서 기본값 유지
- `indirect_miss` (double, ms)
  - 입력이 전혀 들어오지 않았을 때 노트를 자동 미스로 처리하는 간접 미스 기준
  - 시간 기준은 `bd`와 맞추며, `Judge Hard`에서는 미입력 노트를 BAD 대신 콤보 브레이크 간접 `POOR`/OD8 `MISS`로 기록
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
- 시각 스크롤은 누적 진행 시간이 가장 긴 대표 BPM을 기준으로 고정되며 이후 BPM 변속으로 초당 이동 속도가 보정되지 않음; STOP 대기는 대표 BPM 계산에서 제외하고 명시적 `#SCROLL`, 정지, 역주행은 유지. [계산 규칙](reference-bpm.md) 참고.

### `gauge`

Gauge Shift는 항상 적용됩니다. `mode.gauge`의 `ex_hard / hard / normal / easy`는 시작 등급이며 EX부터 Easy 순서입니다. 선택한 등급과 그 아래 등급을 각각 100%에서 병렬 계산하고, 현재 등급이 탈락하면 다음 생존 등급으로 이동합니다. 모든 대상 등급이 탈락해야 게이지 실패가 됩니다. 기존 `shift` 값은 EX 시작으로 해석합니다.

단위인정/Session Mix는 별도 LR2 참조 단위 게이지를 사용하며 일반 `gauge.delta` 설정을 적용하지 않습니다. 간접미스를 유지하며 수치·32% 보정·2% 실패·일반 게이지 비교는 [LR2 게이지 감사](lr2-gauge-audit.ko.md)를 참고하세요.

- `delta`: `ex_hard`, `hard`, `normal`, `easy` → `PG`, `GR`, `GD`, `BD`, `PR`.

### `graphics`
- `display_mode` (string)
  - `borderless | windowed | fullscreen`
  - 기본값은 `borderless`; Discord/OBS/Game Bar 같은 외부 오버레이에도 이 모드를 권장
  - `windowed`는 제목줄이 있는 고정 크기 창이며 이동 가능
  - `fullscreen`은 DXGI 독점 전체 화면이라 현재 Discord Game Overlay가 표시되지 않음
- `resolution` (string)
  - `native | 720p | 1080p | qhd`
- `vsync` (bool)
- `refresh_hz` (int)
  - `-1`은 `디스플레이에 맞춤`, `0`은 프로필 호환용 `무제한` 선택값(실제 최대 1500 FPS)
  - 고정 숫자 제한은 폐기되었으며 로드 시 `-1`로 이전
  - `vsync=false`의 무제한은 게임플레이를 1500 FPS로 제한하고 메뉴는 300 FPS cap 유지
  - `vsync=false`면 menu는 effective cap `300`; gameplay는 `-1`일 때 모니터 주사율을 따르고 `0`일 때 1500 FPS render pacing을 적용함
  - `vsync=true`면 present refresh는 active monitor Hz를 따르고, render pacing은 `monitor_hz * 2`를 목표로 함 (`1050` clamp)
- `performance_overlay` (bool)
  - 기본값은 `false`; 우상단을 사용하므로 Discord Voice 위젯을 같은 모서리에 두면 겹칠 수 있음
  - 인게임 frame pacing은 성공한 DXGI `Present()` 완료 시각 사이의 간격을 측정하며, HUD 업데이트 주기는 FPS 샘플로 사용하지 않음
- `bga_enabled` (bool)
  - 기본값은 `true`; `false`면 게임플레이의 이미지/영상 BGA와 관련 디코더·업스케일러 작업을 끔
  - Song Select 배경 미리보기는 별도 기능이므로 계속 표시됨
- `background_upscale_mode` (string)
  - `onnx | off`; 기존 `lunasr` 값은 호환을 위해 `onnx`로 마이그레이션
  - 기본값은 `off`; Graphics Settings의 `BGA Upscaler`에서 ON/OFF를 직접 바꿈
  - ON으로 바꿀 때 고사양 기능 경고를 확인해야 하며 자동 성능 벤치마크는 실행하지 않음
- `background_upscale_model_path` (string)
  - Graphics Settings의 `ONNX Model`에서 파일을 선택하거나 해당 화면에 `.onnx`를 드롭하면 경로만 저장되며 업스케일러를 자동으로 켜지는 않음
  - 절대 경로 또는 실행 파일/현재 작업 폴더 기준 상대 경로를 허용하며 공개 패키지에는 모델을 포함하지 않음
  - 현재 계약은 float32 또는 float16 NCHW `rgb_lr [1,3,540,960]` -> `rgb_residual_x2 [1,3,1080,1920]` residual x2; 외부 경계를 float로 유지하는 INT8 QDQ 모델은 내부 양자화를 감지해 지원
  - 모델 로드, 입출력 계약 또는 추론 실패 시 native scaling 유지
  - 사용자 모델의 권리·품질·성능은 사용자가 확인해야 하며 상세 계약은 `tools/onnx_upscaler/README.md`
- `background_upscale_prefer_npu` (bool)
  - 기본값은 `false`이며 기본 경로는 high-performance DirectX GPU를 요청함
  - Graphics Settings의 `저전력 DirectX(실험)`에서 `DirectXMinPower` 요청을 켬
  - 레거시 WinML 경로는 NPU를 명시 선택하거나 검증하지 못하므로 이 옵션을 NPU 성공 근거로 사용하면 안 됨
  - 저전력 session 생성에 실패하면 기존 high-performance DirectX 경로로 폴백

### `mode`
차트 로더와 인덱서는 BMS 계열(`.bms/.bme/.bml/.pms`) 전용입니다. 예전 `enable_osu_charts`와 `format` 값은 읽더라도 무시하며 다시 저장하지 않습니다.

- `key_mode` (string)
  - `none | auto | 4k | 5k | 6k | 7k | 8k | 9k | 10k | 12k | 14k | 16k`
  - `none`은 차트 원래 키 수를 그대로 사용
  - `10k` 변환은 standalone BMS key converter의 krrcream식 10K preset과 맞춰 `max_keys=10`, `min_keys=1`, `transform_speed_slot=5`, `seed=0`으로 적용
- `key_conversion_algorithm` (string)
  - `krrcream | nk2 | nk3`
  - 게임 내 `Mode Settings > Key Converter`에서 `Krrcream`, `KeyWeaver nK2`, `KeyWeaver NK3 ONNX` 선택
  - 기본값은 `krrcream`; NK3는 같은 키 수에서도 리마스터를 실행하고 기본 `AUTO` 백엔드는 ncnn Vulkan을 우선 사용
  - Krrcream은 원본 노트만 목표 레인으로 재배치
  - nK2는 키 수 확장 시 원본에 먼저 노트를 붙이지 않고, 변환 중 목표 레이아웃에 안전한 보조 노트를 직접 생성
  - NK3는 P64와 host beam 안전 솔버를 항상 사용하고, 10K가 아닌 원본을 10K로 변환할 때만 일반화 MLP를 추가한다. `TENRIFF_NK3_BACKEND=AUTO|VULKAN|NCNN_CPU|OPENVINO`로 백엔드를 고르며 `AUTO`는 ncnn Vulkan을 먼저 시도한다. 여러 Vulkan GPU가 있으면 `TENRIFF_NK3_VULKAN_DEVICE=<index>`로 선택
- `key_conversion_nk2_preset` (string)
  - `native | transform | remaster`; 기본값은 `native`
  - nK2에서 `Native (12%)`, `Transform (35%)`, `Remaster (65%)`를 선택하며, Krrcream에서는 설정 행이 잠김
  - `Remaster`는 예산을 올리면서도 원곡 배치를 유지하고, 롱노트 구간의 보조 노트를 같은 길이의 롱노트로 채움
  - 세 값은 모두 상한이며, 실제 추가량은 원본 밀도와 안전창에 따라 더 낮게 나옴
- `gauge` (string)
  - `normal | hard | ex_hard | easy | shift`
- `random` (string)
  - `off | mirror | rr | frns | sr` (`fr` = `frns`)
- `random_seed` (int)
  - RR/SR, 강제 key-mode 변환, LN Mix 대상 선택의 고정 seed이며 Mirror 레인 반전 자체는 사용하지 않음. 일반 Random은 플레이마다 새 session seed를 만들고 replay에 실제 값을 기록
- `mods` (string array)
  - Note Structure에서 `full_long_notes`, `ln_mix_10`~`ln_mix_90`, `full_short_notes` 중 하나를 선택 가능
  - LN Mix는 base BPM 기준 8비트 LN도 다음 동일 레인 노트보다 50ms 먼저 끝낼 수 있는 단노트만 후보로 삼고, 요청 비율만큼 선택한 LN 길이를 긴 8비트 60% / 중간 16비트 20% / 짧은 24·32비트 20%로 배분
  - 기존 롱노트는 보존하고 같은 레인의 기존 span과 겹치는 head는 제외하며, 같은 `random_seed`에서는 같은 단노트가 변환됨
- `ghost_battle_enabled` (bool)
  - 기본값은 `false`
  - `true`면 선택한 차트의 최고 호환 replay를 자동 ghost 비교 대상으로 불러옴
  - `false`면 일반 플레이를 단일 필드로 유지
- `autoplay_enabled` (bool)
  - QA용 비경쟁 자동 플레이 모드
  - `true`면 판정 가능한 노트 입력을 자동으로 처리하고 결과를 `AUTOPLAY`로 저장함
  - 공식 클리어, 최고 점수, 클리어 램프, 기본 ghost 비교 대상에서는 제외되며 로컬 기록/리플레이는 남음
- `practice_no_fail_enabled` (bool)
  - QA용 assist 모드
  - `true`면 gauge 기반 조기 실패를 막고 차트 끝까지 판정/결과 저장을 유지함
  - 결과에는 `ASSIST` clear status가 붙음
- `one_miss_fail_enabled` (bool)
  - `true`면 첫 OD8 환산 객체 `MISS`에서 게이지가 0이 되고 즉시 실패함
  - 네이티브 `BAD`만으로는 즉사하지 않으며 빈 키 입력의 `POOR`도 즉사 조건에 포함하지 않음
  - Mode Settings에서 활성화하면 `practice_no_fail_enabled`가 자동으로 꺼짐
- `pacemaker_mode` (string)
  - `off | accuracy | score`; 기본값은 `off`
  - `accuracy` 또는 `score`이면 게이지로 조기 종료하지 않고 차트 끝의 선택 목표 달성 여부로 CLEAR/FAILED를 결정
  - Pacemaker를 켜면 Practice와 Sudden Death는 꺼지며, 리플레이 재생과 멀티플레이에서는 적용하지 않음
- `pacemaker_target_accuracy` (double)
  - `0..100`, 기본값 `90.0`; Result의 표준 Accuracy가 이 값 이상이면 clear
- `pacemaker_target_score` (int)
  - `0..10000`, 기본값 `8000`; 배율 적용 뒤 Result에 표시되는 최종 Score가 이 값 이상이면 clear
- `song_index_profile` (string)
  - `safe | fast`
  - `safe`는 대형 라이브러리에서 RAM high-water를 우선 줄이는 기본값
  - `fast`는 곡 목록용 최소 메타데이터만 읽고 파일 해시, 미리보기, 난이도표, 자체 LV/CR을 생략하는 선택값
- `calculate_song_index_difficulty` (bool)
  - 기본값은 `false`
  - `false`면 BMS `#PLAYLEVEL`을 메뉴 LV로 유지하고 CPU 비용이 큰 자체 LV/CR 계산을 건너뜀
  - `true`면 `safe` 전체 인덱싱 중 Revive LV/Circus Rating을 계산하며 `fast`에서는 항상 생략
  - 설정을 바꾸면 캐시 계산 모드를 구분해 현재 song source를 전체 재인덱싱함

### `ui`

| 항목 | 형식·범위·기본값 | 동작 |
| --- | --- | --- |
| `profile_nickname` | string; UTF-8 ≤48 bytes | 표시 이름. 비면 프로필 ID를 사용하며 공백·제어문자를 정리. |
| `profile_avatar_path` | string; UTF-8 ≤2048 bytes | 로컬 PNG/JPG 아바타 경로. |
| `language` | `en`, `ko`; `en` | UI 언어. 다른 값은 en으로 정규화. |
| `result_tail_ms` | double; `500` ms | 판정 완료 후 결과 전환 시 추가 대기 시간. 차트 오디오 종료 시점도 함께 고려합니다. |
| `require_enter_to_exit` | bool; `true` | 읽기·저장 호환 필드. 현재 Windows 결과 입력 경로는 이 값으로 자동 종료하지 않습니다. |
| `show_cursor_in_gameplay` | bool; `true` | 인게임 마우스 포인터 표시. |
| `active_song_source`, `recent_song_sources` | string / string[] | 현재 곡 폴더와 최근 곡 폴더 목록. |
| `song_sources_initialized` | bool; `false` | 소스를 명시적으로 추가·제거한 뒤 true. 마지막 소스를 지운 빈 목록을 재실행 때 자동 복원하지 않음. [곡 소스 관리](library-management.md) 참고. |
| `session_mix_lr2_course_path` | string | 선택한 LR2 코스 파일 경로. |
| `favorite_chart_keys` | string[] | 즐겨찾기 차트의 내부 식별 키. |
| `collections` | object: name → string[] | 이름 있는 컬렉션별 차트 키 목록. |
| `song_collection_filter` | string; `all` | 전체·즐겨찾기·컬렉션 필터. 변경 즉시 저장. |
| `song_key_filter` | int: `0..16`; `0` | 키 수 필터. 0은 전체. UI 선택은 4K–10K, 12K, 14K, 16K. |
| `song_level_min_filter`, `song_level_max_filter` | int: `0..50`; `0` | 난이도 경계. 0이면 해당 경계를 사용하지 않음. |
| `difficulty_table_path` | string | 로컬 header JSON 또는 다운로드한 프로필 캐시 header. 표 선택 시 safe 인덱스로 전환. 기본 LV 선택 시 비우고 정상 캐시에서 외부 표 정보만 제거. |
| `difficulty_table_url` | string | 원본 HTTP(S) BMSTable 페이지/header URL. bmstable meta를 해석해 header/data를 캐시하고 로컬 JSON 선택 시 비움. |
| `online_records_server_url` | string | 기록·랭킹·채팅 API URL. 실패해도 로컬 플레이·기록은 유지. |
| `tenriff_main_server_url` | string | F10 메인 서버 API URL. 기본 주소는 Config.h의 kTenRiffMainApiUrl. |
| `private_server_url` | string | F10 사설 API URL. 원격 HTTPS, localhost만 HTTP 허용. |
| `account_server_mode` | `main`, `private`; `main` | 마지막 로그인 서버 선택. |

난이도표 header는 `name`, `symbol`, 로컬 상대경로 `data_url`을 사용하며 data 항목은 `md5` 또는 `sha256`과 `level`로 매칭합니다. 원격 가져오기는 프로필의 `difficulty_tables` 캐시에 저장합니다.

선택 창의 `F4 기본 LV`는 `difficulty_table_path`와 `difficulty_table_url`을 함께 비웁니다. 기본 LV는 `mode.calculate_song_index_difficulty=false`일 때 BMS `#PLAYLEVEL`, 계산을 켜 둔 캐시에서는 기존 자체 계산 LV입니다. [곡 소스와 표 선택](library-management.md) 참고.

### `skin`

현재 `skin` 설정과 활성 스킨 자산은 [휴대용 `.trskin` 프리셋](skin-presets.md)으로 내보내고 가져올 수 있습니다. 오디오 장치·키맵·계정·곡 소스와 타이밍 보정은 포함하지 않습니다.

이 표는 프로필 `config.json`의 `skin` 설정입니다. 스킨 패키지의 `skin.json` 계약은 [스킨 형식](skin-format.md)을 참고하세요.

| 항목 | 형식·범위·기본값 | 동작 |
| --- | --- | --- |
| `source` | string: `native`, `tenriff`, `lr2` | 스킨 소스. |
| `tenriff_skin_name`, `lr2_skin_name` | string | 가져온 스킨 폴더 이름. |
| `scratch_position` | `left`, `right`; `left` | 7+1 스크래치 표시 순서만 변경. 입력·판정 레인은 유지. |
| `lr2_resolution_mode` | `auto`, `sd`, `hd`, `fhd`; `auto` | LR2 좌표 기준 해상도 해석. auto는 파일명 대신 `#DST_NOTE` 좌표를 사용. |
| `visual_preset` | `classic`, `neon`, `minimal`, `tenriff`; `tenriff` | 메뉴에서 선택하면 시각 옵션 묶음을 재설정. |
| `note_shape` | `rect`, `triangle`, `pentagon`, `hexagon`, `circle`; `rect` | 기본 도형 노트 모양. |
| `note_image_aspect` | `stretch`, `contain`, `width`; `stretch` | 늘이기 / 비율 유지해 안에 맞추기 / 폭을 고정하고 높이를 비율로 계산. |
| `preserve_note_image_aspect_ratio` | bool; `false` | 구버전 호환 필드. 명시된 `note_image_aspect`가 우선하며 저장 시 stretch 이외는 true. |
| `note_border_enabled`, `show_lane_dividers`, `show_judgement_line` | bool; `true` | 각각 노트 테두리, 레인 구분선, 판정선 표시. |
| `note_divider_gap_px` | double: `0..40`; `12` px | 노트 한쪽 가장자리와 구분선 사이 여백. 0이면 구분선까지 확장. |
| `show_gear_boundary_line` | bool; `false` | 기어 경계선 표시. |
| `show_timing_feedback` | bool; `true` | FAST/SLOW 문구와 타이밍 기록 표시. 판정 등급은 별도로 유지. |
| `show_hold_tail`, `hold_tail_taper_enabled` | bool; `false` | 각각 LN 꼬리 캡 표시, 꼬리 테이퍼. 판정 규칙은 변경하지 않음. |
| `judgement_line_glow_enabled` | bool; `true` | 판정선 주변 빛 표시. |
| `key_pulse_brightness` | double: `0..1`; `1` | Hit Burst 밝기. 0이면 끔. |
| `key_pulse_enabled` | bool; `true` | 구버전 ON/OFF 호환. false 또는 밝기 0이면 끔. |
| `hit_burst_style` | `prism`, `ring`, `spark`; `prism` | 내장 Hit Burst 모양. |
| `ui_font` | `default`, `malgun`, `bahnschrift`, `consolas`; `default` | 메뉴 글꼴. default는 Segoe UI; 로고·랭크·콤보 전용 글꼴은 유지. |
| `key_label_position` | `bottom`, `top`, `off`; `bottom` | 레인 키 이름 위치. |
| `judgement_line_position` | double: `0..1`; `0.82` | 판정선 세로 위치 비율. |
| `gameplay_field_offset_x` | double: `-720..720`; `0` | 1920×1080 기준 기어 가로 이동. 실제 창에서 기어와 ↔ 핸들이 보이도록 추가 제한. |
| `combo_position`, `judgement_position` | double: `0.10..0.78`; `0.24` | 콤보와 판정의 독립 Y 위치. 판정 값이 없는 기존 프로필은 combo_position을 상속. |
| `combo_offset_x`, `judgement_offset_x` | double: `-600..600`; `0` | 1920×1080 기준 콤보와 판정의 독립 X 오프셋. |
| `lane_background_opacity` | double: `0..0.45`; `0.18` | 레인 배경 불투명도. |
| `black_playfield_enabled` | bool; `true` | 레인 간격을 포함한 필드 전체를 검정으로 표시. |
| `visual_opacity` | double: `0.20..1`; `0.96` | 노트·리셉터·키 라벨의 공통 불투명도 배율. |
| `note_outline_opacity` | double: `0..1`; `0.78` | 노트 외곽선 불투명도. |
| `hold_body_opacity` | double: `0.05..1`; `1` | LN 몸통 불투명도. |
| `lane_width_scales` | object: mode → number[]; `0.50..1.75` | 레인 수 길이의 개별 폭 배열. |
| `note_width_scale` | double: `0.50..1.40`; `1` | Note & Field Size. 중심을 유지하며 필드·레인·노트·인접 게이지를 함께 조절. |
| `lane_spacing_scales` | object: mode → number[]; `0..2` | 레인 사이 간격 배열. 길이는 lane_count - 1. |
| `note_height_scale` | double: `0.50..4`; `1.8` | 노트 머리·꼬리 높이 배율. |
| `lane_divider_width_scale` | double: `0..2`; `1` | 모든 키 모드 공용 구분선 폭. 가져온 LR2 구분선 폭에도 적용. |
| `lane_center_gap_scale` | double: `0..2`; `0` | 16K 좌우 블록의 중앙 간격. |
| `hold_body_width_scale` | double: `0.50..1.20`; `1` | LN 몸통 폭 배율. |
| `note_width_scales`, `note_height_scales`, `lane_center_gap_scales` | object: mode → number | 해당 공용 값의 키 모드별 override. 같은 범위로 제한. |
| `lane_divider_width_scales` | object: mode → number | 레거시 호환 필드. 현재는 공용 lane_divider_width_scale 사용. |
| `lane_colors` | object: mode → string[] | 레인 수 길이의 색상 토큰 배열. |
| `single_color` | string; `off` | 색상 토큰을 선택하면 전체 레인에 적용. 기존 lane_colors는 보존. |

모드별 배열·override 지원: `4k`, `5k`, `6k`, `7k`, `7+1`, `8k`, `9k`, `10k`, `12k`, `14k`, `16k`. 색상 토큰: `ice`, `azure`, `gold`, `mint`, `rose`, `violet`, `orange`, `teal`. `7+1`은 스킨 팔레트 전용이며 별도 키맵 모드가 아닙니다. 구버전 `expand_notes_to_dividers=true`는 여백 0으로 읽으며, 명시된 `note_divider_gap_px`가 우선합니다.

### `offsets`
- `input` (double)
- `visual` (double)
  - `-500..500` 범위로 clamp
  - 설정 키와 동작은 유지하며 UI에서는 `Skins > Visual Latency`로 표시
- `sound` (double, ms)
  - `-500..500` 범위로 clamp하며 UI에서는 `Audio Settings > Sound Offset`과 `Calibration Wizard`에 표시
  - 양수는 차트 BGM과 자동재생 키음을 늦추고 음수는 앞당김; 판정, 노트/BGA 위치, 입력에 맞춰 재생되는 `follow` 키음은 변경하지 않음

## `keymap.json`

### Shape
- `layout` (string)
- `bindings`
  - legacy 10K compatibility
- `modes`
  - `4k`, `5k`, `6k`, `7k`, `8k`, `9k`, `10k`, `12k`, `14k`, `16k`
  - 각 mode 아래 lane id -> key token

### Notes
- old single-layout keymaps는 런타임에서 10K map으로 마이그레이션됩니다.
- runtime은 최종 차트 lane count 기준으로 해당 mode binding을 선택합니다.
- key rebinding은 성공 즉시 `keymap.json`에 저장되고 별도의 최종 저장 단계를 요구하지 않습니다.
- Song Select에서 키맵 편집을 열면 현재 선택된 차트의 lane count를 우선 사용하고, 그 다음 `mode.key_mode`, 마지막으로 `10k`를 기본 편집 대상으로 삼습니다.

## Runtime Migration Notes
- stale profile은 일부 값이 자동 교정됩니다.
- 특히 BMS 기본값과 keysound policy 관련 값은 런타임 migration 대상이며, 예전 osu chart/skin 필드는 더 이상 저장되지 않습니다.
- config 파일이 없으면 defaults로 시작하고 즉시 profile이 저장됩니다.
