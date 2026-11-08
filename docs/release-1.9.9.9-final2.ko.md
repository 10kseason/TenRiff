# TenRiff 1.9.9.9-final2

오탐을 만들던 키음 길이·무키음·분석 필수·곡별 IR 제외와 표시를 제거했습니다. 키 오버레이는 모든4~16키와25~100% 크기를 지원합니다. 운영 웹v28과 함께 사용하며 기본 랭킹 자격·판정·점수·리플레이 형식과 고정 기준선1.9.6은 유지합니다.

## Key overlay

Skin → Judgement / HUD → Key Overlay Plugin (4~16K). Default OFF. Key Overlay Size:25–100% in1% steps, default100%. X/Y keep the scaled panel inside the canvas. Size persists in settings, profiles and portable skin presets, including both previews. Only key viewing and1-second KPS are shown.

## IR

Keysound length, silence ratio, audio scan availability, scan version and chart title/hash blacklist no longer exclude scores. The client stops selected-song/result audio eligibility scans and does not send scan evidence. Existing native-key, autoplay/practice/aborted/pause, supported judgement/mod and account/record validation policies remain. The web ignores retired scan fields for older measured clients while preserving all other strict record validation.

Previously hidden Speed of Link records become visible again; stored rows are preserved. No database migration, record deletion, replay-format change or new judgement generation. Production ranking is client submitted, without server replay/audio re-execution.

Extract the official ZIP into a new folder and run launch_win.bat. Keep prior installations/profiles. Physical play/listening, long sessions, arbitrary skins and other PCs remain separate checks. [Release verification gates](release-1.9.9.9-final2-gate.md).
