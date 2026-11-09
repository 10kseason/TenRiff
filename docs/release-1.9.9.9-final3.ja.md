# TenRiff 1.9.9.9-final3

失敗ノートを判定ラインより下で隠し、判定ラインの明度設定を追加しました。既存12スキンを共通の現代的スタイルに再設計し、3つのループ動画付きTenRiff Code / CLIを同梱します。AI制作ガイド・v1スキーマ・Web/同梱エディターは新しい明度と動画フィールドに対応します。

Windows command-line paths and profile names now use the wide CRT entrypoint and explicit UTF-8 conversion, preserving Korean, Japanese, emoji and spaces. The normal relative-path launcher remains compatible.

## Use

Extract the release ZIP into a new folder and run `launch_win.bat`. In Skin settings, select a bundled TenRiff theme. The profile judgement-line brightness defaults to100%; a skin's optional `gameplay.judgement_line_brightness` multiplies it, including mode overrides and previews. Failed taps and failed LN heads continue falling visually but are clipped below the timing line; their committed judgement and later-note processing do not change.

Twelve existing skin folders now use a shared native menu/gameplay style with their own accents. Their original manifests remain as `skin.legacy.json`; original art and available README files are retained. `TenRiff_Code_CLI` adds a coral terminal theme with silent16s menu/title and24s gameplay loops. It is an unofficial fan design with procedurally authored art; no Anthropic logo or mascot is included. Font fallback applies if Cascadia Mono is unavailable.

## Video and authoring

Optional v1 fields: `lobby.background_video`, `lobby.screen_videos`, `gameplay.background_video`, and gameplay mode overrides. Set explicit relative paths; filenames are never auto-detected. An empty screen override disables the common video. Keep PNG fallbacks: reduced motion, the first frame and decode failure use still backgrounds. Gameplay videos use the chart clock and yield to chart/Common BGA base layers; overlays keep their existing order. The browser canvas previews still images while folder/ZIP export preserves video bytes.

Use [AI guide](skin-agent-guide.md), [format](skin-format.md), [schema](tenriff-skin.schema.json), [video details](skin-video-background-20261109.en.md), and [web editor](https://tenriff-skin-editor.lastestarcorp.chatgpt.site). Final2 and earlier do not support the new gain/video fields; omit them for older clients. The schema format remains v1.

Media Foundation now crops decoded frame padding to the display aperture, fixing green rows at the bottom of1080p videos. Loop recovery also handles a fresh FFmpeg request beyond EOF, and failed skin video returns to the still image. This does not change scoring, judgement, input, audio, replay/R13-R14 or leaderboard identities. Final2's removal of false-positive IR audio exclusions and4–16K /25–100% overlay remains. Fixed baseline1.9.6 remains.

Physical input/listening, long sessions, arbitrary user skins/codecs, and other PCs remain separate checks. No production score/account/key writes, database migration or installation/profile overwrite. [Release gates](release-1.9.9.9-final3-gate.md).
