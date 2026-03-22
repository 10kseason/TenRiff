# TenRiff 1.0.0 Baseline

이 문서는 앞으로 TenRiff 작업을 이어갈 때 기준선으로 삼아야 하는 `1.0.0` 베이스 문서입니다. 목적은 "무엇을 현재 기본값으로 보고, 무엇을 유지해야 하며, 어떤 범위 안에서 다음 작업을 쌓아야 하는지"를 빠르게 고정하는 것입니다.

## Release Identity
- 기준 릴리스 라인: `1.0.0`
- 현재 메인 타깃: Windows GUI 빌드
- 기본 제품 표면: BMS-first
- 선택 확장 표면: `.osu` osu!mania 4K~10K
- 배포 기준 경로:
  - Windows 패키지: `Baepoks/TenRiff-1.0.0`
  - 공개 소스 패키지: `opensource-Tenriff-source/TenRiff-1.0.0-source`
- 배포용 빌드 소스 오브 트루스: `build-dist/Release`

## Product Base
- 메뉴 진입점은 `MenuApp` + `MenuWindow` 조합이다.
- 기본 사용자 흐름은 `Title -> Song Select -> Gameplay -> Result`다.
- Song Select는 캐시 우선 로드, 마우스/키보드 혼합 조작, 외부 폴더 drag-and-drop, recent source 재열기를 기본 제공한다.
- Gameplay는 `GameSession`이 차트 로드, 입력, HUD snapshot, 차트 오디오 준비, 종료 경계를 맡는다.
- 렌더는 D3D11 + Direct2D/DirectWrite, 오디오는 WASAPI, 입력은 RawInput 또는 고주사율 polling을 기본 축으로 둔다.

## Baseline Capabilities
- BMS parser/normalizer/timeline은 실사용 호환 위주로 강화된 현재 구현을 기준으로 본다.
- BMS explicit compact layout(`#4K`, `#6K`, `#8K`)과 SP compact layout(`5+1 SP`, `7+1 SP`)은 이미 지원되는 기본 기능이다.
- BMS long note는 LN channel, `#LNOBJ`, `#LNMODE 2` charge tail 판정을 포함한 현재 구현을 기준으로 유지한다.
- Song indexing은 대형 라이브러리를 감안한 `safe` 기본 프로필 기준 동작을 베이스로 본다.
- osu!mania는 비활성 기본값이지만, 옵션 활성 시 4K~10K를 메뉴/런타임에서 지원하는 상태가 기본 베이스다.
- 결과 화면, replay/result JSON export, 곡별 local record 누적은 이미 포함된 기본 기능이다.

## Baseline Defaults
- 기본 판정 범위:
  - `GOOD = 75ms`
  - `BAD = 340ms`
  - `indirect_miss = 340ms`
- gameplay 시작 전 `3 / 2 / 1` 카운트다운이 기본이다.
- gameplay 종료 후 결과 전환 tail은 `ui.result_tail_ms = 3000ms` 기준이다.
- 인게임 Hi-Speed는 `F3/F4` 기본 미세 조정, `F5/F6` 굵은 조정을 유지한다.
- `F3/F4`는 누르고 있으면 연속 반영되고, 플레이 중 변경한 HS는 프로필에 저장되는 상태를 기본으로 본다.
- 기본 게이지 상태:
  - `Hard`, `Normal`, `Easy` 모두 `100%` 시작
  - auto-shift threshold는 `Hard -> Normal = 66`, `Normal -> Easy = 33`
  - `BAD/PR` 손실은 `Hard -4 / Normal -2 / Easy -2`
  - Easy 게이지가 `25% 이하`이면 `BAD/PR` 손실에 추가 `0.90x` 완화를 적용
  - `PG` 회복은 `Hard 0.01 / Normal 0.01 / Easy 0.032`
  - `GR` 회복은 `Hard 0.05 / Normal 0.05 / Easy 0.0016`
  - `GD` 회복은 `Hard 1/65 / Normal 1/65 / Easy 0.00064`

## Baseline UX
- Song Select 좌측 내비게이션은 정렬/필터, source, browse, mode/mod, records, options 흐름을 바로 접근할 수 있는 현재 구조를 유지한다.
- 좌측 내비게이션과 주요 메뉴 버튼은 마우스 좌클릭 기준 즉시 선택/실행 가능해야 한다.
- 마우스 우클릭은 현재 버튼열에서 이전 버튼 선택으로 한 칸 뒤로 이동하는 보조 UX를 제공하는 상태를 기준으로 본다.
- `F1` 도움말, `F5` 재인덱싱, Song Select 하단 커맨드 바 같은 현재 안내 UX는 유지 대상이다.

## Baseline Constraints
- 이미 안정적으로 동작하는 경로를 재설계하지 않고, 필요한 부분만 국소 수정하는 방향을 우선한다.
- Linux는 여전히 preview 수준이며, Windows GUI 경로를 기준선으로 판단한다.
- 오래된 설계 문서보다 현재 코드와 `docs/current-state.md`가 우선이다.
- 다음 작업은 `1.0.0` 베이스를 깨지 않는 가산형 변경을 우선한다.

## What To Preserve In Follow-Up Work
- BMS-first 표면
- large-library safe indexing 안정성
- 메뉴 캐시 우선 로드
- 게임플레이 HUD/오디오/입력 분리 구조
- 현재 결과 export / local records 경로
- UTF-8/Korean path 대응
- `1.0.0` 패키징 규칙과 no-songs 배포 구성

## Recommended Companion Docs
- 현재 실제 구현 상태: `docs/current-state.md`
- 설정/프로필 구조: `docs/config.md`
- 메뉴/상태 흐름: `docs/menu.md`
- 플레이 루프/오디오/입력 경계: `docs/core-loop.md`
