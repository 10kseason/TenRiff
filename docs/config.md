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
  - 메뉴, 결과, 곡 미리듣기 음악만 켜고 끔; 게임플레이 차트 BGM은 유지
- `volume` (double)
  - master volume
- `bgm_volume` (double)
- `keysound_volume` (double)

### `input`

- `backend` (string)
  - `polling | rawinput`
  - 현재 `1.5.1` 릴리스 라인의 기본값은 `rawinput`
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
  - 현재 `1.5.1` runtime은 별도 오디오 판정 서브루프를 이 값으로 구동하지 않음
  - 기본값은 `4000` (`0.25ms`)
- `debounce_ms` (double)
  - 실제 Press/Release 전환은 버리지 않고 같은 상태의 중복 이벤트만 상태 추적에서 제거
  - `0..25` 범위로 clamp
  - 기본값은 `8ms`
### `judge`
- `pg`, `gr`, `gd`, `bd` (double, ms)
- 기본 `pg / gr / gd`는 각각 `20ms / 45ms / 90ms`
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
- 시각 스크롤은 곡 시작 BPM에 고정되며 이후 BPM 변속으로 초당 이동 속도가 보정되지 않음; 명시적 `#SCROLL`, 정지, 역주행은 유지

### `gauge`
- `normal | hard | ex_hard | easy`는 항상 적용되는 Gauge Shift의 시작 등급입니다. 화면에서는 `ex_hard`를 `EX`로 표시합니다.
- 선택한 시작 등급부터 Easy까지를 각각 100%에서 동시에 계산합니다. 현재 게이지가 0%로 탈락하면 이미 같은 판정을 누적한 바로 아래 생존 게이지를 선택하며, 종료 시 살아남은 가장 높은 게이지가 최종 게이지가 됩니다.
- 기존 `shift` 값은 호환을 위해 EX 시작으로 해석합니다.
- `delta`
  - `ex_hard`, `hard`, `normal`, `easy`
  - 각 안에 `PG`, `GR`, `GD`, `BD`, `PR`

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
  - NK3는 P64와 host beam 안전 솔버를 항상 사용하고, 10K가 아닌 원본을 10K로 변환할 때만 일반화 MLP를 추가한다. `TENRIFF_NK3_BACKEND=AUTO|VULKAN|OPENVINO`로 백엔드를 고르며 `AUTO`는 ncnn Vulkan을 먼저 시도한다. 여러 Vulkan GPU가 있으면 `TENRIFF_NK3_VULKAN_DEVICE=<index>`로 선택
- `key_conversion_nk2_preset` (string)
  - `native | transform | remaster`; 기본값은 `native`
  - nK2에서 `Native (12%)`, `Transform (35%)`, `Remaster (65%)`를 선택하며, Krrcream에서는 설정 행이 잠김
  - `Remaster`는 예산을 올리면서도 원곡 배치를 유지하고, 롱노트 구간의 보조 노트를 같은 길이의 롱노트로 채움
  - 세 값은 모두 상한이며, 실제 추가량은 원본 밀도와 안전창에 따라 더 낮게 나옴
- `gauge` (string)
  - `normal | hard | ex_hard | easy | shift`
- `random` (string)
  - `off | mirror | fr | sr`
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
- `profile_nickname` (string)
  - Quick Setup에서 편집하며 저장 기록과 멀티플레이 표시 이름으로 사용
  - 제어문자/중복 공백을 정리하고 UTF-8 기준 최대 48바이트로 제한; 비어 있으면 프로필 ID를 표시
- `profile_avatar_path` (string)
  - Profile Setup에서 선택한 로컬 PNG/JPG 경로; 비어 있으면 TenRiff 기본 표시 사용
  - 프로필별로 저장하며 UI 안전 문자열과 UTF-8 최대 2048바이트로 정규화
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
- `song_collection_filter` (string)
  - 마지막으로 선택한 전체/즐겨찾기/사용자 컬렉션 필터
- `song_key_filter` (int, `0..16`)
  - 곡 브라우저의 마지막 키 수 필터. `0`은 전체 키
- `song_level_min_filter`, `song_level_max_filter` (int, `0..50`)
  - 마지막 레벨 범위 필터. `0`은 해당 경계를 사용하지 않음
  - 키 수·레벨·컬렉션 필터는 변경 즉시 프로필 설정에 저장되어 재실행 후에도 유지됨
- `difficulty_table_path` (string)
  - Browse 화면에서 고른 로컬 BMS 난이도표 header JSON 또는 링크에서 내려받은 프로필 캐시 header 경로
  - header는 `name`, `symbol`, 로컬 상대경로 `data_url`을 사용하고, data array entry는 `md5` 또는 `sha256`과 `level`을 사용
  - 선택/해제 시 현재 song source를 재인덱싱해 일치 곡의 표 레벨을 표시하며, 표 선택 시 해시가 필요한 `safe` 인덱스로 자동 전환
- `difficulty_table_url` (string)
  - Browse에서 가져온 http(s) BMSTable HTML 페이지 또는 header JSON 원본 링크
  - 표준 `<meta name="bmstable" content="...">`를 해석해 header/data JSON을 프로필의 `difficulty_tables` 캐시에 저장하며, 로컬 JSON 선택 시에는 비워짐
- `online_records_server_url` (string)
  - 현재 로그인한 서버의 기록·랭킹·글로벌 채팅 API 기준 URL
  - TenRiff 메인 기본값은 `https://121.174.18.181:27303`; 로컬 사설 서버는 `http://127.0.0.1:27302` 사용 가능
  - 서버 오류나 버전 불일치는 로컬 기록/플레이를 막지 않으며 Online 탭만 fail-closed로 오류를 표시
  - 곡 브라우저 설정에서 URL을 복사한 뒤 Enter 또는 편집 중 Ctrl+V로 지정 가능
- `tenriff_main_server_url` (string)
  - F10의 `TenRiff 메인` 선택에 사용되는 고정 메인 API 주소
  - F10 로그인 창의 `텐리프 메인` 서버 URL. 운영 배포에서는 유효한 HTTPS 주소가 필요함
- `private_server_url` (string)
  - F10 로그인 창에서 사용자가 입력한 사설 API URL. 원격 주소는 HTTPS만 허용하고 localhost만 HTTP 허용
- `account_server_mode` (string)
  - 마지막 로그인 서버 선택: `main | private`

### `skin`
- `source` (string)
  - `native | tenriff | lr2`
- `tenriff_skin_name` (string)
  - 가져온 TenRiff `skin.json` 스킨 폴더 이름
- `scratch_position` (string)
  - BMS `7+1` 레이아웃의 스크래치 표시 위치: `left | right`
  - 입력·판정·리플레이 레인 번호는 유지하고 화면 표시 순서만 바꿈
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
  - `rect | triangle | pentagon | hexagon | circle`
  - 100% 기준에서 procedural 원·다각형은 rect 막대와 같은 lane 전체 폭을 사용
- `show_hold_tail` (bool)
  - 롱노트 판정과 body 연결은 유지하면서 tail cap만 표시하거나 숨김
- `note_border_enabled` (bool)
- `lane_background_opacity` (double)
  - lane별 반투명 배경 alpha
  - `0.00..0.45` 범위로 clamp
- `black_playfield_enabled` (bool)
  - `true`이면 lane spacing 구간까지 포함한 player/ghost 플레이필드 전체를 완전한 검정으로 표시
  - 기본값은 `true`이며, 기존 프로필에 명시된 `false`는 그대로 유지
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
- `key_pulse_brightness` (double)
  - 노트를 칠 때 판정선에서 터지는 폭발 이펙트의 밝기
  - `0.00..1.00` 범위로 clamp하며 `0.00`이면 이펙트를 끈다
  - Options에서는 `Skins > Hit Burst / 폭발 이펙트` 행에서 5% 단위로 조절
- `key_pulse_enabled` (bool)
  - `key_pulse_brightness`의 on/off 형태. 구버전 config 호환용으로 함께 저장한다
  - 둘 중 하나라도 꺼져 있으면 꺼진 것으로 처리한다
- `ui_font` (string)
  - 메뉴 텍스트에 쓰는 글꼴
  - `default`(Segoe UI) | `malgun`(Malgun Gothic) | `bahnschrift` | `consolas`
  - Options의 `Skins > UI Font / UI 폰트` 행에서 바꾸며 즉시 적용된다
  - 로고·랭크·콤보 숫자와 디버그 readout은 자체 글꼴을 유지한다
- `key_label_position` (string)
  - `bottom | top | off`
  - gameplay lane 안쪽에 현재 keymap의 키 이름을 작게 표시
- `judgement_line_position` (double)
  - gameplay 판정선의 세로 위치 비율
  - `0.00..1.00` 범위(0%~100%)로 clamp
  - 기본값은 `0.82`
- `gameplay_field_offset_x` (double)
  - gameplay 기어 오른쪽 위의 `↔` 핸들을 드래그해 정하는 가로 위치
  - 1920x1080 기준 `-720..720` 범위이며, 실제 화면에서는 기어와 핸들이 보이는 범위로 한 번 더 제한된다
  - 기본값은 `0.0`
- `combo_position` (double)
  - gameplay 필드 내부 콤보 표시의 세로 위치 비율
  - `0.10..0.78` 범위로 clamp
  - 기본값은 `0.24`
- `lane_width_scales` (object)
  - key mode별 개별 lane 폭 배율 배열
  - 각 mode 값은 lane 수만큼의 number array
  - 각 값은 `0.50..1.75` 범위로 clamp
- `note_width_scale` (double)
  - 중앙 기준 플레이필드 전체, lane/divider, 노트 머리·꼬리, 인접 게이지를 함께 확대·축소하는 배율 (`0.50..1.40`)
  - 100%에서 인접 노트 사이 기본 합산 여백은 `24px`
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
  - native skin은 기본 `1px` divider에 곱하고, LR2 skin은 가져온 divider 폭이 있을 때 그 값에도 곱함
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
- `single_color` (string)
  - `off` 또는 지원 색상 토큰
  - `off`가 아니면 모든 key mode와 scratch lane을 선택 색으로 표시
  - 레인별 `lane_colors`는 덮어쓰지 않으므로 `off`로 되돌리면 기존 팔레트가 복원됨
- `lane_colors` (object)
  - key mode별 lane 색상 팔레트
  - 현재 기본/저장 대상 mode는 `4k..10k`, `16k`, BMS 전용 `7+1`
  - 각 mode 값은 lane 수만큼의 string array
  - 지원 토큰:
    `ice`, `azure`, `gold`, `mint`, `rose`, `violet`, `orange`, `teal`

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
  - `4k`, `5k`, `6k`, `7k`, `8k`, `9k`, `10k`
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
