# TenRiff Current State

이 문서는 다음 에이전트나 새 작업자가 가장 먼저 읽어야 하는 현재 상태 문서입니다. 목표는 "지금 이 프로젝트가 무엇이고, 어디를 보면 되고, 무엇이 아직 미검증인지"를 빠르게 파악하게 하는 것입니다.

## Baseline
- 현재 프로젝트 버전은 `0.9.17`
- 후속 작업의 기준선 문서는 `docs/baseline-0.8.0.md`
- Windows GUI 빌드가 메인 타깃
- Linux는 `Baepoks-Linuxs/TenRiff-0.5.0-linux-preview` 수준의 preview만 존재
- 기본 표면은 BMS-first
- `.osu`는 옵션으로 다시 활성화 가능하며 4K~10K를 지원

## Core Architecture
- `MenuApp`
  - 메뉴 상태머신의 중심
  - Song Select, Options, Keymap, Result, Gameplay launch 진입 관리
  - 최근 유지보수 리팩터에서 Song Select record/keymap/render/state 경계를 전용 `.cpp` 파일로 분리
  - `10k-calc` Python reference 없이도 오픈소스 소스패키지에서 핵심 테스트 실행이 가능하도록 optional reference test는 skip 가능
- `SongIndexerThread`
  - 곡 인덱싱 전용 백그라운드 스레드
  - Song Select에 진행률을 보냄
- `AudioThread`
  - 오디오 마스터 클럭과 믹싱 담당
- `InputThread`
  - RawInput/폴링 입력 수집 후 큐로 전달
- `RenderThread` + `MenuWindow`
  - D3D11 + Direct2D/DirectWrite 기반 메뉴/인게임 HUD 렌더
  - 최근 유지보수 리팩터에서 대형 구현 파일을 조각 파일로 분리하는 방향으로 정리 중
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
  - `mode.key_mode`는 N2NC 스타일 lane remap 기반으로 키수를 변환
  - `mode.key_mode=none`은 차트의 원래 키 수와 기본 패턴 레이아웃을 그대로 유지
- Skins / Gameplay feel:
  - `rect` / `circle` note shape
  - note border on/off
  - combo Y 조절
  - judge line / note width / divider width / note height / LN body width 조절
  - osu!mania `ColumnLineWidth`를 읽어 lane divider 폭에 반영
  - `skin.lr2_resolution_mode`는 `auto / sd / hd / fhd`로 LR2 playskin 해상도 override 토큰을 저장
  - LR2 auto-detect는 asset 이름이 아니라 playskin `#DST_NOTE` 좌표 범위를 기준으로 SD/HD/FHD를 판정
  - 미래 노트 상단 진입 easing
  - 마지막 판정 노트 처리 직후 플레이 종료
- Judge:
  - 기본 `GOOD` 범위는 `75ms`
  - 기본 `BAD` 범위는 `340ms`
  - 간접 미스(auto-miss)는 별도 `POOR` 없이 `BAD`로 접힘
  - 간접 미스 범위도 내부적으로 `BAD`와 같은 `340ms`
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
- Song source 전환 시 profile-local `profiles/<name>/.tenriff/song-index/<source-hash>.json` 캐시를 먼저 읽음
- 캐시가 없거나 무효하면 백그라운드 인덱싱 시작
- indexing profile:
  - `safe` 기본값
  - `fast` 선택값
  - Mode Settings의 `Indexing` row와 `config.mode.song_index_profile`로 제어
- 인덱싱 stage:
  - `SCANNING FILES`
  - `BUILDING METADATA`
  - `WRITING CACHE`
- 대형 라이브러리용 메모리 하드닝:
  - 2-pass enumerate + small batch metadata build
  - `safe` profile은 대형 scan에서 1-worker 중심 budget과 촘촘한 heap trim으로 RAM high-water를 우선 관리
  - 인덱싱용 BMS parse는 asset map/불필요 header/비필수 command를 생략하는 저메모리 경로 사용
  - cache save는 giant JSON tree 대신 streaming write
- 실측:
  - `D:\Stellaverse (2025-12-14)` safe full-index 기준 `46,636` candidate / `46,602` indexed entries
  - peak memory 약 `working set 453MB`, `private 524MB`
  - 같은 라이브러리 1024-chart sample에서 fast profile throughput은 safe 대비 약 `2.05x`
- cache schema:
  - `version = 8`
  - `include_osu` 포함
  - optional `layout_label` 포함

## Runtime / Packaging Rules
- 새 사용자 프로필은 자동 생성
- 마지막으로 스테이징된 배포 패키지는 `Baepoks/TenRiff-0.9.7`
- 배포 패키지에는 `Songs`를 넣지 않음
- 배포 패키지는 메뉴 BGM용 `Mainmusic/` 런타임 자산을 함께 포함
- 배포 업데이트 요청 시 built artifacts만 `Baepoks/`에 넣는 규칙
- source-only/public handoff 요청 시 먼저 include/exclude 리스트를 작성하는 것이 사용자 선호
- 마지막으로 스테이징된 공개 소스 패키지는 `opensource-Tenriff-source/TenRiff-0.9.7-source`처럼 버전별로 별도 스테이징

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
- renderer layout 변경 뒤에는 `docs/ui-audit-checklist.md` 기준으로 `1080p`, `720p windowed`, `Performance HUD on/off` 전수 확인 필요
- Song Select fast-scroll crash repro on real CJK-heavy library
- fast profile의 장시간 full-index RAM/commit 재검증
- gameplay low-FPS/0.1%/0.01% low 확인
- OBS/Discord/Game Bar와 graphics live-apply 공존 확인
- drag-and-drop / external Korean-path sources GUI 확인
- 4K~10K `.osu` 실기 keymap 분리 확인
- Linux는 아직 실제 실행판이 아님

## Best Next Read
- runtime/config 쪽이면 `docs/config.md`
- 메뉴/인덱싱/상태머신이면 `docs/menu.md`
- 플레이루프/오디오/저지먼트면 `docs/core-loop.md`
