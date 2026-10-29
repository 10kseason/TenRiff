> **1.9.9.7 current:** [일반 LN 해제는 PG까지만 허용하며 R11/R12 온라인 제출 지원](release-1.9.9.7.ko.md). R9/R10 제출도 별도 조건으로 유지합니다. 아래 수치표는 단노트/머리에 그대로 적용하며 이전 유지 정책 설명은 이력입니다.

# 점수 무결성과 Replay 검증

1.9.6의 새 기본/Hard는 R9, EX는 R10이며 [사용자 고정 판정표](judgement-baseline-20261018.ko.md)를 적용합니다. 과거 R1–R8과 custom은 원래 판정·점수·식별자를 유지합니다. Sites는 정식 설정의 새 Online R9/R10만 받으며 이전 규칙의 새 제출은 차단합니다. 기존 보드·기록·순위는 레거시 읽기 전용으로 보존합니다. Guest·출처 미표시 과거 기록·커스텀·LN Mix v2와 별도 Ranked 검증 서버의 미지원 규칙은 계속 제외합니다. 커스텀 기록은 custom9/10 ID로 비공식 상태를 유지합니다. 대전은 같은 패킷 배치의 protocol v7로 다른 판정창의 v6 상대를 거부합니다. 일반 소스 기준선1.9.0과 이전 공개 릴리스는 보존합니다. 아래1.9.3 설명은 당시 R7/R8 도입 이력이며 현재 범위는 [1.9.6](release-1.9.6.ko.md)을 따릅니다.

1.9.3의 새 기본/Hard는 R7, EX는 R8을 사용합니다. 공푸어의 일반·상세·구간 정확도 기여만 제외하고 체력·POOR 수·원점수·콤보·판정창은 유지합니다. 과거 R1–R6와 custom 기록은 원래 규칙으로 재생하며 결과를 다시 쓰지 않습니다. [공푸어 정책과 호환 범위](empty-poor-accuracy.ko.md). 1.9.0의 R5/R6 도입은 [당시 EX 정책](judge-ex-1.9.0.ko.md)에 보존합니다.

TenRiff의 공식 로컬 베스트는 새 evidence-format replay부터 저장된 점수 숫자를
그대로 믿지 않습니다. 정확한 차트와 규칙을 다시 구성하고 sample-time 입력 trace를
headless 게임 엔진에 재실행해 점수, 판정, 게이지, clear 상태를 다시 계산합니다.

## 새 기록의 증거

- `replay_format_version = 3`
- 원본 차트 바이트의 `chart_sha256`
- 고정 점수 규칙을 나타내는 `ruleset_id`
- result가 전체 replay 파일을 가리키는 `replay_sha256`
- lane/down-up/sample로 구성된 입력 trace

v3 trace는 로드 전에 이벤트 수, lane 범위, sample 범위/정렬, lane별 down-up 전이,
sample rate/rate/multiplier의 유한 범위를 검사합니다. 그 뒤 차트 SHA-256을 확인하고
기록된 random seed, key conversion, mods, rate를 동일하게 적용해 결과를 재산출합니다.

## 기록 상태

- `verified`: 차트·ruleset·trace를 재구성해 결과를 재현함. 저장된 점수/통계가 달라도
  해당 주장은 무시하고 재계산 결과를 사용합니다.
- `legacy-unverified`: 이전 포맷이라 차트/ruleset 결합 증거가 없습니다.
- `custom-ruleset`: 기본 판정창이나 게이지 수치와 다른 사용자 규칙입니다.
- `missing-evidence`: replay 또는 필요한 SHA-256 결합이 없습니다.
- `invalid`: 해시, trace 구조, 모드/배율 또는 재구성 결과가 맞지 않습니다.

모든 상태의 원본 result/replay는 Records 히스토리에 남습니다. 공식 베스트에는
`verified`이면서 Autoplay, Practice, 노트 수 변경 mod, 중단 플레이가 아닌 기록만
들어갑니다. 실패한 정상 플레이도 replay로 재현되면 검증된 실패 기록으로 비교할 수
있습니다.

## 보장 범위

이 구조는 result JSON의 `final_score`, 판정 수, 배율을 바꾸는 조작과 replay/result
연결 바꾸기를 공식 베스트에 반영하지 못하게 합니다. replay trace나 차트가 바뀌면
SHA-256 결합이 깨지고, result와 replay를 함께 편집해도 최종 점수는 trace에서 다시
계산됩니다.

하지만 로컬 프로그램 소유자는 수정된 클라이언트로 완벽한 입력 trace 자체를 합성할
수 있습니다. replay 검증은 “이 입력이 이 차트/규칙에서 이 결과를 만든다”를 확인할
뿐 “사람이 실시간으로 입력했다”를 암호학적으로 증명하지 않습니다.

## 멀티플레이

현재 direct-P2P protocol은 상대의 score claim을 native 점수, 판정 수, combo, gauge
범위 안에서만 수용하고 Result에 `UNVERIFIED CLAIM`으로 표시합니다. round nonce는
다른 라운드 패킷을 막지만 현재 라운드 점수를 증명하지 않습니다. 아직 replay proof
전송이나 서버 서명 영수증은 없으므로 P2P 순위는 신뢰 가능한 지인끼리의 참고 비교입니다.

공개 랭킹을 만들 때는 서버가 일회성 challenge를 발급하고 replay를 같은 verifier로
검증한 뒤 서버 전용 키로 result receipt를 서명해야 합니다. 클라이언트에 비밀키를
넣는 방식은 로컬 사용자가 추출해 다시 서명할 수 있으므로 권위 경계가 되지 않습니다.
