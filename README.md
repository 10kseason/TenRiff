# TenRiff

TenRiff는 Windows GUI 기반 BMS-first 리듬게임 런타임/런처 프로젝트입니다. 목표는 실사용 가능한 BMS 플레이 환경을 중심으로, 저지먼트/오디오/입력/렌더링 파이프라인을 직접 제어하는 독립 실행형 리듬게임 클라이언트를 만드는 것입니다. 현재 기준 릴리스 라인은 `0.7.7`이며, MIT 라이선스를 사용합니다. 번들된 서드파티 고지는 [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)에 정리합니다.

이 README는 "프로젝트를 처음 열었을 때 무엇을 보면 되는지"를 설명하는 입문 문서입니다. 더 자세한 현재 동작, 설정 구조, 설계 문서는 [`docs/README.md`](docs/README.md)부터 이어서 읽는 구조를 기준으로 작성했습니다.

## 프로젝트 한눈에 보기

- 기본 타깃 플랫폼: Windows
- 기본 차트 표면: BMS-first
- 선택 지원 차트: `.osu` osu!mania 4K~10K
- 그래픽 경로: D3D11 + Direct2D/DirectWrite
- 오디오 경로: WASAPI
- 입력 경로: RawInput 또는 고주사율 polling
- 라이선스: [MIT](LICENSE)
- 서드파티 고지: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)
- 릴리스 변경 이력: [CHANGELOG.md](CHANGELOG.md)

## 지금 가능한 것

현재 코드베이스는 "메뉴가 뜨고, 곡을 고르고, 차트를 로드하고, 플레이하고, 결과와 로컬 기록을 남기는" 수준까지 올라와 있습니다.

- BMS 파서/노멀라이저/타임라인 처리
  - 헤더, 딕셔너리, 마디 명령
  - `#MEASURE` 분수 처리
  - `#4K / #6K / #8K` header가 있으면 해당 키수로 compact lane mapping
  - `#LNOBJ`, LN 채널(`51`-`55`, `61`-`65`) 처리
  - CP932(Shift-JIS) 기반 레거시 BMS 텍스트 대응
- BMS 오디오 처리
  - WAV 자체 디코드
  - OGG 자체 Vorbis 디코드(`stb_vorbis`) 우선, 필요 시 Windows Media Foundation fallback
  - MP3는 Windows Media Foundation fallback
  - 필요 시 `ffmpeg.exe` fallback
  - keysound 모드 `follow / autoplay / ignore`
- Song Select
  - 캐시 우선 로드
  - `F5` 강제 재인덱싱
  - 검색, 키수 필터, 난이도 필터
  - `LV ASC/DESC`, `TITLE A-Z/Z-A` 정렬
  - 외부 폴더/BMS drag-and-drop
  - recent source 저장/재열기
  - `BMS / OSU / All` 필터
- Gameplay / HUD
  - 실시간 HUD
  - staged chart loading progress
  - gameplay loading 중 `Esc` cancel
  - display offset
  - performance overlay
  - note head/tail bitmap cache + static playfield command-list cache
- 옵션 / 스킨
  - Hi-Speed, Rate, gauge, audio, input, graphics 설정
  - `Skins` 화면에서 판정선 위치, 노트 가로/세로 크기
  - `5K~10K` lane color 편집 + 실시간 프리뷰
- 결과 / 로컬 기록
  - 결과 화면
  - replay/result JSON export
  - 곡별 로컬 기록 누적
  - 클리어 우선 best record 판정

## 아직 제한되는 것

프로젝트는 사용 가능한 상태지만 완전히 마감된 제품은 아닙니다.

- Windows GUI가 메인 경로입니다.
- Linux GUI/audio/input 백엔드는 아직 완성되지 않았습니다.
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

### 3. 테스트 실행

```powershell
.\build-dist\Release\bms_parser_tests.exe
```

### 4. 실행

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

- 전역 설정: `config/config.json`
- 프로필 설정: `profiles/<name>/config.json`
- 키맵: `profiles/<name>/keymap.json`
- 곡 인덱스 캐시: `songs/.tenriff/song_index.json`
- replay export: `profiles/<name>/replays/*.json`
- result export: `profiles/<name>/results/*.json`

설정 구조를 자세히 보려면 [`docs/config.md`](docs/config.md)를 읽는 것이 가장 빠릅니다.

## 문서 읽는 순서

README는 입문 설명만 담당합니다. 세부 내용은 아래 순서로 읽는 것이 가장 효율적입니다.

1. [`docs/README.md`](docs/README.md)
   - 전체 문서 맵
2. [`docs/current-state.md`](docs/current-state.md)
   - 지금 실제로 무엇이 동작하는지
3. [`docs/gameplay-guide.md`](docs/gameplay-guide.md)
   - 실제 플레이 기준의 시작 방법, 기본 조작, HUD/판정/결과 화면 설명
4. [`docs/config.md`](docs/config.md)
   - 설정/프로필/키맵 구조
5. [`docs/menu.md`](docs/menu.md)
   - 메뉴/상태머신/곡 선택 흐름
6. [`docs/core-loop.md`](docs/core-loop.md)
   - 플레이 루프와 데이터 흐름
7. [`docs/roadmap.md`](docs/roadmap.md)
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
