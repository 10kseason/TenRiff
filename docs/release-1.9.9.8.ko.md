# TenRiff 1.9.9.8 — 일반 LN GREAT 허용 핫픽스

일반 LN을 끝보다 일찍 뗄 때 PG뿐 아니라 GREAT 범위까지 성공으로 처리합니다. PG 밖~GR 안은 실제 GREAT이며, GR 경계보다 이르면 GOOD 구간부터 즉시 BAD·콤보 단절·게이지 감소·OD8 MISS입니다. 재누름으로 실패를 취소할 수 없습니다. 끝까지 유지한 자동 PG, CN의 떼기 판정, 단노트·머리 판정창은 유지합니다.

Normal/Hard는 R13, EX는 R14로 저장하고 온라인 제출을 지원합니다. 기존 R9~R12 제출은 별도 순위로 보존하며 R11/R12 리플레이는 원래 PG 기준으로 재생합니다. BEST 세대4와 peer v9로 판정이 다른 기록·대전을 구분합니다. JSON 필드 추가나 DB 이관은 없습니다.

Sites: client_submitted; no server replay re-verification. Existing eligibility exclusions remain. Fixed baseline: 1.9.6. Physical feel, long sessions and real two-PC play are unverified.

[Verification](release-1.9.9.8-gate.md) · [Downloads](https://github.com/10kseason/TenRiff/releases/tag/1.9.9.8)
