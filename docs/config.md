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

- `play_to_end` (bool; default `true`): 마지막 판정 뒤 남은 음악을 끝까지 듣습니다. `false`면 `ui.result_tail_ms`만 기다린 뒤 정상 결과로 넘어갑니다. 판정·점수·리플레이는 바뀌지 않으며, 기존 수동 후주 스킵도 유지합니다.

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
- `title_music` (string)
  - `none | default | random_bms | last_played`, 기본 `default`. 타이틀·빠른 설정 음악을 없음·기본·랜덤 BMS·마지막 플레이한 곡 중에서 선택합니다. 차트 오디오가 없으면 기본 음악으로 돌아갑니다. BMS는 키음 포함 최대 5분을 비동기로 합성해 반복합니다.
- `volume` (double)
  - master volume
- `bgm_volume` (double)
- `normalize_audio` (bool)
  - 인게임 RMS 음량 조절, 기본 false. ON은 기존 RMS→소프트 리미터→마스터 경로, OFF는 선형 마스터→최종 출력 범위 제한만 적용합니다. 메뉴 음악·선곡 미리듣기는 그대로 유지합니다.
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

[최신 판정 범위·RANK별 표·롱노트 규칙](judgement-windows.md)

- BMS `#RANK`는 기본 판정에 적용됩니다: `3/EASY` PG21ms, `2/NORMAL` PG18ms, `1/HARD` PG15ms, `0/VERYHARD` PG8ms. EASY의 GR/GD/BAD는 65/115/210ms로 유지하며 나머지 등급은 각각 18/21, 15/21, 8/21배로 줄입니다. 헤더 누락/미지원 값은 EASY입니다. `Judge Easy/Hard` 모드는 이 파일 판정에 추가 적용됩니다. `hold_grace`·`hold_break` 설정과 자동 미스 시점은 RANK로 변경하지 않습니다. 꼬리 PG/GR/GD 판정은 RANK를 반영합니다.
- `pg`, `gr`, `gd`, `bd` (double, ms)
- 기본 `pg / gr / gd`는 각각 `21ms / 65ms / 115ms`
- 기본 `bd`는 `210ms`
- `Judge Easy`는 RANK 적용 후 PG/GR/GD와 홀드 허용창을 `1.35x`로 넓힘. EASY 기준 PG/GR/GD=`28.35/87.75/155.25ms`; BAD는 모든 RANK에서 `210ms` 고정이며 `mask` 유지
- `Judge Hard`는 EASY 기준 PG/GR/GD=`17.5/55.714286/98.571429ms`. PG에는 `17.5/21`, GR/GD에는 `18/21`을 곱하여 RANK 비율을 유지하고 BAD는 모든 RANK에서 `225ms` 고정. 홀드 허용창 유지
- `indirect_miss` (double, ms)
  - 현재 프로필에서는 `340ms`로 저장·정규화하며, BAD 판정창과 별도로 무입력 자동 미스 확정 시점을 정함
  - 기본 Normal/Easy/Hard 모두 노트 시각에서 `340ms`를 초과하면 자동 미스. Normal/Easy는 BAD, Hard는 콤보를 끊는 간접 `POOR`/OD8 `MISS`로 기록
  - BAD창 밖이지만 자동 미스 전인 늦은 입력은 BAD 적중으로 인정하지 않고 이전 노트를 미스 처리한 뒤 다음 노트를 검사함
- 새 플레이는 `tenriff-native-score-v2-ruleset-4`를 기록합니다. R3는 이전 Easy/Hard 창과 RANK·LN 해제 규칙을 보존합니다. R1/R2는 PG20ms·RANK 미적용·이전 LN 해제, R1은 Easy1.25x/Hard BAD340ms/자동 미스=BAD를 유지합니다. 커스텀 판정은 비공식입니다.
- `hold_grace` (double, ms; 기본 `80ms`)
  - 설정 호환용 값이며 현재 native 꼬리 PG/GR 경계로 사용하지 않음; `hold_break`의 하한으로 사용
- `hold_break` (double, ms; 기본 `200ms`)
  - CN 꼬리를 놓지 않을 때 꼬리 시각 이후 자동 BAD까지의 대기 시간. Judge Easy에서는 `270ms`, RANK로는 변경하지 않음
  - 내부적으로 항상 `hold_grace` 이상으로 유지됨
- LN 해제는 현재 PG/GR/GD 창으로 즉시 판정하며 GD 밖이면 BAD. 일반 LN을 끝까지 누르면 자동 완료. `No LN Release`도 중간 해제를 무시하지 않음
- `mask` (double, ms; 기본 `30ms`)
  - 활성화된 레인의 추가 누름을 일시적으로 무시하는 창; RANK·Judge 모드로 변경하지 않음

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
  - `native`, 기존 별칭 `720p | 1080p | qhd`, 또는 `가로x세로` (예: `1600x900`, `1366x768`, `1280x800`, `3440x1440`). 각 축은 320–8192px.
  - 그래픽 설정의 해상도 항목은 일반적인 크기와 현재 모니터의 표시 모드를 함께 제공. `F5`로 목록 새로고침. 직접 지정한 크기도 저장·재실행 후 유지.
  - 화면 배치는 1920×1080 기준 비율을 유지하며 다른 화면 비율에는 여백을 표시. 창 모드는 제목줄·작업 표시줄을 포함해 화면 안에 들어가도록 비례 축소.
  - 스킨 설정 미리보기는 실제 게임 렌더러를 같은 비율로 축소. 플레이필드 이동, 레인·노트·기어 크기, 판정·콤보 위치와 스킨 글꼴을 반영.
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
  - `off | accuracy | score` (default `off`)
  - 정확도 또는 점수 목표와 현재 진행의 차이를 실시간 표시. 일반 게이지 실패·클리어·점수 계산을 유지하며, 목표 미달만으로 실패 처리하지 않음. 다른 자격 조건을 충족하면 최고 기록·랭킹에 반영
  - Pacemaker를 켜면 Practice와 Sudden Death는 꺼지며, 리플레이 재생과 멀티플레이에서는 적용하지 않음
- `pacemaker_target_accuracy` (double)
  - 0..100, 기본 90.0; 현재 표준 Accuracy와 목표의 차이를 %p로 표시
- `pacemaker_target_score` (int)
  - 0..10000, 기본 8000; 판정된 노트 가중치에 비례한 목표 점수와 현재 점수의 차이를 표시
- `auto_scratch_hide_lanes` (bool)
  - 기본 false. mods에 auto_scratch가 있을 때 실제 BMS 스크래치 열을 숨김. 나머지 키의 폭과 입력 번호는 유지
  - `auto_scratch`: 실제 BMS 스크래치만 자동 입력. 점수 배율 0%, ASSIST 기록이며 공식 최고 기록·랭킹 제외
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

`last_played_chart_path`는 마지막으로 실제 플레이를 시작한 차트의 로컬 경로입니다. 기본값은 빈 문자열이며 리플레이·편집기 연습은 갱신하지 않습니다. `title_music=last_played`에서 재사용합니다.

| 항목 | 형식·범위·기본값 | 동작 |
| --- | --- | --- |
| `profile_nickname` | string; UTF-8 ≤48 bytes | 표시 이름. 비면 프로필 ID를 사용하며 공백·제어문자를 정리. |
| `profile_avatar_path` | string; UTF-8 ≤2048 bytes | 로컬 PNG/JPG 아바타 경로. |
| `language` | `en`, `ko`, `ja`; `en` | UI 언어. 다른 값은 en으로 정규화. |
| `menu_font_size` | `normal`, `large`, `extra_large`; `normal` | 프로필/첫 실행에서 보통·크게·더 크게 선택. 메뉴 글자 100%·115%·130%, 게임플레이 스킨 글자는 별도 유지. |
| `all_song_sources` | bool; `false` | `ALL SONG` 통합 목록을 다음 실행에서도 복원. 등록된 폴더 캐시를 순서대로 읽고 경로가 같은 차트는 한 번만 표시. |
| `result_tail_ms` | double; `500` ms | 판정 완료 후 결과 전환 시 추가 대기 시간. `audio.play_to_end=true`이면 차트 오디오 종료 시점도 함께 고려합니다. |
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
| `note_shape` | `rect`, `circle`, `triangle`, `pentagon`, `hexagon`, `square`, `diamond`, `arrow`; `rect` | 기본 도형 노트 모양. `hex`는 `hexagon`의 호환 별칭. |
| `note_image_aspect` | `stretch`, `contain`, `width`; `stretch` | 늘이기 / 비율 유지해 안에 맞추기 / 폭을 고정하고 높이를 비율로 계산. |
| `preserve_note_image_aspect_ratio` | bool; `false` | 구버전 호환 필드. 명시된 `note_image_aspect`가 우선하며 저장 시 stretch 이외는 true. |
| `note_border_enabled`, `show_lane_dividers`, `show_judgement_line` | bool; `true` | 각각 노트 테두리, 레인 구분선, 판정선 표시. |
| `note_divider_gap_px` | double: `0..40`; `12` px | 노트 한쪽 가장자리와 구분선 사이 여백. 0이면 구분선까지 확장. |
| `show_gear_boundary_line` | bool; `false` | 기어 경계선 표시. |
| `show_timing_feedback` | bool; `true` | FAST/SLOW 글자 표시. 타이밍 막대는 별도 설정. |
| `show_timing_bar` | bool; `true` | 타이밍 막대 표시. 값이 없는 이전 프로필/스킨은 기존 글자 표시 값을 상속. |
| `timing_bar_always_visible` | bool; `false` | 막대를 피그렛 외 판정 뒤 0.75초 동안만 표시(`false`)하거나 항상 표시(`true`). 글자·현재 오차 마커는 유효한 최근 비PG 오차 뒤 0.75초 동안 유지되며 뒤이은 피그렛으로 지워지지 않음. 막대 끄기는 `show_timing_bar`로 별도 설정. |
| `timing_feedback_override` | bool; `false` | 사용자가 표시 설정을 바꾼 후 스킨 매니페스트보다 프로필의 두 표시 값을 우선. |
| `timing_text_offset_x`, `timing_bar_offset_x` | double: `-600..600`; `0` | 글자/막대의 독립 X 이동. 기존 판정 주변 배치에 더하는 1920×1080 기준 픽셀. |
| `timing_text_offset_y`, `timing_bar_offset_y` | double: `-400..400`; `0` | 글자/막대의 독립 Y 이동. 양수는 아래, 음수는 위. 기준 픽셀. |
| `show_hold_tail`, `hold_tail_taper_enabled` | bool; `false` | 각각 LN 꼬리 캡 표시, 꼬리 테이퍼. 판정 규칙은 변경하지 않음. |
| `judgement_line_glow_enabled` | bool; `true` | 판정선 주변 빛 표시. |
| `key_pulse_brightness` | double: `0..1`; `1` | Hit Burst 밝기. 0이면 끔. |
| `key_pulse_enabled` | bool; `true` | 구버전 ON/OFF 호환. false 또는 밝기 0이면 끔. |
| `hit_burst_style` | `prism`, `ring`, `spark`; `prism` | 내장 Hit Burst 모양. |
| `hud_layout` | `classic`, `studio`; `studio` | Native·가져온 LR2 스킨의 단독 플레이 HUD 스타일. LR2 노트·건반·기어 이미지와 비율은 유지함. 키가 없는 기존 프로필도 기본값 `studio`를 사용하며, Options › Skins에서 클래식으로 바꿀 수 있음. 다른 이미지 스킨·고스트 대전·멀티플레이는 기존 배치를 유지. 알 수 없는 값은 `studio`. |
| `hud_riff_map_visible` | bool; `true` | 스튜디오 덱 필드 왼쪽 노트 밀도 그래프와 시계 표시. Options › Skins › 스튜디오 리프 맵에서 변경하며 프로필·스킨 프리셋에 저장됨. 클래식 HUD에는 영향 없음. |
| `ui_font` | `default`, `malgun`, `bahnschrift`, `consolas`; `default` | 메뉴 글꼴. default는 Segoe UI; 로고·랭크·콤보 전용 글꼴은 유지. |
| `key_label_position` | `bottom`, `top`, `off`; `bottom` | 레인 키 이름 위치. |
| `judgement_line_position` | double: `0..1`; `0.82` | 판정선 세로 위치 비율. |
| `gameplay_field_offset_x` | double: `-720..720`; `0` | 1920×1080 기준 기어 가로 이동. 실제 창에서 기어와 ↔ 핸들이 보이도록 추가 제한. |
| `combo_position`, `judgement_position` | double: `0.10..0.78`; `0.24` | 콤보와 판정의 독립 Y 위치. 판정 값이 없는 기존 프로필은 combo_position을 상속. |
| `combo_offset_x`, `judgement_offset_x` | double: `-600..600`; `0` | 1920×1080 기준 콤보와 판정의 독립 X 오프셋. |
| `combo_font_scale`, `judgement_font_scale` | double: `0.50..2`; `1` | 콤보·판정 글자 크기 독립 배율. 스킨 설정에서 50–200%, 5% 단위 또는 마우스 슬라이더로 조절하며 가져온 스킨에도 프로필 값이 적용됨. |
| `lane_background_opacity` | double: `0..0.45`; `0.18` | 레인 배경 불투명도. |
| `black_playfield_enabled` | bool; `true` | 레인 간격을 포함한 필드 전체를 검정으로 표시. |
| `visual_opacity` | double: `0.20..1`; `0.96` | 노트·리셉터·키 라벨의 공통 불투명도 배율. |
| `note_outline_opacity` | double: `0..1`; `0.78` | 노트 외곽선 불투명도. |
| `note_fade_in` | double: `0..1`; `0` | 상단 검은 안개의 깊이. 노트가 내려오며 서서히 나타납니다. 0은 OFF. |
| `note_fade_out` | double: `0..1`; `0` | 판정선 위 검은 안개의 깊이. 노트가 서서히 사라집니다. 0은 OFF. 판정선·키·HUD는 유지합니다. |
| `hold_body_opacity` | double: `0..1`; `1` | LN 몸통 불투명도. |
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

모드별 배열·override 지원: `4k`부터 `16k`까지 모든 키 수와 `7+1`. 색상 토큰: `ice`, `azure`, `gold`, `mint`, `rose`, `violet`, `orange`, `teal`. 스킨 키 모드의 `7K` 다음 항목은 `7+1`이며, 바로 아래 스크래치 위치에서 좌우 표시 순서를 바꿉니다. `7+1`은 별도 키맵 모드가 아닙니다. 구버전 `expand_notes_to_dividers=true`는 여백 0으로 읽으며, 명시된 `note_divider_gap_px`가 우선합니다.

모드별 크기를 처음 조절할 때는 화면에 표시된 공용 값을 기준으로 5%씩 증감한 뒤 해당 모드의 override로 저장합니다. 예를 들어 노트·필드 크기 100%에서 `+`를 누르면 105%, 노트 높이 180%에서 `-`를 누르면 175%가 됩니다. 키 모드를 바꾸어도 아직 편집하지 않은 모드의 공용 값은 유지하며, 뒤로 나갈 때 프로필에 저장해 재실행 후에도 유지합니다.

전체 BGA 암막은 적용하지 않으며 검정 필드, 레인 배경과 기어는 기존 설정대로 유지합니다. 스킨 설정의 비주얼 레이턴시는 다섯 번째 항목입니다.

글자 가독성 보정은 메뉴·옵션·곡 목록·결과·도움말·채팅·계정·편집기·인게임 전체에 공통 적용합니다. 밝은 글자에는 어두운 외곽선, 어두운 글자에는 밝은 외곽선을 사용하며 스킨의 글자 색과 투명도는 유지합니다.

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
- 같은 모드에서는 하나의 물리 키를 기본·보조 슬롯 중 한 곳에만 새로 지정합니다. 이미 지정된 키를 입력하면 이전 슬롯은 미할당으로 비우고 선택한 슬롯으로 옮깁니다. 다른 키 모드는 바꾸지 않습니다.
- 기본 키의 명시적인 빈 문자열은 미할당으로 저장·복원합니다. 재할당으로 비운 슬롯이 재시작 후 기본 키로 돌아오지 않도록 하기 위한 동작입니다. 과거 파일의 빈 문자열도 미할당으로 취급하며, 누락한 항목이나 잘못된 비어 있지 않은 키 이름은 기존처럼 기본값을 사용합니다. `Reset`으로 해당 모드의 기본 배치를 복원할 수 있습니다.
- Song Select에서 키맵 편집을 열면 현재 선택된 차트의 lane count를 우선 사용하고, 그 다음 `mode.key_mode`, 마지막으로 `10k`를 기본 편집 대상으로 삼습니다.

## Runtime Migration Notes
- stale profile은 일부 값이 자동 교정됩니다.
- 특히 BMS 기본값과 keysound policy 관련 값은 런타임 migration 대상이며, 예전 osu chart/skin 필드는 더 이상 저장되지 않습니다.
- config 파일이 없으면 defaults로 시작하고 즉시 profile이 저장됩니다.

## 설정 조작성과 오디오 동기

새 프로필의 `audio.volume` 기본값은 `0.7`입니다. 기존에 저장한 값은 유지합니다. `audio.mute_when_inactive` 기본값은 `false`이며, 켜면 다른 창이 활성화된 동안 출력만 음소거합니다. 곡 진행과 저장 음량은 유지합니다. ASIO 버퍼 사이즈는 채널당 **샘플** 단위로 표시합니다. 내부 API의 `frames_per_buffer` 이름은 유지합니다.

키 설정은 가로 건반의 `Key 1`, `Key 2` 방식이며 기본·보조 입력을 각각 지정합니다. `keymap.json`의 선택적 `secondary_modes`는 기존 `modes`와 같은 모드/레인 구조입니다. 두 키 중 하나가 눌려 있으면 해당 키는 계속 눌린 상태로 처리합니다. 보조 키의 × 또는 입력 대기 중 Delete로 보조 지정만 해제합니다. 옵션에서 처음 열면 키·스킨 편집 모드는 4K입니다. 곡에서 키 설정을 열면 실제 차트 키 수를 사용합니다.

`skin.key_backdrop_enabled`와 `skin.key_backdrop_opacity`(0–1)는 키를 누를 때 깔리는 색을 제어합니다. 타격 효과 밝기는 기존 `key_pulse_brightness`입니다. 최초 변경 전에는 스킨 기본값을 사용하고, 사용자가 변경하면 `key_backdrop_override=true`로 프로필 값이 우선합니다. 판정선 두께는 노트 높이에 비례합니다.

난이도표 선택 중 ALL SONG은 표에 해당하는 차트만 표시합니다. Native LV를 선택하면 전체를 다시 표시하며 캐시나 원본 곡은 삭제하지 않습니다. 게임 화면과 BGA의 시계는 오디오의 실제 재생 위치를 기준으로 하며 판정·오디오 예약은 기존 쓰기 시계를 유지합니다. 속도를 실제로 변경하면 짧은 클릭음을 재생합니다.

키 입력 배경 밝기는 `skin.key_backdrop_brightness`(0–2, 기본 1), 최대 높이는 `skin.key_backdrop_height`(0–1, 기본 1)입니다. 밝기는 RGB만 바꾸고 높이는 필드 아래쪽을 기준으로 적용합니다. 농도와 독립적이며 실플레이·고스트·스킨 미리보기에서 동일하게 적용합니다. `−/+`가 표시되는 설정은 좌우 방향키를 누른 채 연속 조절할 수 있습니다.
