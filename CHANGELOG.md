# Changelog

TenRiff의 사용자/배포 관점에서 의미 있는 변경만 간단히 기록합니다.

## [0.7.8] - 2026-03-16

### Added
- gameplay 초보자 온보딩용 `docs/gameplay-guide.md` 문서 추가

### Changed
- 간접미스(auto-miss) 판정 시점을 별도 `judge.indirect_miss` 설정으로 분리하고 기본값을 `500ms`로 완화
- 배포판과 공개 소스 번들 릴리스 라인을 `0.7.8`로 승격

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
