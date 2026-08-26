# TenRiff 랭킹 신뢰 및 온라인 기록 단계 계획

상태: **독립 서버의 계정·challenge·업로드·재실행·DB 기반 구현 완료, 운영 shadow 검증과 계정 복구는 배포 전 게이트**

온라인 기록은 기능 수보다 신뢰 경계를 먼저 만든다. 클라이언트가 보낸 최종 점수나 판정 수를
그대로 믿지 않고, 등록된 차트와 replay evidence를 서버의 동일 규칙 엔진으로 다시 실행해 얻은
결과만 공개 랭킹으로 승격한다.

## 고정 정책

- 공개 랭킹 대상은 BMS 계열 `.bms/.bme/.bml/.pms`의 승인된 차트 SHA-256만 허용한다.
- `.osu` 차트, osu import/parser 경로, osu!mania 규칙·ScoreV1·OD8 점수를 랭킹 기준으로 선택한 실행,
  또는 향후 추가될 osu 파생 변환은 모두 `ranked_ineligible_osu`로 온라인 등록을 거부한다.
- BMS 플레이 뒤 로컬 결과에 보조 OD8 통계가 함께 계산되는 것만으로는 부적격이 되지 않는다.
  서버에 제출하는 권위 규칙과 점수는 TenRiff native BMS 규칙이어야 한다.
- 스킨과 배경 같은 순수 시각 요소는 판정 결과에 영향을 주지 않으므로 랭크 자격과 분리한다.
- autoplay, practice/no-fail, replay 재생, 중단된 플레이, 허용 목록 밖의 규칙·모드·배율은 이유 코드를
  붙여 등록하지 않는다. 허용 mod와 rate 범위는 서버 공개 전에 버전된 규칙표로 고정한다.
- 서버는 클라이언트의 `final_score`, 판정 수, clear claim을 신뢰하지 않는다.

## 기록 상태 모델

- `local_unverified`: legacy/custom/불완전 evidence 기록
- `local_verified`: 현재 클라이언트에서 exact chart SHA-256과 replay v3를 재실행해 일치한 기록
- `ranked_ineligible`: 정책상 온라인 제출 불가, 이유 코드 표시
- `online_pending`: 서버 shadow 검증 대기
- `online_verified`: 서버 재실행과 정책 검사를 통과한 공개 가능 기록
- `online_rejected`: 해시·규칙·재실행·정책 불일치, 사용자에게 이유 코드 표시

로컬 기록은 삭제하지 않는다. 온라인 자격과 무관하게 히스토리에 남기되 상태를 숨기지 않는다.

## 단계 0 — 공통 자격 판정부터 고정

1. 클라이언트와 서버가 공유할 `RankedEligibility` 결과와 안정적인 reason code를 정의한다.
2. 입력은 chart format/SHA-256, replay format, ruleset ID, rate, mods, assist, pause/abort 상태,
   클라이언트·프로토콜 버전으로 제한한다.
3. 같은 입력은 UI, 로컬 저장, 업로드 준비, 서버 검증에서 항상 같은 자격 결과를 내야 한다.
4. reason-code 전체 조합, golden replay, 변조 replay, 오래된 replay migration을 단위 테스트한다.

이 단계가 끝나기 전에는 업로드 API를 열지 않는다.

## 단계 1 — 기록 보기 화면

- Song Select의 현재 최고 기록 카드와 기존 기록 목록 위에 전용 `Records` 화면을 추가한다.
- Local 탭은 곡/키 수/규칙/mod/rate/클리어/검증 상태 필터와 상세 replay 열기를 제공한다.
- `verified`, `unverified`, `ranked ineligible`을 색과 텍스트로 함께 표시하고 이유를 볼 수 있게 한다.
- `Online Records` 탭은 이 단계에서는 비활성 또는 준비 중으로 보이며 로컬 기록과 섞지 않는다.

## 단계 2 — 읽기 전용 서버와 온라인 기록 보기

- TLS 연결, API 버전, 서버 시간, 지원 replay/ruleset 버전, 점검 상태를 먼저 handshake한다.
- 알려진 chart SHA-256에 대한 leaderboard 조회만 제공하고 업로드는 받지 않는다.
- `Online Records`는 내 기록, 전체 상위 기록, 주변 순위, 검증 버전과 상태를 표시한다.
- 서버 장애·버전 불일치는 로컬 플레이를 막지 않으며 온라인 기능만 fail-closed로 비활성화한다.

현재 구현은 별도 공개 TenRiff Server의 schema-v1 API, Caddy HTTPS 배포 구성, 승인 BMS
SHA-256 catalog, 계정/만료 bearer session, 일회성 challenge, replay 업로드, 외부 verifier 재실행,
SQLite 기록과 HMAC 영수증까지 포함한다. 클라이언트는 Local/Online 전환과 차트별 공개 상위 기록을
표시한다. 서버 시간/지원 replay 버전 handshake, 내 기록·주변 순위, 계정 복구 UI는 아직 남아 있다.

## 단계 3 — shadow 제출과 서버 재실행

- 공개 순위에 반영하지 않는 shadow endpoint로 replay evidence v3를 받는다.
- 제출에는 chart SHA-256, ruleset ID/version, mod/rate, replay SHA-256, idempotency key를 포함한다.
- 서버는 승인된 차트 바이트/정규화 결과를 사용해 headless 엔진으로 입력 trace를 재실행한다.
- 서버 계산 점수·판정·clear와 제출 메타데이터가 다르면 거부하고 감사 로그와 reason code를 남긴다.
- nonce, 크기 제한, 이벤트 수 제한, rate limit, 중복 replay 탐지를 적용한다.
- 충분한 기간 동안 오탐·규칙 버전 차이·서버 비용을 측정한 뒤에만 공개 단계로 넘어간다.

서버의 해당 코드 경로와 실제 별도 프로세스 재실행 테스트는 구현되어 있다. 운영자는 공개
`online_verified` 승격 전 동일 endpoint를 비공개 catalog와 shadow 환경에서 운용해 golden replay,
부하, 오탐과 ruleset drift를 측정해야 한다.

## 단계 4 — 공개 랭킹 등록

- 계정 인증과 복구, 제출 idempotency, 개인정보 최소 수집 정책을 먼저 확정한다.
- `online_verified`만 leaderboard에 표시하고 pending/rejected는 개인 기록 화면에서만 보인다.
- 규칙 엔진 버전이 바뀌면 기존 점수를 묵시적으로 재해석하지 않고 시즌 또는 ruleset 버전을 분리한다.
- 클라이언트 서명이나 실행 파일 attestation은 보조 신호일 뿐 신뢰의 기준으로 삼지 않는다.
  최종 기준은 서버의 chart hash 확인과 deterministic replay 재실행이다.

## 단계 5 — 운영 신뢰와 이의 제기

- 검증 엔진 버전, 제출 해시, 판정 결과, 상태 변경을 append-only 감사 로그로 남긴다.
- 비정상 분포는 자동 삭제가 아니라 검토 플래그로 사용하고, 취소·복원 이력을 보존한다.
- 사용자에게 거부 이유와 재검증 요청 경로를 제공한다.
- 서버가 재현할 수 없는 점수는 운영자 판단만으로 verified 처리하지 않는다.

## 구현 전 완료 조건

- 전용 Records 화면과 검증 상태 표시가 로컬 데이터로 먼저 완성됨
- BMS-only ranked eligibility와 osu 관련 reason code 테스트 완료
- replay v3 golden corpus가 Windows Release와 서버 headless 빌드에서 동일 결과를 냄
- 차트 catalog의 SHA-256 등록·갱신·폐기 정책 확정
- API schema, 보존 기간, 개인정보, 계정 복구, 운영 비용 문서화
