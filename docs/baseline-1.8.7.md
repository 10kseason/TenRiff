# TenRiff 1.8.7 Fixed Stable Baseline

Language: Korean | [English](baseline-1.8.7.en.md) | [简体中文](baseline-1.8.7.zh-CN.md) | [日本語](baseline-1.8.7.ja.md)

2026-10-10에 고정한 후속 작업의 기준선은 **1.8.7**이다. 이전 `baseline-*` 문서는 해당 버전의 역사 기록이며, 현재 기준은 이 문서와 [현재 상태](current-state.md), 고정 커밋의 코드·회귀 테스트다. 기준선을 고정한다는 것은 이후 개발을 중단한다는 뜻이 아니라, 비교·호환성 판단의 출발점을 자동으로 옮기지 않는다는 뜻이다.

## Release Identity

- 고정 Git tag: `1.8.7`
- 고정 소스 커밋: `6b060ad47b950ad9e0dbc516fbb40189e8e21347` (PR #81 병합)
- 릴리스 날짜: 2026-10-09. 기준선 문서 고정 날짜: 2026-10-10.
- [버전 지정 릴리스](https://github.com/10kseason/TenRiff/releases/tag/1.8.7). `latest`, 이동하는 브랜치 HEAD, 웹 배포 버전은 이 기준선의 식별자가 아니다.
- 기준 플랫폼: Windows GUI. 제품 차트 범위: BMS 계열(`.bms/.bme/.bml/.pms`).
- 아래는 공개된 기존 자산의 SHA-256이다. 문서 수정은 이 ZIP에 포함되어 있다고 간주하지 않으며, 태그·ZIP·체크섬 파일을 교체하지 않는다.

| 자산 | SHA-256 |
|---|---|
| `TenRiff-1.8.7.zip` | `270ace26dd912228083314ccc99bb27f4e6fd1e3d617f45cffc3f82bb46d3476` |
| `TenRiff-1.8.7-source.zip` | `4c22c89827e6c460ed951c178c0371355636ee6dd2836dece52b028aeeca5ad1` |
| `TenRiff-1.8.7-SHA256SUMS.txt` | `23ff5a092690fb40b5c486eda931db47dc034f5f2328de14ca9987e26a11be99` |

## Product And Packaging Contract

- 기본 흐름은 `Title -> Song Select -> Gameplay -> Result`다. 로컬 기록은 온라인 서비스 실패와 무관하게 보존한다.
- BMS 파싱·샘플 타임라인·가변 스크롤·롱노트·지뢰·키음·결정적 리플레이 재검증을 보존한다. 외부 `.osu` 차트는 현재 제품 범위가 아니다.
- NK3는 P64와 host beam 안전 솔버를 권위 경로로 유지한다. 일반화 패턴 MLP는 10K가 아닌 원본을 10K로 변환할 때만 사용한다.
- native, TenRiff `skin.json`, LR2 스킨을 지원한다. 완성 스킨은 `skins/`, 최소 템플릿은 `examples/skins/TenRiff-Example`에 두고 같은 이름의 프로필 스킨을 번들보다 우선한다.
- 실행 ZIP에는 `Mainmusic/`, 번들 스킨, NK3 런타임·모델을 포함한다. Songs, BGA 업스케일러 모델, standalone BMS key converter 실행 파일은 포함하지 않는다. 외부 업스케일러는 기본 OFF이며 모델 선택만으로 켜지지 않는다.
- 공개 소스는 클라이언트·verifier·빌드 자료다. 서버 구현, 배포 비밀, 계정, 사용자 프로필·로그·리플레이·결과·업로드 키는 배포 범위에서 제외한다. [소스 패키지 경계](../README_SOURCE_PACKAGE.md)를 따른다.

## Input, Audio And Rules Contract

- 입력 시각은 audio playback head 기준이다. 양수 입력 보정은 콜백 진행 위치에 따라 줄어들지 않으며, WASAPI 위치 샘플은 그 위치를 관측한 QPC와 짝지어 사용한다. ASIO fallback을 보존한다.
- 저장한 RawInput/Polling 설정과 런타임 fallback을 구별하고, fallback 결과로 저장 backend를 덮어쓰지 않는다. 좌우 Shift 및 기본·보조 키를 지원하며 같은 키 모드의 재할당은 이전 슬롯에서 키를 이동한다. 명시적으로 비운 기본 키는 저장 후에도 미할당이다.
- 기본 출력은 WASAPI이며 ASIO는 선택 기능이다. 차트 분석·샘플 레이트 선택 후 게임 오디오를 열고 로딩 화면만 60FPS로 제한한다. 플레이가 시작되면 설정된 프레임 동작을 복구한다.
- 라이브 키음은 실제 입력에 즉시 반응한다. 보정된 샘플만 저장하는 리플레이의 키음 시각과 항상 같다고 보장하지 않는다.
- 곡 종료 방식은 `audio_ui.play_to_end=true`(기본, 끝까지 듣기) 또는 `false`(후주 스킵)다. 정상 완료 결과는 추가 키 입력 없이 저장·제출하고, 남은 후주를 건너뛰는 입력은 점수·리플레이를 바꾸지 않는다.
- 새 표준 플레이는 canonical R4를 사용한다. R1/R2/R3 리플레이·고스트·verifier는 각 원래 규칙을 유지한다. BMS RANK와 Judge Mod별 판정, LN 해제, 게이지·점수 계산은 [판정 규칙](judgement-windows.md)을 따른다.
- replay evidence v3의 차트 SHA-256·ruleset·결과 연결, 결정적 입력 재실행과 검증된 로컬 best 경계를 보존한다. 일반 Random의 실제 seed를 기록하고 재실행에 재사용한다.

## Presentation And Controls Contract

- Native/LR2 단독 플레이에 Studio Deck을 제공하고 Classic 선택과 외부 이미지·고스트·멀티의 Classic fallback을 보존한다. 리프 맵 표시, 어두운 HUD 바탕과 전체 곡 제목 자동 맞춤을 유지한다.
- FAST/SLOW는 750ms 유지되며 뒤따르는 PG가 지우지 않는다. 글자·막대 위치는 판정 위치와 독립적이고 막대는 조건부 또는 항상 표시를 선택한다.
- Options는 8개 카드다. Mods는 키 모드 설정, NKRO Test는 키 설정에서 연다. 게임 중 `F5/F6`은 Hi-Speed 절반/두 배, `F7/F8`은 화면 타이밍, `F9`는 캡처, `F10`은 계정, `F11`은 채팅이다. 메뉴의 `F5` 재인덱싱과 게임 중 동작을 구별한다.
- 지원되는 노트 이미지는 VSync OFF에서 원래 순서로 GPU 배칭한다. VSync ON·작은 배치·지원하지 않는 변환/크롭·procedural LN/지뢰 경로는 기존 처리를 유지한다. 노트 표시량·화질·타이밍을 성능을 위해 축소하지 않는다.
- HUD는 표시 계층이다. 화면 변경으로 판정·입력·오디오·리플레이·고스트·게이지·점수 또는 노트 배칭 계약을 바꾸지 않는다.

## Online And Trust Boundaries

- 최대 8인 직접 IP/LAN 멀티플레이와 공통 Rate는 protocol v6의 호환 빌드끼리 사용한다. peer 점수는 `UNVERIFIED CLAIM`이며 서버 권위 검증으로 취급하지 않는다. [멀티플레이](multiplayer.md)와 [Rate 안내](multiplayer-rate-build.ko.md)를 따른다.
- GPT Sites 리더보드는 `client_submitted` 기록이다. 서버가 리플레이를 독립 재실행한 검증 결과라고 표시하지 않는다. 연결·업로드는 사용자의 기존 설정과 동의 경계를 유지한다.
- 자체 호스팅 계정/채팅/랭킹 경로는 Sites와 구별한다. 해당 랭킹의 challenge·정확한 차트 바이트·외부 verifier 재검증 정책을 Sites에도 적용된 보장으로 확대하지 않는다. `TenRiff Server v1.1.0`이라는 과거 호환 표기는 현재 서버 운영이나 전 구간 호환 검증의 증거가 아니다.
- 자체 호스팅 경로의 DPAPI 세션 보호, 마스킹된 비밀번호 입력, 승인 후 채팅 URL 열기와 원격 HTTPS 요구를 보존한다. 서버 운영·인증서·비밀 저장·보존 정책은 별도 운영 책임이다.
- 자체 호스팅 포트·서버 인증 정책은 [서버 계획](ranked-integrity-plan.md)의 운영 경계를 따른다. 공개 클라이언트 소스만으로 서버 구현·실제 운영을 재검증했다고 보지 않으며, 해당 포트 정책을 Sites의 호스팅 설정으로 해석하지 않는다.

## Verification And Limits

다음 수치는 **2026-10-09 릴리스 검증 기록**이다. 문서 고정 작업에서 게임 검사를 다시 실행했다는 뜻이 아니다. [1.8.7 릴리스 검증](release-1.8.7-gate.md)에 확인 범위와 증거 위치를 정리한다.

- Release 및 독립 추출 동등 소스 CTest 각 3/3, 1,045 cases, 통합 생략 0; 에디터 각 33/33.
- 실제 app/session 84/84, 무음 WASAPI 공유 출력 23/23, 두 HUD sprite gate 각 7/7, 네이티브 메뉴 17개/939 hit 검사, 4개 presentation mode 각 180/180 Presents.
- 제목 28개 사례/2,520 측정 Presents, 1.8.5 대비 고정 합성 화면 6회 A/B의 p50 +0.03ms 이하 및 p95/p99 게이트, PR/main ASan·OpenVINO CI 통과.
- ZIP CRC, 클라이언트 573개·소스 1,057개 해시, 소스 1,057개와 병합 Git blob 일치, 개인정보 검출 0, 공개 자산 3개 재다운로드 비교 통과. 최종 ZIP의 소스 트리는 독립 빌드한 후보와 같지만 최종 ZIP 자체를 다시 독립 빌드한 것은 아니다.
- 물리 입력·가청 출력·제보자 기기·Discord·긴 연주·다른 PC·온라인 대전 및 API 내부 선점은 미검증이다. 선택적 외부 Python 참조 비교도 미실행이다.
- 별도 동적 시험의 Classic p50 +0.1939ms와 매우 긴 제목의 작은 글자, 극단적 HUD 배치·모든 LR2 변형·웹 Canvas와 실게임 표현의 차이는 남은 한계다. 기준선 고정은 이 한계를 해결했다는 선언이 아니다.

## Compatibility And Update Rule

- 후속 작업은 위 커밋에서 새 브랜치/작업본으로 시작하고 1.8.7과 비교한다. 다른 버전의 성능 측정은 해당 비교 버전을 명시한다. 기존 릴리스·설치·프로필·실험 작업본을 보존한다.
- 이동하는 `main`/`latest` 또는 새 릴리스만으로 고정 기준선을 자동 변경하지 않는다. 사용자가 기준선 변경을 명시적으로 요청하면 새 버전 문서, 네 언어의 README·문서 인덱스·현재 상태·로드맵을 함께 갱신하고 이전 문서는 이력으로 표시한다.
- replay/score/chart identity, 계정 저장, API/포트/프로토콜 변경은 명시적인 호환성 결정, 필요한 migration과 교차 호환 검증을 동반한다.
- 문서 후속 커밋은 릴리스 소스 커밋과 구별한다. `1.8.7` 태그를 옮기거나 같은 이름의 공개 자산을 재생성·덮어쓰지 않는다.
- 설정은 [config](config.md), 플레이는 [gameplay guide](gameplay-guide.md), 기록 신뢰 경계는 [score integrity](score-integrity.md)와 [ranked integrity](ranked-integrity-plan.md)를 함께 확인한다.
