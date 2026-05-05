# TenRiff 1.1.2 Final Stable Baseline

이 문서는 앞으로 TenRiff 작업을 이어갈 때 기준선으로 삼아야 하는 `1.1.2 final stable` 베이스 문서입니다. 목적은 "무엇을 현재 안정 계약으로 보고, 무엇을 유지해야 하며, 어떤 범위 안에서 다음 작업을 쌓아야 하는지"를 빠르게 고정하는 것입니다.

## Release Identity
- 기준 릴리스 라인: `1.1.2`
- 릴리스 명칭: `final stable`
- 현재 메인 타깃: Windows GUI 빌드
- 기본 제품 표면: BMS-first
- 선택 확장 표면: `.osu` osu!mania 4K~10K
- 배포 기준 경로:
  - Windows 패키지: `Baepoks/TenRiff-1.1.2`
  - 공개 소스 패키지: `opensource-Tenriff-source/TenRiff-1.1.2-source`
- 배포용 빌드 소스 오브 트루스: `build-dist/Release`

## Stable Contract
- `1.0.9`에서 정리한 gameplay playback-head 기준 입력 타이밍 보정은 유지 대상이다.
- gameplay live 입력 캡처는 저장된 RawInput 설정을 우선 사용하되 bound-key polling shadow와 RawInput 시작 실패 시 Polling fallback으로 입력 인식이 끊기지 않게 한다.
- gameplay 세션은 foreground 여부와 무관하게 계속 입력을 받는 always-allow gate를 유지한다.
- 메뉴 입력은 기존 foreground process/root-window 경계를 유지한다.
- 저장된 `input.backend` / `input.rawinput` 값은 runtime fallback 결과로 다시 써 버리지 않고 그대로 보존한다.
- Windows 배포 패키지는 메뉴 BGM용 `Mainmusic/` 런타임 자산을 포함한다.
- 공개 소스 패키지는 `external/llama.cpp/`를 계속 제외한다.

## Product Base
- 메뉴 진입점은 `MenuApp` + `MenuWindow` 조합이다.
- 기본 사용자 흐름은 `Title -> Song Select -> Gameplay -> Result`다.
- Song Select는 캐시 우선 로드, 검색/정렬/필터, 외부 폴더 drag-and-drop, recent source 재열기를 기본 제공한다.
- Gameplay는 `GameSession`이 차트 로드, gameplay audio prep, HUD snapshot, 종료 경계를 맡는다.
- 렌더는 D3D11 + Direct2D/DirectWrite, 오디오는 WASAPI, 입력은 RawInput 또는 고주사율 polling 경로를 기본 축으로 둔다.

## Baseline Capabilities
- BMS parser/normalizer/timeline은 실사용 호환 위주로 강화된 현재 구현을 기준으로 본다.
- BMS explicit compact layout(`#4K`, `#6K`, `#8K`)과 SP compact layout(`5+1 SP`, `7+1 SP`)은 기본 기능이다.
- BMS long note는 LN channel, `#LNOBJ`, `#LNMODE 2` charge tail 판정을 포함한 현재 구현을 유지한다.
- BMS 오디오는 WAV native, OGG/MP3 fallback, `ffmpeg.exe` fallback, `follow/autoplay/ignore` keysound 모드를 기본 지원한다.
- Song indexing은 `safe` 기본 프로필과 `fast` 선택 프로필을 모두 유지하되, large-library 안정성은 `safe` 기준을 우선한다.
- osu!mania는 비활성 기본값이지만 옵션 활성 시 4K~10K를 메뉴/런타임에서 지원하는 상태가 기본 베이스다.
- 결과 화면, replay/result JSON export, local records, ghost 비교 경로는 이미 포함된 기본 기능이다.

## Baseline Defaults
- 기본 판정 범위:
  - `GOOD = 75ms`
  - `BAD = 340ms`
  - `indirect_miss`는 현재 런타임에서 `BAD`와 같은 값으로 접힌다.
- 롱노트 tail 관련 기본값:
  - `hold_grace = 80ms`
  - `hold_break = 200ms`
- gameplay 시작 전 `3 / 2 / 1` 카운트다운이 기본이다.
- 결과 화면 전환 tail은 `ui.result_tail_ms = 3000ms` 기준이다.
- 세 게이지(`Hard`, `Normal`, `Easy`)는 모두 `100%`에서 시작하고 `0%`에서 즉시 실패한다.
- auto-shift gauge는 사용하지 않는다.
- song index 기본 프로필은 `safe`다.

## Baseline UX And Packaging
- Song Select의 좌측 내비게이션, 검색, 정렬, key/chart filter, page 이동, mouse wheel 이동은 유지 대상이다.
- `F5` 재인덱싱, `F1` 도움말, `F9` 스크린샷 같은 공통 단축키는 현재 UX 계약의 일부다.
- `Skins`에서 judge line, note 크기, lane spacing, lane color를 조절하는 현재 흐름은 유지한다.
- 배포 패키지는 `Songs`를 포함하지 않는 no-songs 구성을 유지한다.
- 새 사용자 프로필은 첫 실행 시 자동 생성되는 현재 정책을 유지한다.

## Baseline Constraints
- 이후 변경은 기본적으로 `1.1.2 final stable` 계약을 깨지 않는 가산형 변경을 우선한다.
- Linux는 여전히 preview 수준이며, Windows GUI 경로를 기준선으로 판단한다.
- 오래된 설계 문서보다 현재 코드와 `docs/current-state.md`가 우선이다.
- 릴리스/문서/패키지 규칙이 바뀌면 기준선 문서와 현재 상태 문서를 같이 갱신한다.

## What To Preserve In Follow-Up Work
- playback-head 기준 gameplay 입력 타이밍 보정
- gameplay live capture RawInput 우선 + polling shadow/fallback 정책과 저장 backend 보존의 분리 정책
- BMS-first 표면
- large-library safe indexing 안정성
- 메뉴 캐시 우선 로드
- `Mainmusic/` 포함 no-songs Windows 패키징
- 공개 소스 패키지의 `external/llama.cpp/` 제외 규칙

## Recommended Companion Docs
- 현재 실제 구현 상태: `docs/current-state.md`
- 설정/프로필 구조: `docs/config.md`
- 실제 플레이 흐름: `docs/gameplay-guide.md`
- 유지보수/확장 포인트: `docs/developer-extension-guide.md`
- 중장기 방향: `docs/roadmap.md`
