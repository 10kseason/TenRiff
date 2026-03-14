# TenRiff Current State

이 문서는 다음 에이전트나 새 작업자가 가장 먼저 읽어야 하는 현재 상태 문서입니다. 목표는 "지금 이 프로젝트가 무엇이고, 어디를 보면 되고, 무엇이 아직 미검증인지"를 빠르게 파악하게 하는 것입니다.

## Baseline
- 현재 릴리스 라인은 `0.7.6`
- Windows GUI 빌드가 메인 타깃
- Linux는 `Baepoks-Linuxs/TenRiff-0.5.0-linux-preview` 수준의 preview만 존재
- 기본 표면은 BMS-first
- `.osu`는 옵션으로 다시 활성화 가능하며 4K~10K를 지원

## Core Architecture
- `MenuApp`
  - 메뉴 상태머신의 중심
  - Song Select, Options, Keymap, Result, Gameplay launch 진입 관리
- `SongIndexerThread`
  - 곡 인덱싱 전용 백그라운드 스레드
  - Song Select에 진행률을 보냄
- `AudioThread`
  - 오디오 마스터 클럭과 믹싱 담당
- `InputThread`
  - RawInput/폴링 입력 수집 후 큐로 전달
- `RenderThread` + `MenuWindow`
  - D3D11 + Direct2D/DirectWrite 기반 메뉴/인게임 HUD 렌더
- `GameSession`
  - 차트 로드, gameplay audio prep, HUD snapshot, gameplay 실행 경계

## What Works Now
- BMS parser/normalizer/timeline이 실팩 호환 위주로 강화됨
- BMS explicit key headers:
  - `#4K`
  - `#6K`
  - `#8K`
  - `5+1 SP`
  - `7+1 SP`
  - header가 있거나 SP 패턴이 감지되면 해당 키수 기준 compact lane mapping 적용
- BMS keysound:
  - `follow`
  - `autoplay`
  - `ignore`
- BMS long note:
  - LN channel (`51`-`55`, `61`-`65`)
  - `#LNOBJ`
  - `#LNMODE 2` charge note는 tail release timing 판정 사용
  - 일반 BMS LN은 끝까지 유지 시 tail auto-clear
- BMS audio decode:
  - WAV native first
  - Windows Media Foundation OGG/MP3 fallback
  - MF 실패 시 `ffmpeg.exe` fallback
- Song Select:
  - 캐시 우선 로드
  - `F5` 강제 재인덱싱
  - 마우스 휠 이동
  - 좌측 `KEY` 빠른 필터 토글
  - 외부 폴더/BMS drag-and-drop
  - recent source 저장/재열기
  - BMS / OSU / All 필터
  - difficulty/title 정렬
- osu!mania:
  - 4K~10K 로드/실행
  - 키모드별 별도 keymap
  - 4K~10K chart difficulty 계산
- Skins / Gameplay feel:
  - `rect` / `circle` note shape
  - note border on/off
  - combo Y 조절
  - judge line / note width / note height / LN body width 조절
  - 미래 노트 상단 진입 easing
  - 마지막 판정 노트 처리 직후 플레이 종료
- Judge:
  - 기본 `BAD` 범위는 `200ms`
  - tail release timing은 osu hold와 BMS `#LNMODE 2` charge note에만 적용
- Graphics:
  - resolution preset (`720p`, `1080p`, `qhd`, `native`)
  - `refresh_hz` (`60..1050`, 기본 `1050`)
  - VSync off: menu effective cap `300`, gameplay configured target 최대 `1050`
  - VSync on: present refresh는 active monitor Hz를 따라가고 render pacing은 `monitor_hz * 2`를 목표로 함 (`1050` clamp)
  - `visual_offset_ms`
  - `performance_overlay`
- Gameplay performance:
  - static playfield command-list cache
  - note head/tail bitmap cache
  - fixed-size HUD note transport
- Loading UX:
  - Song Select indexing progress 표시
  - gameplay chart loading progress 표시
  - gameplay loading 중 `Esc` cancel

## Song Indexing Model
- Song source 전환 시 `.tenriff/song_index.json` 캐시를 먼저 읽음
- 캐시가 없거나 무효하면 백그라운드 인덱싱 시작
- 인덱싱 stage:
  - `SCANNING FILES`
  - `BUILDING METADATA`
  - `WRITING CACHE`
- 대형 라이브러리용 메모리 하드닝:
  - RAM-aware worker/batch budget
  - batch 단위로 metadata build
  - 처리된 candidate 즉시 해제
  - cache save는 giant JSON tree 대신 streaming write
- cache schema:
  - `version = 5`
  - `include_osu` 포함
  - optional `layout_label` 포함

## Runtime / Packaging Rules
- 새 사용자 프로필은 자동 생성
- 배포 패키지는 `Baepoks/TenRiff-0.7.6`
- 배포 패키지에는 `Songs`를 넣지 않음
- 배포 업데이트 요청 시 built artifacts만 `Baepoks/`에 넣는 규칙
- source-only/public handoff 요청 시 먼저 include/exclude 리스트를 작성하는 것이 사용자 선호
- 공개 소스 패키지는 `opensource-Tenriff-source/TenRiff-0.7.6-source`처럼 버전별로 별도 스테이징

## Config / Profile Reality
- 실제 기본값은 `config/config.json`
- runtime profile은 `profiles/<name>/config.json`
- keymap은 `profiles/<name>/keymap.json`
- `keymap.json`은 `modes.{4k..10k}` per-mode binding 구조
- stale profile은 runtime migration으로 일부 자동 보정됨
  - BMS-first default
  - keysound policy
  - osu key-mode mismatch 등

## High-Value Files
- `src/app/MenuApp.cpp`
- `src/app/GameSession.cpp`
- `src/app/SongIndex.cpp`
- `src/app/SongIndexerThread.cpp`
- `src/render/MenuWindow.cpp`
- `src/render/RenderThread.cpp`
- `src/app/ChartLoader.cpp`
- `src/chart/BmsParser.cpp`
- `src/config/Config.*`
- `src/config/Keymap.*`

## Validated Commands
- `cmake --build build --config Release --target tenriff`
- `cmake --build build --config Release --target bms_parser_tests`
- `cmake --build build --config Release --target bms_realworld_smoke`
- `ctest --test-dir build -C Release --output-on-failure -R bms_parser_tests`

## Still Manual-Validation Heavy
- Song Select fast-scroll crash repro on real CJK-heavy library
- 300GB+ BMS source에서 indexing RAM/commit 실제 측정
- gameplay low-FPS/0.1%/0.01% low 확인
- OBS/Discord/Game Bar와 graphics live-apply 공존 확인
- drag-and-drop / external Korean-path sources GUI 확인
- 4K~10K `.osu` 실기 keymap 분리 확인
- Linux는 아직 실제 실행판이 아님

## Best Next Read
- runtime/config 쪽이면 `docs/config.md`
- 메뉴/인덱싱/상태머신이면 `docs/menu.md`
- 플레이루프/오디오/저지먼트면 `docs/core-loop.md`
