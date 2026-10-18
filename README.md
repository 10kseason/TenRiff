**2026-10-18 · 1.9.5.3:** [스킨 설정 정리와 검색·입력 개선](docs/release-1.9.5.3.ko.md). 스킨 미리보기의 번호 버튼으로 편집 레인을 선택합니다. UI 폰트·폰트 파일과 인게임 커서는 프로필에서 설정합니다. 스크래치 위치는 런타임 키 모드가 원본일 때만 표시하며, 7+1 배치에서 조절할 수 있습니다.

**2026-10-17 · 1.9.5.2:** [노트·필드 크기 10%](docs/release-1.9.5.2.ko.md). GOMazk님의 기여(PR #102)를 포함합니다. 노트·필드 크기의 조절 범위를 50–140%에서 10–140%로 넓혔습니다. 기본값100%와 저장값, 키 모드별 설정을 유지하며 메뉴·플레이·고스트·미리보기에서 같은 하한을 사용합니다. 판정·점수·입력·오디오 계산과1.9.5.1의350ms FAST/SLOW·파일 선택창 동작은 유지합니다.

**2026-10-17 · 1.9.5.1:** [판정선·FAST/SLOW·파일 선택창](docs/release-1.9.5.1.ko.md). FAST/SLOW는 마지막 최대판정 범위 밖 입력부터 0.35초 동안 남습니다. 최대판정을 쳐도 즉시 지우지 않고, 최대판정 연타가 표시 시간을 연장하지도 않습니다. 그 전에 새 범위 밖 판정이 나오면 즉시 새 값으로 바뀝니다. Always 모드의 범위 막대와 0.7초 점/바코드 수명은 그대로입니다.

**2026-10-17 · 1.9.5:** 메뉴·결과 음악도 사운드에서 선택한 장치로 재생합니다. 인디케이터는 FAST 파랑 / 최대판정 초록 / LATE 빨강이며 FAST/SLOW도 최대판정 입력으로 갱신합니다. 로비 랜덤 순서는 OFF → 미러 → 랜덤 → S랜덤 → 스크고정 랜덤 → OFF입니다. [출력 장치 통합과 판정 인디케이터](docs/release-1.9.5.ko.md).

**2026-10-17 · 1.9.4:** 플레이 중 음원·정적 BGA 캐시를 보관하고 강제 메모리 정리를 보류합니다. 인디케이터 모양·0.7초 표시, 출력 장치 선택, 프로필 단축키, ×0.5/×2와 마디선 정렬을 포함합니다. [메모리 정리 시점과 플레이 설정](docs/release-1.9.4.ko.md).

**2026-10-16 · 1.9.3 릴리스 범위:** 공푸어 정확도 제외(R7/R8), BMS 판정 배지·인게임 레벨·마디선, 상시 FAST/SLOW 수정, 안전 불러오기 기본값, 일반 ONNX 잔차/완성 이미지 및 지원 구조의 외부 Safetensors 변환을 포함합니다. BGA 모델은 실행·소스 패키지에 동봉하지 않으며 OFF/빈 경로가 기본입니다. 게스트 업로드 제외·고정 등급표·기준선 1.9.0을 유지합니다. [변경·호환성](docs/release-1.9.3.ko.md) · [검증 절차](docs/release-1.9.3-gate.md). 공개 여부와 다운로드는 [1.9.3 릴리스 페이지](https://github.com/10kseason/TenRiff/releases/tag/1.9.3)가 기준이며, 아래 날짜별 설명은 당시 이력입니다.

> 이전 1.9.2: [게스트/온라인·원본 해상도 업스케일·UI 변경](docs/release-1.9.2.ko.md).

> 2026-10-16: [FFmpeg 디블록과 외부 ONNX x2](docs/deblock-lunasr-20261016.ko.md)

> 2026-10-16: [원본 해상도에서 먼저 복원하는 BGA 업스케일](docs/native-upscale-20261016.ko.md)

# TenRiff

**2026-10-16 · 1.9.1.1 UI:** [홈·그래픽·로딩 및 고정 등급 — 1.9.1.1 후속 후보](docs/ui-feedback-20261016.ko.md).

**2026-10-15 · 1.9.1.1 BMS 온라인 후속 버전:** 새 BMS 게이지·타이밍·RANDOM 분기 기록을 Sites 커뮤니티 리더보드에 제출하고, 규칙과 실제 분기 내용별로 순위를 분리합니다. 새 클라이언트와 대응 웹 배포가 필요하며, Sites는 계속 `client_submitted` 기록입니다. 별도 Ranked 검증 서버의 차단은 유지합니다. 아래 1.9.1의 로컬 전용 안내는 그 버전의 이력이며 기존 공개 파일과 고정 기준 1.9.0은 보존합니다. [범위·호환성·검증](docs/feedback-1.9.1.1.ko.md).

**2026-10-15 · 1.9.1:** 사용자가 승인한 1.9.1에 앞선 볼륨·입력·후주 수정, BMS TOTAL·RANDOM 호환, 결과 색과 10 로고를 포함합니다. 홈의 언어 버튼은 EN → KO → JA → ZH 순서로 바꾸고 선택을 저장합니다. 새 BMS 게이지·분기 기록은 서버 검증 지원 전까지 로컬 전용입니다. 고정 기준은 1.9.0이며 이전 공개 버전·설치·프로필은 보존합니다. 아래 날짜별 로컬 작업의 버전 유지·미공개 문구는 당시 기록이며 이 1.9.1 범위가 현재 기준입니다. [변경·호환성](docs/feedback-1.9.1.ko.md) · [릴리스 검증 절차](docs/release-1.9.1-gate.md).

**2026-10-15 · BMS TOTAL·RANDOM·결과/로고 후속 변경 — 미공개·로컬 빌드 및 CTest 회귀검증 통과:** 1.9.0.2 기반 로컬 후속 작업입니다. 결과 화면의 레벨은 선곡 로비와 같은 빨간 레벨·키 수별 기호 색을 사용하고, 로고는 제공된 10 모양으로 바꿉니다. 새 일반 BMS 플레이는 #TOTAL과 노트 수를 반영하는 LR2 기반 회복·피해량을 사용하며, 채보의 #RANDOM 분기는 레인 랜덤과 독립된 시드로 선택합니다. 옛 리플레이는 기존 동작을 보존합니다. 이전 볼륨 통합 변경을 포함하며 공개 1.9.0.2·고정 기준 1.9.0·버전 번호는 그대로입니다. [수식·분기·호환성 설계와 검증 범위](docs/bms-total-random-followup.en.md).

**2026-10-15 · BMS 볼륨 통합 후속 변경 — 미공개·로컬 빌드 및 CTest 회귀검증 통과:** 1.9.0.2 기반 로컬 작업에서 반주·키음 개별 조절을 없애고 기존 BMS 음량을 **BMS 연주 볼륨** 하나로 통합합니다. 새 기본값은 20%이며 저장값은 유지합니다. 마스터 10%·텐리프 음악 100% 기본값, 남은 음량의 1%p 조절, 로비 미리듣기 ×0.75를 유지합니다. 버전 번호와 고정 기준 1.9.0은 바꾸지 않습니다. 아래 날짜별 기록은 당시 상태입니다. [설정·호환 안내](docs/config.md#audio).

이번 미공개 후속 변경은 난이도표를 불러오는 동안 상태를 표시하고, debounce UI를 1ms 단위·최대 20ms(기본 0ms)로 조절합니다. 과거 5→8ms 자동 보정은 폐기하여 저장한 5ms와 8ms를 그대로 유지합니다. 스킨의 한국어 표시명은 `LN 끝 줄임`이며 모양·판정 동작은 같습니다. 일반 싱글플레이에서 끝까지 듣기가 켜져 있고 마지막 노트·LN 끝 처리가 완료되면 `Esc`로 일시정지 창 없이 후주를 건너뛰어 결과로 이동합니다.

**2026-10-15 · 1.9.0.2 릴리스 후보 — 공개 검증 절차:** 공개 1.9.0.1을 바탕으로 Options 초록색, 기본 제공 5K Aery 표에서만 초록 ⑤ 표시·등록 4/6/8/10키 표 기호 색상·레벨 빨강·점수 흰색, `lv:` 레벨 검색, 새 debounce 기본값 0ms와 오디오 음량 1% 단위를 적용합니다. 선곡의 정렬·필터·난이도표 선택기를 Song Library 옆 상단으로 옮기고 앨범 표지 높이를 늘립니다. 마스터 기본 10%와 고정 기준 1.9.0을 유지합니다. [변경·사용 안내](docs/feedback-1.9.0.2.ko.md). 실제 공개 여부와 다운로드는 릴리스 페이지가 기준입니다. [릴리스 범위·검증](docs/release-1.9.0.2-gate.md).

**2026-10-15 · 1.9.0.1:** 마스터 기본 음량 10%, 로비 아이콘 중심 정렬을 포함합니다. 미리듣기 ×0.75, 프로필에서 지정하는 F5/F6 -10/+10, Xbox/XInput 버튼 입력, 파일 선택창 중복 방지, BMS BPM/STOP 순서 수정과 옛 리플레이 호환을 제공합니다. 고정 기준은 1.9.0을 유지합니다. [변경 안내](docs/feedback-20261015.ko.md) · [릴리스 검증 범위](docs/release-1.9.0.1-gate.md).

**2026-10-14 · 1.9.0 / 새 고정 기준선:** 사용자가 검토한 로컬 후보를 정식 배포 버전으로 올립니다. BMS/텐리프 음악 음량 분리, 로비·필터·스킨 조작, 등급·SUBARTIST 표시, 한글 모드와 EX 판정, 업로드 상태 표시를 포함합니다. 기본/Hard는 R5, EX는 R6이며 R1–R5 기록·기존 설정을 보존합니다. [기준선](docs/baseline-1.9.0.md) · [릴리스 검증 범위](docs/release-1.9.0-gate.md). 아래 날짜별 항목은 당시 릴리스 이력입니다.

**1.9.0 릴리스:** BMS/메뉴 음악 개별 음량, 더 조용한 메뉴 음악, 묶음 로비, 난이도 범위·정렬 수정, 인디케이터 크기·심플 사각 노트·사용자 폰트, EX/빠름/고스트 끔 기본값. 기존 저장값과 기존 저장값은 유지하며 기준선은 1.9.0으로 올립니다. [변경과 검증 범위](docs/controls-1.9.0.ko.md).

**2026-10-14 · 1.8.9.7:** 입력 스레드에 MMCSS Games/High와 전원 스로틀링 해제를 적용하고, 로비 메뉴 테두리를 아이콘 색의 은은한 glow로 표시합니다. 텐리프 자체 메뉴·결과 음악만 기존의 40%로 낮추며 BMS 곡·키음·미리듣기는 원래 음량을 유지합니다. 고정 기준선은 1.8.7이며 물리 입력 지연 개선은 아직 입증되지 않았습니다. [Scope / 검증](docs/release-1.8.9.7-gate.md).

**2026-10-13 · 1.8.9.6:** 입력과 게임 진행/HUD의 짧은 대기에 재사용 고해상도 타이머를 적용하고, RawInput은 메시지가 오면 깨어납니다. B 비교 방식의 제품 적용이며, 수치 판정 창·R1~R5·리플레이·고정 기준 1.8.7을 유지합니다. 실제 체감은 회차마다 달라 모든 움찔의 해결을 보장하지 않습니다. [Scope / 검증](docs/release-1.8.9.6-gate.md).

**2026-10-13 · 1.8.9.5:** 오디오 버퍼에 따라 달라지던 LN·자동 판정 순서를 R5에서 시간순으로 통일하고, 입력·화면 시간과 BPM/STOP 경계의 노트 위치를 맞춥니다. 로비는 어두운 카드와 색상 아이콘으로, 오디오는 마스터 음량을 맨 위로 정리합니다. R1~R4 기록과 고정 기준 1.8.7은 보존합니다. [변경·검증 범위](docs/release-1.8.9.5-gate.md).

**2026-10-13 · 1.8.9.4:** 결과의 큰 등급·점수 배치와 상세 정확도를 정리하고 Gauge Shift CLEAR 4단계 색을 구분합니다. 오디오의 BGM·키음 음량을 맨 위로 옮기며 새 기본값은 모두 100%입니다. 한국어 스킨 명칭, 명시된 obj 채보자와 BPM! 변속 표시, 같은 시점의 동일 음원 중복 재생 보정을 포함합니다. 채보자 업로드는 선택 사항이며 기존 기록 식별자는 유지합니다. [변경·호환·검증 범위](docs/release-1.8.9.4-gate.md). 검증과 게임·웹 공개 상태는 해당 문서에 별도로 기록하며 고정 기준은 1.8.7입니다.

**2026-10-13 · 1.8.9.3:** 리절트 점수·등급·정확도와 판정 개수의 크기를 조정하고, 로비 동작 카드 색·은은한 Bloom, 스킨 레인 색을 따르는 키 설정, 곡 목록 레벨 색을 개선했습니다. 인디케이터 배경과 스킨의 대상 간격·비주얼 프리셋 항목을 제거하고 대상 레인을 노트 색상 바로 위로 옮겼습니다. [변경·검증 범위](docs/release-1.8.9.3-gate.md). 고정 기준은 1.8.7이며 1.8.9.2의 BMS 보정과 리플레이 호환성을 유지합니다.

**2026-10-13 · 1.8.9.2:** BMS의 같은 레인·시각에 중복된 일반 노트와 롱노트 시작점을 게임 안에서 롱노트 하나로 보정합니다. 원본 파일, LN 길이와 키음을 보존하며 Native와 변환에 함께 적용합니다. 이전 리플레이는 당시 배치를 유지합니다. [동작·호환 한계](docs/release-1.8.9.2-gate.md). 고정 기준선은 1.8.7입니다.

**2026-10-13 · 1.8.9.1:** 연속 키 설정, 전 변환기 키음·롱노트 보존, FAST/SLOW·타이밍 배경 토글, 게이지 확대, 미리보기 기어 드래그와 판정 테두리·배경 제거를 제공합니다. 기존 리플레이는 원래 변환으로 재생합니다. [동작·검증·호환 한계](docs/release-1.8.9.1-gate.md). 고정 기준선은 1.8.7입니다.

**고정 기준선: [1.9.0](docs/baseline-1.9.0.md)** (2026-10-14 사용자 요청). Git tag `1.9.0`을 기준으로 하며 `main`이나 `latest`를 따라 자동 변경하지 않습니다. 1.8.7 기준선과 기존 릴리스는 이력으로 보존합니다.

**2026-10-12 · 1.8.9:** 로비의 큰 BEST 숫자·키 모드 색·흰 통합 외곽선, 오른쪽 위 정보·왼쪽 아래 점수·오른쪽 아래 가로 riff map의 Studio HUD, 큰 결과 글자·분석 탭·중앙 정렬한 조작을 제공합니다. S+ 색·글꼴을 유지하고, 메뉴의 Judge Easy/Hard가 새 세션에서 사라지던 전달 경로를 수정했습니다. [동작·검증·남은 범위](docs/release-1.8.9-gate.md), [Judge 원인과 호환성](docs/judge-session-handoff-1.8.9.ko.md)를 참고하세요.

**2026-10-12 · 1.8.8:** 결과 분석, 자유 해상도와 F9 PNG 안내, 긴 LN Mix에 더해 화면 가독성·난이도표 입력/필터·스킨 레인 단축키·키 수 축소 시 키음 보존·단독 시작/즉시 재시작 조작을 정리했습니다. 현재 검증·패키지·공개 상태는 [1.8.8 릴리스 안내](docs/release-1.8.8-gate.md)에서 확인하세요. 고정 기준은 1.8.7을 유지합니다.

**분석과 확장 계획:** 레인별 분포·시간대 정확도·단노트/LN 시작·해제·유지 실패를 구분하고 측정 불가 미스를 0ms 표본에 넣지 않습니다. 긴 LN Mix v2는 로컬 기록·리플레이만 허용하며 기존 BEST·Sites 업로드에서 제외합니다. 플러그인 실행 환경/API/SDK는 후속 계획이며 1.9.0에는 포함하지 않습니다. [분석 기준](docs/result-analysis-1.8.8.ko.md), [화면·메뉴·키음 변경](docs/release-feedback-1.8.8.ko.md), [시작·재시작 조작](docs/gameplay-start-controls.ko.md).

**2026-10-09 · TenRiff 1.8.7:** 입력 보정과 로딩 오디오 경로를 보완하고 LR2 단독 플레이 스튜디오 덱, 750ms FAST/SLOW 유지, 키 재할당, 옵션 통합과 곡 제목 자동 맞춤을 제공합니다. [1.8.7 release](docs/release-1.8.7-gate.md).


**2026-10-08 · TenRiff 1.8.6:** Native 단독 플레이에 스튜디오 덱 HUD를 추가합니다. 좌우 Shift를 플레이 키로 사용할 수 있으며, F5/F6은 배속 절반/두 배, F11은 채팅입니다. 클래식 HUD도 선택할 수 있습니다. [1.8.6 release](docs/release-1.8.6-gate.md).

**2026-10-06 · TenRiff 1.8.5:** VSync OFF 플레이에서 지원되는 노트 이미지를 순서대로 묶어 그려 CPU의 반복 제출을 줄입니다. 호환되지 않는 스킨·롱노트와 VSync ON은 기존 경로를 유지합니다. 로비·메뉴 및 R1~R4 판정·리플레이 규칙은 유지합니다. [1.8.5 release](docs/release-1.8.5-gate.md).

**2026-10-05 · TenRiff 1.8.42:** 고밀도 구간에서 프레임 예산을 조금 넘었을 때 추가로 쉬던 렌더 제한기를 수정하고, 성능 오버레이 OFF 상태의 통계 수집·정렬을 생략합니다. 노트 화질·표시량과 R1~R4 판정·리플레이 규칙은 유지합니다. [1.8.42 release](docs/release-1.8.42-gate.md).

**2026-10-05 · TenRiff 1.8.41:** Judge Hard의 EASY 기준 PG/GR/GD를 ±17.5/55.714286/98.571429ms로 조정합니다. BAD는 모든 BMS RANK에서 Hard ±225ms·Easy ±210ms로 고정합니다. 새 기록은 R4이며 R1/R2/R3 리플레이와 기존 웹 기록을 보존합니다. [1.8.41 release](docs/release-1.8.41-gate.md).

**2026-10-05 · TenRiff 1.8.4:** 일시정지 마우스 조작과 설정 클릭을 개선하고, 실시간 페이스메이커·연습용 오토스크래치·전체 설정 이전을 추가합니다. 고스트의 플레이어 필드 크기, LN 불투명도와 HUD 검색·프레임 대기열 제한을 수정합니다. [1.8.4 release](docs/release-1.8.4-gate.md).

**2026-10-04 · TenRiff 1.8.3:** 전체화면 스킨 미리보기·노트 안개·FAST/SLOW 개별 위치 설정과 심플 네모 스킨을 추가합니다. 노멀라이즈 OFF의 숨은 압축과 일시정지 재개 표시를 수정하고, 곡 종료 방식 선택·즉시 LN 해제 판정·BMS RANK별 R3 판정을 적용합니다. [1.8.3 release](docs/release-1.8.3-gate.md).

**2026-10-03 · TenRiff 1.8.2:** 옵션 아이콘 10색, 아이콘 색을 따르는 부드러운 선택 테두리, FAST/SLOW 막대 표시 수정과 스킨 에디터 미리보기를 제공합니다. [1.8.2 release](docs/release-1.8.2-gate.md).

**2026-10-03 · TenRiff 1.8.1:** 기능별 아이콘 18종과 PNG 스킨을 추가하고 옵션·곡 선택 로비를 검정과 중립색으로 정리했습니다. 항목별 도움말, 기록 창의 GPT 사이트 순위, 좌우로 4K~16K를 고르는 키 테스트를 제공합니다. [1.8.1 design notes](docs/menu-studio-1.8.1.ko.md).

**멀티 Rate 지원 빌드:** 로비 `대전 Rate`에서 리더가 공통 배속을 설정합니다. 참가자 모두 이 protocol v6 빌드를 사용해야 합니다. [사용 방법과 호환성](docs/multiplayer-rate-build.ko.md).

공개 여부·다운로드: [**1.9.2 릴리스 페이지**](https://github.com/10kseason/TenRiff/releases/tag/1.9.2). 공개 상태와 파일은 해당 페이지를 기준으로 확인합니다. [검증 절차](docs/release-1.9.2-gate.md)를 확인하세요. 게스트·출처 없는 과거 기록은 로컬 전용이며, 온라인에서 새로 시작한 플레이만 기존 지원 조건에 따라 제출합니다. 서버·DB 변경은 포함하지 않습니다.

Language: Korean | [English](README.en.md) | [简体中文](README.zh-CN.md) | [日本語](README.ja.md)

TenRiff는 Windows GUI 기반 BMS 리듬게임 런타임/런처 프로젝트입니다. 고정 기준은 `1.9.0`, 이 소스의 버전은 `1.9.2`이며, 차트 입력은 BMS 계열(`.bms/.bme/.bml/.pms`) 전용입니다. Graphics Settings에서 권리 정리된 외부 ONNX 모델을 선택해 BGA/BGI 확대에 사용할 수 있습니다. 공개 패키지에는 BGA 업스케일러 모델을 넣지 않으며, 키 모드 변환용 NK3 P64 결정 모델과 일반화 패턴 MLP만 포함합니다. MIT 라이선스를 사용하며, 번들된 서드파티 고지는 [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)에 정리합니다.

이 README는 "프로젝트를 처음 열었을 때 무엇을 보면 되는지"를 설명하는 입문 문서입니다. 더 자세한 현재 동작, `1.9.2` 릴리스와 고정 `1.9.0` 기준, 설정 구조, 설계 문서는 [`docs/README.md`](docs/README.md)부터 이어서 읽으세요.

TenRiff 코드는 전통적인 장기 설계 문서 중심 개발만으로 쌓인 프로젝트가 아니라, 빠른 반복과 실험을 중시한 `vibe coding` 성격이 강한 작품이라는 점을 명시합니다.

## 1.8.1 Studio 메뉴 스킨

기능별 아이콘 18종과 PNG 스킨을 추가하고 옵션·곡 선택 로비를 검정과 중립색으로 정리했습니다. 항목별 도움말, 기록 창의 GPT 사이트 순위, 좌우로 4K~16K를 고르는 키 테스트를 제공합니다.

[Design / skin controls](docs/menu-studio-1.8.1.ko.md) · [Release verification](docs/release-1.8.1-gate.md) · [CHANGELOG](CHANGELOG.md)

## 스크린샷

### 홈

플레이, 멀티플레이, 옵션을 한 화면에서 시작합니다.

![TenRiff 홈 화면](docs/images/studio-home.png)

### 선곡

곡 목록, 내 최고 기록, 난이도표 설정을 함께 확인합니다.

![TenRiff 선곡 화면과 난이도표 설정](docs/images/studio-song-select.png)

### 플레이 결과

점수와 판정 통계, 타이밍 분포 추정, 게이지 변화를 표시합니다.

![TenRiff 플레이 결과와 타이밍·게이지 분석](docs/images/studio-result.png)

### 스킨 설정

레인과 노트 구성을 실시간 미리보기로 확인하며 조절합니다.

![TenRiff 스킨 설정과 실시간 미리보기](docs/images/screenshots/skin-settings.png)

## 프로젝트 한눈에 보기

- 기본 타깃 플랫폼: Windows
- 지원 차트: BMS 계열(`.bms/.bme/.bml/.pms`) 전용
- 그래픽 경로: D3D11 + Direct2D/DirectWrite
- 오디오 경로: WASAPI(기본), 네이티브 ASIO(Windows x64 드라이버 필요)
- 입력 경로: RawInput 또는 고주사율 polling + Xbox/XInput 디지털 버튼·트리거
- 직접 IP/헤드리스 서버 멀티플레이: 고정 코디네이터 기반 최대 8인 TCP 대전, BMS 전용 공통곡과 `F8` 채팅(기본 `27301/TCP`, [사용 안내](docs/multiplayer.md))
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
  - 일반 gameplay 로딩 중 `Esc` 취소; 세션 믹스는 Esc 무시
  - display offset
  - performance overlay
  - note head/tail bitmap cache + static playfield command-list cache
  - 고스트 배틀은 새 설정에서 기본 `OFF`이며 사용자가 켠 기존 프로필은 유지
- 옵션 / 스킨
  - Hi-Speed, Rate, EX-Hard/Hard/Normal/Easy gauge, audio, input, graphics 설정
  - `Skins` 화면에서 판정선 위치, 노트 가로/세로 크기
  - `4K–10K / 12K / 14K / 16K / 7+1` lane color 편집 + 실시간 프리뷰
  - native 벡터, TenRiff `skin.json`, LR2 playskin 지원
  - 배포판의 `skins/` 완성 스킨을 기본 목록에 표시하며 같은 이름의 프로필 스킨을 우선 사용
  - LR2 스킨 폴더 선택/drag-and-drop으로 활성 프로필에 복사하고 note/LN/lane-gap/destination-size 정보를 반영
- 결과 / 로컬·읽기 전용 온라인 기록
  - 결과 화면
  - replay/result JSON export
  - 곡별 로컬 기록 누적
  - RECORDS에서 `Tab`으로 로컬 / GPT 사이트 웹 순위 / 기존 검증 서버 전환. 웹 순위는 좌우로 조건 선택, F5로 새로고침하며 기존 서버는 정확히 일치하는 BMS SHA-256의 `online_verified` 기록을 조회
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

1.9.0 공식 Windows 빌드와 ZIP에는 standalone BMS key converter CLI/GUI를 빌드하거나 포함하지 않습니다. 게임 안의 Mode Settings에서 `NK3`를 선택하면 P64와 host beam 안전 솔버가 항상 적용됩니다. 10K가 아닌 원본을 10K로 변환할 때만 일반화 패턴 MLP를 추가하며, 10→10과 나머지 모든 변환은 P64만 사용합니다. 기본 `AUTO` 백엔드는 ncnn Vulkan으로 P64와 MLP를 AMD/NVIDIA GPU에서 실행합니다. `TENRIFF_NK3_BACKEND=AUTO|VULKAN|NCNN_CPU|OPENVINO`와 `TENRIFF_NK3_VULKAN_DEVICE=<index>`로 실행 경로를 선택할 수 있습니다. `NCNN_CPU`는 GPU를 공유하지 않는 서버 검증기처럼 재현성이 필요한 격리 환경용입니다.

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
3. [`docs/baseline-1.9.0.md`](docs/baseline-1.9.0.md)
   - 후속 작업이 기준으로 삼아야 하는 `1.9.0 fixed stable baseline` 베이스 문서
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
