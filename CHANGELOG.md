# Changelog

TenRiff의 사용자/배포 관점에서 의미 있는 변경만 간단히 기록합니다.

## [1.0.3] - 2026-03-24

### Changed
- 한글 IME가 켜진 Windows 환경에서도 `RawInput`이 `VK_PROCESSKEY` 같은 값으로 깨질 때 scan code 기준으로 실제 키를 복구해 lane/control 입력이 끊기지 않도록 보강
- 프로젝트 메타데이터와 현재 상태/로드맵 문서를 `1.0.3` 배포 라인 기준으로 갱신

### Packaged
- `build-dist` 기준 Windows 배포 스테이징과 공개 오픈소스 소스 스테이징을 `1.0.3` 라인으로 갱신

## [1.0.21] - 2026-03-24

### Changed
- `Audio > Background Sound` 토글을 추가해 메뉴 BGM과 차트 배경음을 on/off 할 수 있도록 하고, hit keysound는 별도로 유지되도록 분리
- 게임 시작 직전에 눌린 키가 이후 입력 상태를 오염시키지 않도록, countdown 종료 직전 현재 눌림 상태를 non-scoring baseline으로 재동기화
- `ClockSync`가 아직 fit되지 않은 게임 시작 직후에는 입력을 `current_playback_sample_`에 뭉개지 않고, 오디오 콜백 시점의 startup anchor 기준으로 샘플 위치를 선형 환산하도록 바꿔 첫 몇 노트 입력이 늦게 몰리는 문제를 완화
- 프로젝트 메타데이터와 현재 상태/로드맵 문서를 `1.0.21` 배포 라인 기준으로 갱신

### Packaged
- `build-dist` 기준 Windows 배포 스테이징과 공개 오픈소스 소스 스테이징을 `1.0.21` 라인으로 갱신

## [1.0.1] - 2026-03-24

### Changed
- rate 변경 시 차트/오디오가 이미 playback timeline으로 압축된 상태를 기준으로, 판정 윈도우를 더 이상 `1 / rate`로 이중 축소하지 않도록 수정
- 인게임 HUD 상단에 곡 진행 프로그레스 바와 `경과 / 전체 / 남은 시간` 표기를 추가
- Song Select에 `ARTIST` 정렬을 추가하고, 검색 입력을 별도 Browse 화면이 아니라 Song Select 왼쪽 `SEARCH` 항목에서 바로 받도록 조정
- Song Select에 osu! 스타일의 `GROUP` 토글을 추가해 `Artist / Level / Folder / None` 기준으로 현재 정렬 안에서 묶어 볼 수 있게 조정
- Song Select 왼쪽 레일 항목이 많아졌을 때 버튼 높이/간격/텍스트를 자동으로 압축해 footer 영역을 침범하지 않도록 UI 레이아웃을 조정
- 해상도와 `windowed / borderless / fullscreen` 전환을 반복할 때 swap-chain 전환 실패가 곧바로 `Present` fatal로 이어지지 않도록 렌더러 재시도/복구 경로를 보강
- fullscreen 상태에서 `720p` 같은 낮은 해상도로 전환할 때 flip-model swap-chain이 `SetFullscreenState(TRUE)` 이후 다시 `ResizeBuffers` 되도록 순서를 고쳐, `Failed to present the menu frame` fatal을 줄이도록 보강
- 인게임 입력이 RawInput/큐 입력 정체 상황에서도 끊기지 않도록, 게임 세션 안에서 매핑된 lane/control 키를 큐 입력과 `GetAsyncKeyState` fallback polling으로 병합
- Windows 비미국권/중국권 키보드 레이아웃에서 `;`, `[`, `\` 같은 OEM 키가 깨지지 않도록 keymap/rawinput/polling 경로를 스캔코드 기반 정규화로 통일
- 프로젝트 메타데이터와 현재 상태/로드맵 문서를 `1.0.1` 배포 라인 기준으로 갱신

### Packaged
- `build-dist` 기준 Windows 배포 스테이징과 공개 오픈소스 소스 스테이징을 `1.0.1` 라인으로 갱신

## [1.0.01] - 2026-03-23

### Changed
- 오디오 콜백 내부 판정 루프를 입력 `polling_hz`(`1000/2000/4000/8000Hz`) 기준의 서브스텝으로 세분화하고, 미래로 매핑된 입력은 현재 버퍼 끝으로 당기지 않고 후속 틱까지 유지하도록 정리
- 입력 폴링과 내부 판정 서브스텝 빈도를 분리해 `input.polling_hz` 기본값은 `1000Hz`, `input.judgement_hz` 기본값은 `4000Hz`로 운용하도록 조정
- 입력 시각을 write cursor가 아니라 실제 playback head 기준으로 오디오 clock sync에 맞추도록 수정해, recent future-queue 변경 이후 생긴 체감 입력 지연/먹통 회귀를 완화
- 인게임 timing indicator를 최근 `100노트` 롤링 히스토리로 확장해, 최신 기록은 선명하게 남고 오래된 기록은 점차 흐려지며 밀려나도록 조정
- 렌더러 fatal 에러가 launcher 없이 `TenRiff.exe`만 직접 실행돼도 `logs/run.log`를 직접 남기도록 보강하고, Windows 배포판에 `launch_win.bat`와 빈 `logs/` 폴더를 다시 포함
- 판정 hot path를 직접 비교할 수 있는 synthetic `gameplay_judgement_benchmark` 타깃을 추가
- 루트 README와 문서 맵에 `vibe coding` 작품 성격과 감사 크레딧을 명시
- 프로젝트 메타데이터와 현재 상태/로드맵 문서를 `1.0.01` 배포 라인 기준으로 갱신

### Packaged
- `build-dist` 기준 Windows 배포 스테이징과 공개 오픈소스 소스 스테이징을 `1.0.01` 라인으로 갱신

## [1.0.0] - 2026-03-22

### Changed
- 플레이가 끝나기 전에 중도 종료한 세션은 더 이상 `CLEAR`로 기록/export되지 않도록 결과 상태 계산을 수정
- 프로젝트 메타데이터와 핵심 현재 상태/로드맵 문서, 기준선 문서를 `1.0.0` 기준으로 승격

### Packaged
- `build-dist` 기준 Windows 배포 스테이징과 공개 오픈소스 소스 스테이징을 `1.0.0` 라인으로 갱신

## [0.9989] - 2026-03-21

### Changed
- `Options > Skins`에 `Target Gap`, `Lane Width`, `Lane Spacing`를 추가해 key mode별 개별 lane 폭과 lane 사이 간격을 직접 조절할 수 있도록 확장
- skin preview, gameplay, ghost field가 모두 같은 per-lane/per-gap 레이아웃 계산을 사용하도록 정리
- 판정 흐름을 LR2 기준으로 다시 분리해 note-consuming 실패는 `BAD`, 너무 이른 non-consuming 입력은 `POOR`로 처리하고, `POOR`를 결과/리플레이/UI에 다시 노출
- 프로젝트 메타데이터와 핵심 현재 상태/로드맵 문서를 `0.9989` 기준으로 갱신

### Packaged
- `build-dist` 기준 Windows 배포 스테이징과 공개 오픈소스 소스 스테이징을 `0.9989` 라인으로 갱신

## [0.998] - 2026-03-21

### Changed
- `Options > Skins`에 `16K Center Gap` 옵션을 추가해 16키 필드/미리보기에서 좌우 8레인 사이를 14K2S 스타일로 벌릴 수 있도록 정리
- Keymap 화면의 캡처/안내 문구를 분리된 하단 footer 영역으로 옮겨, `Enter`로 키 바인딩 대기를 시작해도 lane 리스트가 재배치되지 않도록 수정
- 프로젝트 메타데이터와 핵심 현재 상태/로드맵 문서를 `0.998` 기준으로 갱신

### Packaged
- `build-dist` 기준 Windows 배포 스테이징과 공개 오픈소스 소스 스테이징을 `0.998` 라인으로 갱신

## [0.997] - 2026-03-21

### Changed
- 저장된 최고 replay가 있으면 일반 플레이에서 자동 ghost 비교를 켜고, 실플레이 왼쪽 / ghost 오른쪽 split HUD로 score, combo, gauge, judgement feedback을 함께 볼 수 있도록 추가
- Song Select에 Favorites / named Collections / local clear lamp 표시를 추가하고, 관련 필터와 상태 저장을 `config.ui`에 연결
- Options에 `Calibration Wizard` 화면을 추가해 입력 오프셋과 표시 오프셋을 즉시 저장/리셋할 수 있도록 정리
- 루트 README 3종과 `README_SOURCE_PACKAGE.md`에 공개 오픈소스 소스 패키지 전용 CMake 빌드 절차와 제외 항목(`tools/`, `10k-calc/`, runtime data dirs) 안내를 추가
- `docs/current-state*`, `docs/developer-extension-guide*`, `README_SOURCE_PACKAGE.md`에 공개 소스 패키지 갱신 시 staged source bundle 자체에서 standalone configure/build/test를 확인해야 한다는 유지보수 규칙을 추가
- 프로젝트 메타데이터와 핵심 현재 상태/로드맵 문서를 `0.997` 기준으로 갱신

### Packaged
- `build-dist` 기준 Windows 배포 스테이징과 공개 오픈소스 소스 스테이징을 `0.997` 라인으로 갱신

## [0.995] - 2026-03-21

### Changed
- 오토 게이지 시프트를 제거하고, 선택한 `Hard / Normal / Easy` 게이지가 곡 시작부터 종료 또는 실패까지 그대로 유지되도록 정리
- 세 게이지 모두 `100%`에서 시작하고 `0%` 도달 시 즉시 게임오버가 나도록 통일
- 리절트 화면에서 `Left`로 즉시 재시작, `F1`로 replay 재생, `F9`로 전역 스크린샷 저장을 지원하도록 입력 동작을 재배치
- 키 설정 화면에서 Save를 한 번만 눌러도 즉시 저장되고 저장 완료 메시지가 뜨도록 UX를 정리
- Graphics Settings에 `Language` row를 추가하고 메뉴 UI의 `English / 한국어` 전환을 즉시 반영하도록 추가
- 향후 다국어 확장을 쉽게 따라갈 수 있도록 `docs/localization*.md` 현지화 참고 문서를 추가
- 프로젝트 메타데이터와 핵심 현재 상태/로드맵 문서를 `0.995` 기준으로 갱신

### Packaged
- `build-dist` 기준 Windows 배포 스테이징과 공개 오픈소스 소스 스테이징을 `0.995` 라인으로 갱신

## [0.994] - 2026-03-20

### Changed
- 게이지 상한을 `Hard 100 / Normal 50 / Easy 40`으로 재정의
- `Normal BAD/PR`을 `-6.25`, `Easy BAD/PR`을 `-4.1`로 조정하고 다른 회복 수치는 유지
- 다운시프트 후 게이지가 다음 타입의 상한값으로 시작하도록 정리
- 직전 `Hard/Normal/Easy = -10/-6/-4` 기본 테이블을 쓰던 기존 프로필도 새 손실값으로 올라오도록 마이그레이션 경로를 추가
- 프로젝트 메타데이터와 핵심 현재 상태 문서를 `0.994` 기준으로 갱신

### Packaged
- `build-dist` 기준 Windows 배포 스테이징과 공개 오픈소스 소스 스테이징을 `0.994` 라인으로 갱신

## [0.9.92] - 2026-03-20

### Changed
- 노말 게이지 시작 시점을 `50%`가 아니라 `100%`로 올리고, 이후 `33%` 이하에서 Easy로만 다운시프트되도록 조정
- 이지 게이지 시작 시점도 `100%`로 올려서, Easy 시작 모드에서는 `0%` 도달 시 바로 게임오버가 나도록 정리
- BAD 손실을 `Hard -4 / Normal -2 / Easy -2`로 완화
- Easy 게이지만 회복 테이블을 따로 조정해 `PG 0.032 / GR 0.0016 / GD 0.00064`를 사용하고, Hard/Normal 회복은 기존 `PG 0.01 / GR 0.05 / GD 1/65`를 유지
- 직전 기본 회복/손실 테이블(`Hard/Normal/Easy BAD = -8/-6/-4`, `Easy PG 0.135`)을 쓰던 기존 프로필도 새 값으로 올라오도록 마이그레이션 경로를 추가
- 게이지 시작값 변경을 반영하도록 관련 gauge/gameplay 회귀 테스트를 갱신
- 프로젝트 메타데이터와 핵심 현재 상태/로드맵 문서를 `0.9.92` 기준으로 갱신

### Packaged
- `build-dist` 기준 Windows 배포 스테이징과 공개 오픈소스 소스 스테이징을 `0.9.92` 라인으로 갱신

## [0.9.91] - 2026-03-20

### Changed
- BAD 게이지 기본 손실을 `Hard -8 / Normal -6 / Easy -4`로 재조정
- Easy 게이지가 `25%` 이하일 때 BAD/PR 손실에 추가 `0.90x` 완화를 적용
- 이전 `-14.24896 / -8.90560 / -6.27845` BAD 기본값을 쓰던 프로필도 새 값으로 올라오도록 마이그레이션 경로를 추가
- 프로젝트 메타데이터와 핵심 현재 상태/로드맵 문서를 `0.9.91` 기준으로 갱신

### Packaged
- `build-dist` 기준 Windows 배포 스테이징과 공개 오픈소스 소스 스테이징을 `0.9.91` 라인으로 갱신

## [0.9.19] - 2026-03-20

### Changed
- gameplay 시작 전 `3 / 2 / 1` 카운트다운 숫자를 DirectWrite text metrics 기준으로 다시 중앙 배치해 정가운데 정렬이 틀어지지 않도록 수정
- 프로젝트 메타데이터와 핵심 현재 상태/로드맵 문서를 `0.9.19` 기준으로 갱신

### Packaged
- `build-dist` 기준 Windows 배포 스테이징과 공개 오픈소스 소스 스테이징을 `0.9.19` 라인으로 갱신

## [0.9.18] - 2026-03-20

### Changed
- 타이틀 메뉴 메인 로고 텍스트 정렬을 고정 좌표 추정 대신 DirectWrite text metrics 기준으로 재배치해 위치가 틀어지지 않도록 수정
- 기본 `BAD/PR` 게이지 손실을 이전 `0.9.17` 기본값 대비 2.2배로 강화하고, 이전 기본값을 쓰던 프로필이 새 손실값으로 마이그레이션되도록 조정
- 프로젝트 메타데이터와 핵심 현재 상태/로드맵 문서를 `0.9.18` 기준으로 갱신

### Packaged
- `build-dist` 기준 Windows 배포 스테이징과 공개 오픈소스 소스 스테이징을 `0.9.18` 라인으로 갱신

## [0.9.17] - 2026-03-20

### Changed
- 타이틀 메뉴의 메인 브랜딩을 중앙 정렬 `TenRiff` 락업으로 조정하고, 기존 버튼/가이드 패널 레이아웃은 유지
- 프로젝트 메타데이터와 핵심 현재 상태/로드맵 문서를 `0.9.17` 기준으로 갱신

### Packaged
- `build-dist` 기준 Windows 배포 스테이징과 공개 오픈소스 소스 스테이징을 `0.9.17` 라인으로 갱신

## [0.9.16] - 2026-03-20

### Changed
- `src/render/MenuWindow.cpp`에서 분리 과정 중 누락됐던 창 초기화, imported-skin cache, gameplay static cache, mouse hit-test 구현을 복구해 `build-dist` 릴리스 링크 실패를 수정
- `build-dist` 기준 `tenriff`와 `bms_parser_tests` 빌드, `bms_parser_tests.exe` 실행까지 다시 통과

## [0.9.15] - 2026-03-20

### Changed
- `tests/unit/test_10k_calc_consistency.cpp`와 `tests/smoke/bms_10k_compare_smoke.cpp`를 조정해 로컬 `10k-calc` Python reference가 없을 때 optional check를 skip 하도록 변경
- 오픈소스 소스패키지가 `10k-calc` 체크아웃 없이도 앱 빌드와 핵심 테스트 실행을 진행할 수 있게 정리
- 프로젝트 메타데이터와 핵심 현재 상태/로드맵 문서를 `0.9.15` 기준으로 갱신

## [0.9.14] - 2026-03-20

### Changed
- `MenuApp`의 Song Select render 조립과 filter/sort state rebuild 경계를 `src/app/MenuAppSongSelectRender.cpp`, `src/app/MenuAppSongSelectState.cpp`, `src/app/MenuAppSongSelectUtils.cpp`로 분리
- `MenuAppTail.inl`에서 Song Select 전용 대형 구현 블록을 제거해 tail 조각이 gameplay/result 쪽에 더 집중되도록 정리
- 프로젝트 메타데이터와 핵심 현재 상태/로드맵 문서를 `0.9.14` 기준으로 갱신

## [0.9.13] - 2026-03-20

### Changed
- 유지보수용으로 `MenuApp` 분리 파일과 Song Select 경계에 짧은 설명 주석을 추가해 캐시/정렬/legacy 동기화 규칙을 더 쉽게 추적할 수 있게 함
- 프로젝트 메타데이터와 핵심 현재 상태/로드맵 문서를 `0.9.13` 기준으로 갱신

## [0.9.12] - 2026-03-20

### Changed
- `MenuApp`의 Song Select 하위 유지보수 경계를 더 잘라 `src/app/MenuAppRecords.cpp`와 `src/app/MenuAppSongSelect.cpp`로 분리
- record/replay/best-result/background-preview 경로와 일부 song-list helper를 `MenuAppTail.inl`에서 분리해 tail 조각의 책임을 축소
- 프로젝트 메타데이터와 핵심 현재 상태/로드맵 문서를 `0.9.12` 기준으로 갱신

## [0.9.11] - 2026-03-20

### Changed
- `MenuApp`의 keymap 화면 입력/렌더/캡처/저장 로직을 `src/app/MenuAppKeymap.cpp`로 분리해 메인 파일과 tail 조각의 결합도를 낮춤
- 프로젝트 메타데이터와 현재 상태/로드맵 문서를 `0.9.11` 기준으로 갱신

## [0.9.10] - 2026-03-20

### Added
- 루트 README와 `docs/` 문서들의 영문/중문 번역본을 추가해 다국어 문서 진입점을 정리
- `mode`/mod 확장과 유지보수 절차를 설명하는 개발자용 문서를 추가

### Changed
- 문서 인덱스와 현재 상태 문서를 `0.9.10` 기준으로 갱신

## [0.9.9] - 2026-03-20

### Changed
- `MenuWindow.cpp`, `MenuApp.cpp`, `GameSession.cpp` 대형 구현 파일을 더 작은 구현 조각으로 분리하는 유지보수 리팩터를 진행
- 동작 변경 없이 대형 렌더/메뉴/게임플레이 구현을 분리해 후속 `1.0.0` 안정화 작업의 충돌 범위를 줄임

## [0.9.8] - 2026-03-20

### Changed
- `skin.lr2_resolution_mode` config schema를 추가하고 `auto | sd | hd | fhd` 저장/정규화/저장 테스트를 맞춤
- `docs/config.md`와 `docs/current-state.md`를 새 LR2 해상도 override 설정에 맞게 갱신
- 프로젝트 메타데이터를 `0.9.8` 기준으로 승격

## [0.9.7] - 2026-03-19

### Added
- LR2 플레이 스킨 포팅 경로를 추가해 기본 활성 분기 기준으로 노트/LN 이미지와 레인 배치를 가져올 수 있게 함
- osu!mania 쪽 per-note hitsound 해석, imported skin 크기 비율 반영, 흰선 토글, LN tail taper 같은 1.0 안정화 작업을 반영

### Changed
- `mode.key_mode` 변환이 osu!mania에도 적용되도록 정리하고, judge/easy-hard persistence, gauge 기본값, 스킨 설정 동작을 보정
- `MenuApp` 대형 파일 분리 리팩터를 진행해 skin/settings 화면 로직을 별도 translation unit으로 이동

### Packaged
- Windows 배포 스테이징과 공개 소스 스테이징을 `0.9.7` 기준으로 새로 생성
- 공개 소스 zip과 Windows 배포 zip을 `0.9.7` 기준으로 다시 생성

## [0.9.4] - 2026-03-18

### Added
- `mode.key_mode`에 `none` 옵션을 추가해 차트의 원래 키 수와 패턴 레이아웃을 그대로 따르는 native 경로를 노출
- `tools/build_with_retry.ps1`와 패키징 재시도 로직으로 Windows Defender/안티바이러스가 `TenRiff.exe`를 잠그는 동안에도 빌드/배포 재시도를 자동화

### Changed
- Mode Settings에서 BMS-only 상태여도 `Key Mode`를 `none` 포함 전체 런타임 키모드로 바꿀 수 있게 하고, 설정/마이그레이션 기본값도 `10k` 강제 대신 `none` 기반으로 정리
- 결과 화면 복귀 직후 메뉴 BGM 장치 충돌이 나던 경로를 줄이기 위해 gameplay 세션 종료 시 WASAPI/input 자원을 실제 `shutdown()`까지 수행하도록 조정
- 메뉴 BGM 재생 실패가 날 때 같은 파일을 프레임마다 다시 열어 경고를 도배하던 동작을 짧은 재시도 쿨다운으로 완화

### Packaged
- Windows 배포 스테이징과 공개 소스 스테이징을 `0.9.4` 기준으로 새로 생성
- 공개 소스 zip과 Windows 배포 zip을 `0.9.4` 기준으로 다시 생성

## [0.9.3] - 2026-03-18

### Added
- BMS 실차트 기준 `key_mode` 조합 스모크(`bms_mode_smoke`)와 `N2NC` 비교 스모크를 통해 리팩터 이후 차트 변환/모드 조합 회귀를 더 넓게 검증
- 비정상 종료 시 `logs/crash-*.log`를 남기는 Windows 크래시 로거를 추가해 추후 사용자 로그 수집/분석 경로를 마련

### Changed
- `mode.key_mode`의 `N2NC` 포팅 품질을 원본 `krrcream-Toolkit` 흐름에 더 가깝게 맞춰 note delta와 변환 shape 차이를 크게 줄임
- BMS에서 `key_mode`를 `SR/FR/full_short/full_long/judge` 등 다른 모드와 함께 써도 정렬/overlap이 깨지지 않도록 후처리와 회귀 테스트를 보강
- CMake, README, 현재 상태 문서, 패키지 스코프를 `0.9.3` 기준으로 정렬하고 공개 소스 번들에 `README.en.md`, `README.zh-CN.md`를 포함하도록 패키징 스크립트를 갱신

### Packaged
- Windows 배포 스테이징과 공개 소스 스테이징을 `0.9.3` 기준으로 새로 생성
- 공개 소스 zip과 Windows 배포 zip을 `0.9.3` 기준으로 다시 생성

## [0.9.2] - 2026-03-18

### Added
- 영문 README(`README.en.md`)와 중국어 README(`README.zh-CN.md`)를 추가하고 루트 README에 언어 링크를 연결

### Changed
- CMake 프로젝트 버전과 루트/문서 메타데이터를 `0.9.2` 기준으로 정렬
- `mode.key_mode` 변환과 메뉴 BGM 수정 이후 현재 코드 상태를 반영하도록 현재 상태/README 문구를 갱신
- `MenuApp::publish_snapshot()`가 Song Select 렌더 데이터를 직접 조립하던 중복 블록 대신 기존 `populate_song_select_render_data(...)` 헬퍼를 다시 사용하도록 정리

## [0.9.1] - 2026-03-17

### Changed
- `ModeManager` 기반 모드 정규화/차트 변환/점수 배율 계산 리팩터가 들어간 현재 런타임 상태로 공개 릴리스 메타데이터를 `0.9.1` 기준으로 정렬

### Packaged
- 공개 소스 번들 스테이징 경로를 `opensource-Tenriff-source/TenRiff-0.9.1-source`로 갱신
- Windows 배포 스테이징과 공개 소스 zip을 `0.9.1` 기준으로 다시 생성

## [0.9.0] - 2026-03-17

### Added
- BMS는 `SUBTITLE`/`DIFFICULTY`, osu!mania는 `Version`/`TitleUnicode`/`ArtistUnicode`를 활용해 차분명과 표시 메타데이터를 더 정확히 보존
- Title/Song Select 메뉴 화면에서 `Mainmusic/` 배경 음악을 재생하고, 공개 배포판도 해당 런타임 자산을 함께 포함

### Changed
- osu skin import가 현재 활성 스킨 소스를 강제로 `osu`로 덮어쓰지 않도록 조정해 native/osu 토글 고정을 해소
- 공개/오픈소스 릴리스 메타데이터를 `0.9.0` 기준으로 정렬

### Packaged
- 공개 소스 번들 스테이징 경로를 `opensource-Tenriff-source/TenRiff-0.9.0-source`로 갱신
- Windows 배포 스테이징과 공개 소스 zip을 `0.9.0` 기준으로 다시 생성

## [0.8.8] - 2026-03-17

### Changed
- 기본 `GOOD` 판정 범위를 `75ms`로 상향하고, 이전 기본값 `55ms`를 쓰는 프로필은 런타임 migration으로 자동 승격
- Linux preview launcher의 기본 judge preset도 현재 기본값(`GOOD 75 / BAD 340 / hold 80/200`)으로 정렬
- 공개/오픈소스 릴리스 메타데이터를 `0.8.8` 기준으로 정렬

### Packaged
- 공개 소스 번들 스테이징 경로를 `opensource-Tenriff-source/TenRiff-0.8.8-source`로 갱신
- Windows 배포 스테이징과 공개 소스 zip을 `0.8.8` 기준으로 다시 생성

## [0.8.6] - 2026-03-17

### Added
- Skin Settings에 key-mode별 `Divider Width` 조절 추가
- osu!mania skin의 `ColumnLineWidth`를 읽어 gameplay lane divider 폭에 반영

### Changed
- gameplay static playfield와 skin preview가 lane divider 폭 변경을 캐시 키로 추적하도록 보강
- 공개/오픈소스 릴리스 메타데이터를 `0.8.6` 기준으로 정렬

### Packaged
- 공개 소스 번들 스테이징 경로를 `opensource-Tenriff-source/TenRiff-0.8.6-source`로 갱신
- Windows 배포 스테이징과 공개 소스 zip을 `0.8.6` 기준으로 다시 생성

## [0.8.5] - 2026-03-17

### Changed
- 공개/오픈소스 릴리스 메타데이터를 `0.8.5` 기준으로 정렬
- 루트 README, 현재 상태 문서, CMake 프로젝트 버전이 현재 공개 라인을 직접 가리키도록 갱신
- 공개 소스 번들 scope를 정리해 내부 작업용 `AGENTS.md`는 더 이상 오픈소스 패키지에 포함하지 않음

### Packaged
- 공개 소스 번들 스테이징 경로를 `opensource-Tenriff-source/TenRiff-0.8.5-source`로 새로 갱신
- Windows 배포 스테이징과 공개 소스 zip을 `0.8.5` 기준으로 다시 생성

## [0.8.0] - 2026-03-16

### Added
- 대형 라이브러리용 song indexing 프로파일 `mode.song_index_profile` 추가: 기본 `safe`, 선택 `fast`

### Changed
- song indexing을 전후 2-pass enumerate + small batch 처리로 재구성해 후보 파일 전체 적재를 제거
- 인덱싱용 BMS 파서는 `WAV/BMP`, 대부분의 unknown header, 비필수 measure command를 건너뛰는 저메모리 경로를 사용
- 대형 scan에서는 더 보수적인 worker/batch budget과 주기적 heap/working-set trim을 적용해 RAM high-water를 크게 낮춤
- Mode Settings에서 `Indexing` row로 `Safe/Fast` 프로파일을 직접 선택 가능
- 배포판과 공개 소스 번들 릴리스 라인을 `0.8.0`으로 승격

### Verified
- `D:\Stellaverse (2025-12-14)` full-index 실측에서 `46,636` candidate 기준 safe profile peak 메모리가 약 `working set 453MB / private 524MB` 수준으로 완주 확인
- 같은 라이브러리 1024-chart sample 기준 fast profile이 safe profile 대비 약 `2.05x` 빠른 metadata throughput 확인

## [0.7.9] - 2026-03-16

### Changed
- 대용량/외부 BMS 폴더를 불러올 때 song source 내부에 `.tenriff/song_index.json`을 쓰지 않도록 song index cache를 profile-local 경로(`profiles/<name>/.tenriff/song-index/<source-hash>.json`)로 이동
- 기존 song source 내부 legacy cache가 있으면 읽기만 하고 새 profile-local cache로 안전하게 마이그레이션
- 배포판과 공개 소스 번들 릴리스 라인을 `0.7.9`로 승격

## [0.7.8] - 2026-03-16

### Added
- gameplay 초보자 온보딩용 `docs/gameplay-guide.md` 문서 추가

### Changed
- 간접미스(auto-miss) 판정 시점을 별도 `judge.indirect_miss` 설정으로 분리하고 기본값을 `500ms`로 완화
- 배포판과 공개 소스 번들 릴리스 라인을 `0.7.8`로 승격
- 외부 BMS 폴더 안에 song index cache를 쓰지 않도록 캐시 저장 위치를 profile-local 경로로 이동

## [0.7.7] - 2026-03-16

### Added
- 로컬 `10k-calc` Python 원본과 C++ 포트를 직접 대조하는 consistency doctest 추가
- 랜덤 실차트 샘플로 Python `10k-calc`와 TenRiff 결과를 비교하는 `bms_10k_compare_smoke` 추가
- `stb_vorbis` 기반 OGG 내부 디코더 추가
- 배포/소스 번들용 `THIRD_PARTY_NOTICES.md` 정리

### Changed
- C++ 난이도 계산을 축약판이 아니라 로컬 `10k-calc/new_calc.py` 전체 흐름 기준으로 이식
- BMS 난이도 계산 경로가 `5+1 SP`, `7+1 SP`, `14+2 DP` 같은 scratch layout의 canonical lane order를 따르도록 보정
- gameplay note motion이 HUD poll 시각이 아니라 오디오 callback 기준 시각으로 extrapolation 되도록 변경
- gameplay HUD revision을 motion/text로 분리해서 note motion 중 문자열 캐시 churn을 줄임

### Fixed
- 일부 실차트에서 발생하던 `10k-calc` 대비 난이도 오차 완화
- OGG/WAV 재생 시 불필요하게 `ffmpeg`에 의존하던 경로 제거
- gameplay note motion 미세 끊김 원인 중 하나였던 timestamp 기준점 오차 수정

## [0.7.6] - 2026-03-15

### Added
- BMS compact/SP/DP layout 지원 확대: `4K`, `6K`, `8K`, `5+1 SP`, `7+1 SP`, `PMS 9K`, `14+2 DP`
- Song Select cache-first 로드, source 브라우징, recent source 복원, drag-and-drop 폴더 인덱싱
- 결과 화면/로컬 best record 표시 강화

### Changed
- gameplay 렌더 경로 성능 최적화: fixed-size HUD transport, static playfield command-list cache, note bitmap cache
- render pacing과 performance overlay 개선
- WASAPI / chart audio playback 안정화와 sample-rate 선택 로직 개선

### Fixed
- shared-mode WASAPI stutter/slow playback 문제 수정
- 입력 backlog/미래 이벤트 처리 때문에 체감 입력이 밀리던 문제 완화
- 일부 gameplay HUD/overlay 경로의 불필요한 복사와 할당 부담 감소

## [0.7.5] - 2026-03-14

### Added
- Windows GUI 메뉴/곡 선택/옵션/결과/플레이 루프의 첫 배포 가능한 패키지 라인 정리
- replay/result JSON export와 local records 기반 결과 누적
- 초기 공개 소스 번들 라인 정리

### Changed
- 프로젝트 메타데이터와 패키지 구조를 버전별 스테이징 방식으로 정리
- 문서 엔트리포인트를 README + `docs/README.md` 기준으로 재정리
