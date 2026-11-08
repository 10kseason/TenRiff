# TenRiff 1.9.9.9-final2

誤判定の原因だったキー音の長さ・無音率・必須解析・曲別IR除外と表示を撤去しました。キーオーバーレイは4～16Kの全配置と25～100%のサイズに対応します。運用Web v28も同じ変更です。基本のランキング資格・判定・得点・リプレイ形式と固定基準1.9.6は維持します。

## Key overlay

Skin → Judgement / HUD → Key Overlay Plugin (4~16K). Default OFF. Key Overlay Size:25–100% in1% steps, default100%. X/Y keep the scaled panel inside the canvas. Size persists in settings, profiles and portable skin presets, including both previews. Only key viewing and1-second KPS are shown.

## IR

Keysound length, silence ratio, audio scan availability, scan version and chart title/hash blacklist no longer exclude scores. The client stops selected-song/result audio eligibility scans and does not send scan evidence. Existing native-key, autoplay/practice/aborted/pause, supported judgement/mod and account/record validation policies remain. The web ignores retired scan fields for older measured clients while preserving all other strict record validation.

Previously hidden Speed of Link records become visible again; stored rows are preserved. No database migration, record deletion, replay-format change or new judgement generation. Production ranking is client submitted, without server replay/audio re-execution.

Extract the official ZIP into a new folder and run launch_win.bat. Keep prior installations/profiles. Physical play/listening, long sessions, arbitrary skins and other PCs remain separate checks. [Release verification gates](release-1.9.9.9-final2-gate.md).
