# TenRiff 메뉴얼 대비 갭 분석 (v0.1)

- 기준 문서: `개발메뉴얼(v0.1)/개발지시사항.txt`, `개발메뉴얼(v0.1)/Tenriff 런쳐 개발 지시사항.txt`,
  `개발메뉴얼(v0.1)/UI 개발지시사항.txt`, `개발메뉴얼(v0.1)/RAW 인풋과 멀티스레드 활용과 최적화에 대한 개발 지시사항 메뉴얼.txt`
- 분석 시점: 2025-12-23
- 범위: `src/`, `docs/`, `launch_*.{bat,sh}`, `profiles/default/` (Songs 폴더는 제외)

## 심각도 기준
- Critical: 엔드투엔드 플레이 루프를 막거나, 오디오 마스터 클럭/결정론 같은 핵심 원칙을 위반하는 갭
- High: 핵심 기능/UX 요구가 누락되어 사용성이 크게 떨어지는 갭
- Medium: 중요하지만 즉시 차단 수준은 아닌 기능/검증/보완 갭
- Low: 품질/편의 개선 수준의 갭

## 요약 (미해결 기준)
- Critical: 0
- High: 1
- Medium: 8
- Low: 1

## 현재 충족/부분 충족 항목(요약)
- BMS 파서/노멀라이즈/타임라인(샘플 타임) 구축
- 기본 10키 채널 매핑 구현
- SpeedManager/ GaugeManager 기본 스펙 반영
- osu!mania 로더(모드/키/타이밍 포인트/노트) 구현
- RawInput + InputThread + SPSCQueue + ClockSync 스캐폴딩
- 입력 폴링(1000/2000/4000/8000Hz) + RenderThread 분리
- WASAPI 백엔드 + AudioThread 뼈대
- 런처 스크립트 기본 부트스트랩

## 갭 상세

### 1) 오디오 마스터 클럭 기반 “플레이 루프” 부재
- 요구사항: AudioThread가 마스터 클럭이고, 판정/게이지/키사운드는 오디오 스레드에서 처리
- 현재 상태: 오디오 콜백에서 입력 소비 및 판정/게이지 갱신 루프 구현
- 갭: 키사운드/실오디오 믹싱 미구현
- 심각도: Medium
- 관련 파일: `src/audio/AudioThread.*`, `src/input/InputThread.*`, `src/chart/BmsTimeline.*`
- 픽스 방향(요약): AudioThread 콜백에서 Timeline + InputQueue를 소비하고 판정/키사운드/게이지를 갱신하는 핵심 루프 구축

### 2) 메뉴 상태 머신(Title/SongSelect/Gameplay/Result) 미완성(오디오 재사용)
- 요구사항: 메뉴 상태 머신 + SongIndexerThread + 캐시 + 라이브 오디오 클럭 유지
- 현재 상태: 메뉴 상태 머신 + **Windows D3D11 메뉴 UI** + SongIndexerThread + 캐시 구현
- 갭: 메뉴-게임플레이 간 **오디오 디바이스 재사용** 미완
- 심각도: Medium
- 관련 파일: `src/app/MenuApp.*`, `src/render/MenuWindow.*`, `src/render/RenderThread.*`
- 픽스 방향(요약): Menu → GameSession 전환 시 오디오 디바이스 유지(핸드오프) 연결

### 3) 입력 이벤트 → 판정 연결 부재
- 요구사항: InputThread → SPSCQueue → AudioThread에서 이벤트 소비/판정
- 현재 상태: AudioThread 콜백에서 입력 소비 및 판정/게이지 갱신 연결
- 갭: 없음(초기 구현 완료)
- 심각도: 해결
- 관련 파일: `src/input/*`, `src/audio/AudioThread.*`
- 픽스 방향(요약): 오디오 콜백에서 입력 큐 소비 + 판정/마스크/콤보 규칙 적용

### 4) Rate/Hi-Speed 적용 범위 미완성
- 요구사항: Rate는 스케줄/저지 윈도우에 반영, Hi-Speed는 시각 스크롤 속도에만 반영
- 현재 상태: SpeedManager는 존재하나 타임라인/스케줄/실제 판정에는 미연동
- 갭: Rate 변경 시 실제 재생/판정 타이밍 스케일링이 없음
- 심각도: High
- 관련 파일: `src/game/SpeedManager.*`, `src/chart/BmsTimeline.*`
- 픽스 방향(요약): 스케줄 샘플 타임을 rate로 스케일, 판정 윈도우는 scaleJudgeWindow 사용

### 5) Key Remap + 프로파일 저장/중복 경고 UI
- 요구사항: keymap.json 저장, 중복 방지, NKRO 테스트 UI
- 현재 상태: **리맵 UI/저장/중복 처리/테스트 화면 구현**
- 갭: 없음
- 심각도: 해결
- 관련 파일: `src/app/MenuApp.*`, `src/config/Keymap.*`, `docs/menu.md`
- 픽스 방향(요약): 테스트 케이스 추가(추후)

### 6) 결과 화면(Result) 및 리플레이 저장
- 요구사항: 결과 화면 상세 통계 + Enter 대기 + 리플레이 저장
- 현재 상태: **결과 화면/통계 표시/Enter 복귀 + 리플레이/결과 JSON 저장 구현**
- 갭: 없음(초기 구현 완료)
- 심각도: 해결
- 관련 파일: `src/app/MenuApp.*`, `src/app/GameSession.*`, `src/gameplay/Replay.*`, `src/gameplay/ResultStats.*`
- 픽스 방향(요약): 향후 리플레이 재생/검증용 로더 추가

### 7) osu!mania 로더의 공통 파이프라인 연결 미구현
- 요구사항: BMS와 동일한 노멀라이즈/스케줄링 경로
- 현재 상태: osu!mania → GameplayChart 변환 경로 연결됨
- 갭: BMS 노멀라이즈/타임라인과 동일한 중간 모델로의 통합은 미완
- 심각도: Medium
- 관련 파일: `src/chart/OsuManiaLoader.*`, `src/gameplay/GameplayChart.*`
- 픽스 방향(요약): 공통 노멀라이즈 중간 모델 정의 후 BMS/OSU 공통화

### 8) 판정 규칙(윈도우/마스크/LN 처리) 미구현
- 요구사항: PG/GR/GD/BD/PR 윈도우, 30ms 레인 마스크, LN 유지/이탈 규칙
- 현재 상태: GameplayEngine에서 판정/마스크/LN 규칙 구현
- 갭: 없음(초기 구현 완료)
- 심각도: 해결
- 관련 파일: `src/gameplay/GameplayEngine.*`
- 픽스 방향(요약): 테스트 강화 및 판정 파라미터 보정

### 9) 설정(config.json) 로딩 및 CLI 우선순위 적용 미구현
- 요구사항: CLI > config.json 우선순위, rate/HS/gauge 등 반영
- 현재 상태: 전역(`config/config.json`) + 프로필 설정 로딩 및 CLI 우선순위 적용
- 갭: 없음(초기 구현 완료)
- 심각도: 해결
- 관련 파일: `src/config/Config.*`, `profiles/default/config.json`, `config/config.json`
- 픽스 방향(요약): 메뉴 UI에서 실시간 갱신 반영 강화

### 10) 런처 기능 확장 미완성
- 요구사항: 바이너리 메타 정보 표시, SDL2/VC++ 안내, 종료코드별 로그 tail
- 현재 상태: 기본 폴더 체크/기본 파일 생성만 구현
- 갭: 진단/가이드 기능 부족
- 심각도: Medium
- 관련 파일: `launch_win.bat`, `launch_linux.sh`, `개발메뉴얼(v0.1)/Tenriff 런쳐 개발 지시사항.txt`
- 픽스 방향(요약): 런처 스크립트 확장 + 실행 로그 요약 강화

### 11) Linux 입력/오디오 경로 미구현
- 요구사항: evdev + ALSA(또는 대안) 지원
- 현재 상태: Windows 전용 RawInput/WASAPI만 존재
- 갭: 리눅스 실행 경로 부재
- 심각도: Medium
- 관련 파일: `src/input/*`, `src/audio/*`
- 픽스 방향(요약): evdev 입력 스레드 및 ALSA 백엔드 추가

### 12) 성능/지연 지표 HUD 및 진단 로깅 미구현
- 요구사항: 지연 히스토그램, xruns, 큐 깊이 등 표시/로그
- 현재 상태: 문서만 존재
- 갭: 계측/진단 도구 부재
- 심각도: Medium
- 관련 파일: `docs/latency.md`
- 픽스 방향(요약): 오디오 콜백/입력 큐 계측값 수집 및 HUD 노출

### 13) 단위 테스트 커버리지 부족
- 요구사항: 키 리맵, 판정/시프트 쿨다운, Rate 윈도우 스케일 등 테스트
- 현재 상태: 파서/노멀라이즈/Speed/Gauge 일부 테스트만 존재
- 갭: 핵심 신규 기능의 테스트 부재
- 심각도: Medium
- 관련 파일: `tests/unit/*`
- 픽스 방향(요약): 신규 기능 단위 테스트 추가

### 14) 문서-코드 동기화 일부 부족
- 요구사항: README/Docs에 최신 진행 상태 반영
- 현재 상태: README는 요약적, 상세 갭 분석 없음
- 갭: 갭 및 우선순위가 명확히 문서화되지 않음
- 심각도: Low
- 관련 파일: `README.md`, `docs/*`
- 픽스 방향(요약): README 요약 갱신 + 갭 분석 문서 유지

### 15) Keymap 파일 포맷 확장(레이아웃/메타) 미반영
- 요구사항: keymap.json에 layout/bindings 명시
- 현재 상태: layout 필드 포함
- 갭: 없음(초기 구현 완료)
- 심각도: 해결
- 관련 파일: `profiles/default/keymap.json`, `src/config/Keymap.*`
- 픽스 방향(요약): 키 리맵 UI와 NKRO 테스트 추가

## 확인 필요(의사결정)
- UI 구현 프레임워크(예: SDL + ImGui, 커스텀 렌더) 선택
- 리눅스 오디오 백엔드 우선순위(ALSA vs JACK) 결정
- 메뉴/게임플레이 렌더링 기본 해상도와 스케일 정책
- ✅ 리플레이 포맷(샘플 타임 기반 JSON, `profiles/<profile>/replays/*.json` + `profiles/<profile>/results/*.json`)
