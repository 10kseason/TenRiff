# TenRiff

Language: Korean | [English](README.en.md) | [简体中文](README.zh-CN.md) | [日本語](README.ja.md)

TenRiff는 Windows GUI 기반 BMS 리듬게임 런타임/런처 프로젝트입니다. 현재 정식 버전은 `1.4.5`이며, 차트 입력은 BMS 계열(`.bms/.bme/.bml/.pms`) 전용입니다. Graphics Settings에서 권리 정리된 외부 ONNX 모델을 선택해 BGA/BGI 확대에 사용할 수 있습니다. 공개 패키지에는 BGA 업스케일러 모델을 넣지 않으며, 키 모드 변환용 NK3 P64 결정 모델과 일반화 패턴 MLP만 포함합니다. MIT 라이선스를 사용하며, 번들된 서드파티 고지는 [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)에 정리합니다.

이 README는 "프로젝트를 처음 열었을 때 무엇을 보면 되는지"를 설명하는 입문 문서입니다. 더 자세한 현재 동작, 현재 `1.4.5` 프로젝트 상태, `1.1.2 final stable` 기준선, 설정 구조, 설계 문서는 [`docs/README.md`](docs/README.md)부터 이어서 읽는 구조를 기준으로 작성했습니다.

TenRiff 코드는 전통적인 장기 설계 문서 중심 개발만으로 쌓인 프로젝트가 아니라, 빠른 반복과 실험을 중시한 `vibe coding` 성격이 강한 작품이라는 점을 명시합니다.

## 프로젝트 한눈에 보기

- 기본 타깃 플랫폼: Windows
- 지원 차트: BMS 계열(`.bms/.bme/.bml/.pms`) 전용
- 그래픽 경로: D3D11 + Direct2D/DirectWrite
- 오디오 경로: WASAPI
- 입력 경로: RawInput 또는 고주사율 polling
- 직접 IP 멀티플레이: 고정 호스트 코디네이터 기반 최대 8인 TCP 대전, 멀티 선곡은 BMS 전용(기본 `27300/TCP`, [사용 안내](docs/multiplayer.md))
- 라이선스: [MIT](LICENSE)
- 서드파티 고지: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)
- 릴리스 변경 이력: [CHANGELOG.md](CHANGELOG.md)

## Credits / Attribution

TenRiff의 현재 키모드 컨버터 구현은 `krrcream-Toolkit`의 N2NC 아이디어와 코드를 바탕으로 한 적응/포팅을 포함합니다.

- 원본 프로젝트: <https://github.com/krrcream/krrcream-Toolkit>
- 반영 범위: `Tools/N2NC/N2NC.cs` 기반 키모드 변환 로직을 TenRiff의 C++ `GameplayChart` 구조로 이식
- 현재 TenRiff 구현 위치: `src/gameplay/KeyModeConverter.*`
- 라이선스/출처 고지: [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)

가능한 한 원저작자 `krrcream`과 원본 툴킷에 대한 출처를 유지하며, TenRiff 쪽 변경/통합 내용은 별도로 명시합니다.

### Thanks

OpenAI Codex, ChatGPT, Claude Code, Gemini, 그리고 프로젝트를 함께 검증해 주신 게스트 테스터분들께 감사드립니다.

## 지금 가능한 것

현재 코드베이스는 "메뉴가 뜨고, 곡을 고르고, 차트를 로드하고, 플레이하고, 결과와 로컬 기록을 남기는" 수준까지 올라와 있습니다.

- BMS 파서/노멀라이저/타임라인 처리
  - 헤더, 딕셔너리, 마디 명령
  - `#MEASURE` 분수 처리
  - `#4K / #6K / #8K` header가 있으면 해당 키수로 compact lane mapping
  - `#LNOBJ`, LN 채널(`51`-`55`, `61`-`65`) 처리
  - `#SCROLLxx` / `#xxxSC` variable scroll, including stop and reverse visual motion
  - Landmine channels `D1-D9` / `E1-E9`, `#WAV00`, gauge damage, and `ZZ` instant fail
  - CP932(Shift-JIS) 기반 레거시 BMS 텍스트 대응
- BMS 오디오 처리
  - WAV 자체 디코드
  - OGG 자체 Vorbis 디코드(`stb_vorbis`) 우선, 필요 시 Windows Media Foundation fallback
  - MP3는 Windows Media Foundation fallback
  - 필요 시 `ffmpeg.exe` fallback
  - keysound 모드 `follow / autoplay / ignore`
  - 늦게 도착한 실시간 입력도 판정 시각은 유지하면서 가청 키음만 현재 쓰기 가능한 버퍼 경계에 붙여 짧은 키음이 통째로 누락되는 현상을 방지
- Song Select
  - 캐시 우선 로드
  - `F5` 강제 재인덱싱과 화면 중앙의 stage/퍼센트/ETA 진행 표시
  - 검색, 키수 필터, 난이도 필터
  - `LV ASC/DESC`, `TITLE A-Z/Z-A` 정렬
  - 외부 폴더/BMS drag-and-drop
  - recent source 저장/재열기
  - `-` / `+`로 다음 플레이의 Rate 즉시 조정
  - Browse에서 로컬 BMS 난이도표 JSON을 선택해 MD5/SHA-256 일치 곡에 표 레벨 적용
- Gameplay / HUD
  - 실시간 HUD
  - staged chart loading progress
  - gameplay loading 중 `Esc` cancel
  - display offset
  - performance overlay
  - note head/tail bitmap cache + static playfield command-list cache
  - 고스트 배틀은 새 설정에서 기본 `OFF`이며 사용자가 켠 기존 프로필은 유지
- 옵션 / 스킨
  - Hi-Speed, Rate, EX-Hard/Hard/Normal/Easy gauge, audio, input, graphics 설정
  - `Skins` 화면에서 판정선 위치, 노트 가로/세로 크기
  - `5K~10K` lane color 편집 + 실시간 프리뷰
  - native 벡터 스킨과 LR2 playskin만 지원
  - LR2 스킨 폴더 선택/drag-and-drop으로 활성 프로필에 복사하고 note/LN/lane-gap/destination-size 정보를 반영
- 결과 / 로컬 기록
  - 결과 화면
  - replay/result JSON export
  - 곡별 로컬 기록 누적
  - 클리어 우선 best record 판정

## 아직 제한되는 것

프로젝트는 사용 가능한 상태지만 완전히 마감된 제품은 아닙니다.

- Windows GUI가 메인 경로입니다.
- Linux GUI/audio/input 백엔드는 아직 완성되지 않았습니다.
- LR2 playskin은 지원되는 gameplay 요소를 이식하는 기능이며, LR2 전체 UI를 픽셀 단위로 재현하는 기능은 아닙니다.
- 일부 GUI 경로는 빌드/테스트 위주로 검증되어 있고, 실기 수동 검증은 계속 남아 있습니다.
- 오래된 설계 문서와 현재 구현이 일부 다를 수 있으므로, 현재 동작 기준 문서는 반드시 [`docs/current-state.md`](docs/current-state.md)를 우선 봐야 합니다.

## 빠른 시작

### 1. 저장소 구조 준비

일반적으로 아래 디렉터리를 기준으로 사용합니다.

- `src/`: 실제 런타임/게임 코드
- `tests/`: unit/smoke 테스트
- `docs/`: 현재 상태 및 설계 문서
- `config/`: 기본 전역 설정
- `profiles/`: 런타임 프로필 설정/키맵/로컬 결과
- `songs/`: 차트 루트

### 2. Release 빌드

Windows 기준 기본 빌드 예시는 다음과 같습니다.

```powershell
cmake -S . -B build-dist -G "Visual Studio 17 2022" -A x64
cmake --build build-dist --config Release --target tenriff
cmake --build build-dist --config Release --target bms_parser_tests
```

Windows Defender나 다른 안티바이러스가 `TenRiff.exe`를 잠깐 잠그면 잠금이 풀린 뒤 같은 `cmake --build` 명령을 다시 실행합니다.

### 3. 공개 소스 패키지로도 빌드 가능

버전별 공개 소스 번들(`TenRiff-1.2.1-source.zip` 같은 패키지)은 `external/`(단, `external/llama.cpp/` 제외), `src/`, `tests/`, `config/`, `docs/`, `Mainmusic/`를 포함하므로, 압축을 푼 폴더만으로도 바로 configure/build 할 수 있습니다.

- 공개 소스 번들은 별도 로컬 빌드 래퍼에 의존하지 않으며, 위의 plain `cmake --build` 흐름을 사용합니다.
- `10k-calc/`는 공개 소스 번들에서 제외되므로 Python reference 기반 optional 검사는 `[skip]`으로 넘어가도 정상입니다.
- `external/llama.cpp/`도 공개 소스 번들에서 제외되므로, 로컬 LLM/tooling 체크아웃은 별도로 재구성해야 합니다.
- `profiles/`, `songs/`, `logs/`도 번들에는 없지만 `launch_win.bat`가 첫 실행 때 필요한 폴더를 자동 생성합니다.

공개 소스 번들을 푼 폴더 안에서의 예시는 다음과 같습니다.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target tenriff
cmake --build build --config Release --target bms_parser_tests
.\build\Release\bms_parser_tests.exe
```

### 4. 테스트 실행

```powershell
.\build-dist\Release\bms_parser_tests.exe
```

### 5. NK3 키 모드 변환

1.4.5 공식 Windows 빌드와 ZIP에는 standalone BMS key converter CLI/GUI를 빌드하거나 포함하지 않습니다. 게임 안의 Mode Settings에서 `NK3`를 선택하면 P64, 목표 키 수별 일반화 패턴 MLP, host beam 안전 솔버가 적용됩니다. P64는 기본 strict OpenVINO GPU이고 `TENRIFF_NK3_DEVICE=CPU`로 strict CPU를 선택하며, 패턴 MLP는 실제 실행 장치를 확인해 NPU, GPU, CPU 순서로 시도합니다.

standalone converter 소스는 개발 회귀용으로만 남겨 두며 기본 CMake 옵션 `TENRIFF_BUILD_STANDALONE_BMS_KEY_CONVERTER=OFF` 상태에서는 실행 파일을 만들지 않습니다.

### 6. 실행

직접 실행:

```powershell
.\build-dist\Release\TenRiff.exe --songs .\songs --profile default
```

런처 스크립트 사용:

```powershell
.\launch_win.bat
```

## 설정과 런타임 데이터

TenRiff는 전역 설정과 프로필 설정을 분리합니다.

실행 중에는 `OPTIONS -> Profile Setup`에서 현재 프로필의 주요 설정과 키맵을 다시 빠르게 맞출 수 있습니다.

- 전역 설정: `config/config.json`
- 프로필 설정: `profiles/<name>/config.json`
- 키맵: `profiles/<name>/keymap.json`
- 곡 인덱스 캐시: `profiles/<name>/.tenriff/song-index/<source-hash>.json`
- replay export: `profiles/<name>/replays/*.json`
- result export: `profiles/<name>/results/*.json`
- 런타임 로그: `logs/run.log`
- 크래시 로그: `logs/crash-*.log`

설정 구조를 자세히 보려면 [`docs/config.md`](docs/config.md)를 읽는 것이 가장 빠릅니다.

## 문서 읽는 순서

README는 입문 설명만 담당합니다. 세부 내용은 아래 순서로 읽는 것이 가장 효율적입니다.

1. [`docs/README.md`](docs/README.md)
   - 전체 문서 맵
2. [`docs/current-state.md`](docs/current-state.md)
   - 지금 실제로 무엇이 동작하는지
3. [`docs/baseline-1.1.2.md`](docs/baseline-1.1.2.md)
   - 후속 작업이 기준으로 삼아야 하는 `1.1.2 final stable` 베이스 문서
4. [`docs/gameplay-guide.md`](docs/gameplay-guide.md)
   - 실제 플레이 기준의 시작 방법, 기본 조작, HUD/판정/결과 화면 설명
5. [`docs/config.md`](docs/config.md)
   - 설정/프로필/키맵 구조
6. [`docs/menu.md`](docs/menu.md)
   - 메뉴/상태머신/곡 선택 흐름
7. [`docs/core-loop.md`](docs/core-loop.md)
   - 플레이 루프와 데이터 흐름
8. [`docs/roadmap.md`](docs/roadmap.md)
   - 중장기 작업 방향

## 현재 문서 해석 규칙

설계 문서와 실제 코드가 다르게 보일 수 있습니다. 이 경우 우선순위는 다음과 같습니다.

1. 현재 코드
2. [`docs/current-state.md`](docs/current-state.md)
3. [`docs/config.md`](docs/config.md)
4. 오래된 설계 문서

즉, "현재 동작"을 판단할 때는 오래된 설계보다 현재 상태 문서를 우선해야 합니다.

## 다음으로 읽을 문서

- 실제 플레이 방법이 궁금하면 [`docs/gameplay-guide.md`](docs/gameplay-guide.md)
- 설정이 궁금하면 [`docs/config.md`](docs/config.md)
- 메뉴 흐름이 궁금하면 [`docs/menu.md`](docs/menu.md)
- 플레이 루프가 궁금하면 [`docs/core-loop.md`](docs/core-loop.md)
- 전체 상태를 빠르게 파악하려면 [`docs/current-state.md`](docs/current-state.md)
