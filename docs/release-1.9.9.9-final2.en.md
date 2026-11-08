# TenRiff 1.9.9.9-final2

Remove keysound-duration/silence/required-analysis/chart-blacklist IR exclusions and badges that caused false positives. Key overlay supports every4–16K layout and25–100% whole-panel size. Production Site v28 uses the same removal. Existing ranking eligibility, scoring, judgement, replay formats and fixed baseline1.9.6 remain.

## Key overlay

Skin → Judgement / HUD → Key Overlay Plugin (4~16K). Default OFF. Key Overlay Size:25–100% in1% steps, default100%. X/Y keep the scaled panel inside the canvas. Size persists in settings, profiles and portable skin presets, including both previews. Only key viewing and1-second KPS are shown.

## IR

Keysound length, silence ratio, audio scan availability, scan version and chart title/hash blacklist no longer exclude scores. The client stops selected-song/result audio eligibility scans and does not send scan evidence. Existing native-key, autoplay/practice/aborted/pause, supported judgement/mod and account/record validation policies remain. The web ignores retired scan fields for older measured clients while preserving all other strict record validation.

Previously hidden Speed of Link records become visible again; stored rows are preserved. No database migration, record deletion, replay-format change or new judgement generation. Production ranking is client submitted, without server replay/audio re-execution.

Extract the official ZIP into a new folder and run launch_win.bat. Keep prior installations/profiles. Physical play/listening, long sessions, arbitrary skins and other PCs remain separate checks. [Release verification gates](release-1.9.9.9-final2-gate.md).
