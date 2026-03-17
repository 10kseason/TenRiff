# Changelog

TenRiff의 사용자/배포 관점에서 의미 있는 변경만 간단히 기록합니다.

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
