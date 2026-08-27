# TenRiff 1.5.1 Fixed Stable Baseline

이 문서는 `1.5.1` 릴리스에서 고정한 TenRiff의 안정 기준선이다. 이후 작업은 명시적인 호환성 변경과 migration 없이 아래 계약을 깨지 않는다. 오래된 기준선 문서는 역사 기록으로 유지하되 현재 판단에는 이 문서와 실제 코드, `docs/current-state.md`를 우선한다.

## Release Identity

- 고정 기준 릴리스 라인: `1.5.1`
- 릴리스 명칭: `fixed stable baseline`
- 기준 플랫폼: Windows GUI
- 제품 표면: BMS family 전용(`.bms/.bme/.bml/.pms`)
- 호환 자체 호스팅 서버: `TenRiff Server v1.1.0`
- 정식 자산: 실행 ZIP, 공개 소스 ZIP, SHA-256 목록

## Product Contract

- 기본 흐름은 `Title -> Song Select -> Gameplay -> Result`이며 로컬 기록은 온라인 서비스 실패와 무관하게 보존한다.
- BMS parser, timing, long note, landmine, keysound, replay v3와 결정적 재검증 동작은 기준선의 일부다.
- NK3는 P64와 host beam 안전 솔버를 권위 경로로 유지하고, 일반화 MLP는 10K가 아닌 원본을 10K로 변환할 때만 사용한다.
- 완성 TenRiff 스킨은 `skins/`, 최소 제작 템플릿은 `examples/skins/TenRiff-Example`에 둔다. 사용자 프로필의 같은 이름 스킨이 번들보다 우선한다.
- Windows 배포는 `Mainmusic/`, 번들 스킨, NK3 런타임과 모델을 포함하고 Songs 및 standalone BMS key converter는 포함하지 않는다.

## Input And Runtime Contract

- gameplay 입력 타임라인은 audio playback head를 기준으로 한다.
- 저장된 RawInput 설정을 우선하되 bound-key polling shadow와 Polling fallback으로 live capture를 유지한다.
- runtime fallback 결과로 사용자의 저장 backend 설정을 덮어쓰지 않는다.
- 마지막 판정 뒤 곡 오디오가 남아 있으면 재생을 유지하며, 이때 레인 키 입력은 점수나 replay를 바꾸지 않고 결과로 건너뛴다.
- 일반 Random은 플레이마다 새 seed를 만들고 replay에 실제 seed를 기록한다. 서버 재검증은 동일 seed와 규칙을 사용한다.

## Account And Global Chat Contract

- `F10`은 어느 메뉴에서도 계정 창을 열고, 로그인 전에 메인 서버 또는 사용자 지정 사설 API 서버를 선택하게 한다.
- 회원 비밀번호는 평문으로 저장하지 않는다. 서버는 고유 salt와 PBKDF2-HMAC-SHA256 600,000회로 저장하며 bearer token도 원문으로 저장하지 않는다.
- 클라이언트에 유지하는 로그인 세션은 Windows DPAPI로 보호한다. 계정/비밀번호 찾기는 이 기준선의 필수 기능이 아니다.
- 로그인 비밀번호 칸은 `Ctrl+V`를 지원하고, 붙여넣은 비밀번호의 앞뒤 공백을 보존하며 화면에는 마스킹해서 표시한다.
- `F8`은 메뉴, 플레이, 결과에서 같은 서버 단위 글로벌 채팅 오버레이를 하단에 연다. 채팅 입력 중 gameplay/menu 명령과 충돌하지 않는다.
- `/np`는 현재 재생 중인 곡명과 작곡가를 보낸다.
- 채팅 URL은 즉시 열지 않고 주소를 표시하는 경고 창에서 사용자가 명시적으로 승인한 뒤 연다.
- 로그인 상태의 멀티플레이 방 검색은 계정에서 선택한 메인/사설 서버의 인증된 방 목록을 사용한다. 로그아웃 상태에서는 LAN 검색을 유지한다.

## Ranked BMS Contract

- 대량 song indexing은 로컬 검색용이며 서버 랭킹 catalog를 자동으로 채우지 않는다.
- 새 BMS는 실제 기록 제출 시 SHA-256으로 등록 후보가 되고, challenge에 묶인 replay 재검증을 통과한 기록만 리더보드에 들어간다.
- `.osu` 및 osu 규칙 파생 제출, autoplay/assist, 불완전하거나 재현 불가능한 replay는 fail-closed로 거부한다.
- 관리자는 SHA-256 제외 목록으로 BMS 등록 및 리더보드 노출을 차단할 수 있다.
- 클라이언트 점수 claim은 신뢰하지 않고 서버가 정확한 chart bytes와 replay를 외부 verifier로 재실행한 결과를 사용한다.

## Network And Security Contract

- 게임 서버 기본값: `27301/TCP`
- 로컬 origin API: `127.0.0.1:27302/TCP`이며 LAN/WAN에 직접 공개하지 않는다.
- 공개 HTTPS 게이트웨이: `27303/TCP`; HTTP/3를 쓰면 `27303/UDP`도 연다.
- `27304~27305`는 예약하고 `27300`, `80`, `443` host 공개 매핑은 기본 구성에서 사용하지 않는다.
- 원격 계정, 채팅, 랭킹은 신뢰 가능한 HTTPS 인증서를 요구한다. HTTP는 loopback 개발 주소에서만 허용한다.
- 실제 운영 도메인, 인증서, 비밀 저장소, 백업, 보존/삭제 정책은 서버 운영자가 책임지고 중앙 공식 서버 표시는 별도 운영 게이트를 통과해야 한다.

## Compatibility Rule

- 이후 변경은 기본적으로 `1.5.1 fixed stable baseline`에 가산형으로 쌓는다.
- replay, score, chart identity, 계정 저장, API 또는 네트워크 포트를 바꾸면 버전된 migration과 교차 호환 테스트를 함께 추가한다.
- 릴리스 규칙이 바뀌면 이 문서, `docs/current-state.md`, `docs/config.md`, changelog를 같이 갱신한다.
- 기준선의 불변 식별자는 Git tag `1.5.1`이며 태그와 공개 릴리스 자산은 게시 후 교체하지 않는다.

## Companion Docs

- 현재 구현: `docs/current-state.md`
- 설정/프로필: `docs/config.md`
- 플레이 흐름: `docs/gameplay-guide.md`
- 멀티플레이/포트: `docs/multiplayer.md`
- 랭킹 신뢰 경계: `docs/ranked-integrity-plan.md`, `docs/score-integrity.md`
- 릴리스 검증: `docs/release-1.5.1-gate.md`
