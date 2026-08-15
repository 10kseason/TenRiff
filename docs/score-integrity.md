# 점수 무결성과 Replay 검증

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
