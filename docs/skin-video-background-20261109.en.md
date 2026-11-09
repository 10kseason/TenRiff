> **Integrated in 1.9.9.9-final3.** The original candidate report below is historical. See [release guide](release-1.9.9.9-final3.en.md) for final behavior and validation scope.

# Moving skin background videos and the TenRiff Code / CLI skin (2026-11-09 local candidate)

[한국어](skin-video-background-20261109.ko.md) · [日本語](skin-video-background-20261109.ja.md) · [简体中文](skin-video-background-20261109.zh-CN.md)

User request: a skin that feels like playing the Claude Code CLI, with moving backgrounds. This separate local candidate continues the Modern Common candidate unchanged.

## Skin background videos

- `lobby.background_video`: a common loop for every menu screen. It replaces the built-in Song Select/Result loops and the skin's still background.
- `lobby.screen_videos.<screen ID>`: per-screen loops, resolved screen → fallback screen → common video. An empty string keeps that screen on its still image.
- `gameplay.background_video`: a gameplay loop, decoded and shown only while the chart has no base BGA layer. It follows the BGA chart clock (pause, rate, restart) and supports per-mode overrides.
- Formats: MP4/M4V/MPG/MPEG/WMV/AVI/MOV/WEBM/MKV with the image path rules. Videos are never auto-detected and their audio is never played.
- A video is drawn in place of the still at the same screen opacity. Before its first frame, under system/menu reduced motion and after a decode failure, the still is used. Reloading the skin clears failure latches and reopens the files.
- Folder import and `.trskin` export copy referenced videos. The schema and bundled offline editor (validation, folder open, ZIP export) support the new fields; the editor canvas previews the still.

## Video decoder fix

H.264 1080p decodes into 1088 rows. The Media Foundation path did not crop to the 1080-row display aperture, so four green rows appeared at the bottom of the screen; the built-in Song Select/Result loops reproduced it too. The decoder now reads `MF_MT_MINIMUM_DISPLAY_APERTURE` (falling back to the geometric aperture) and crops to it. Missing or malformed apertures keep the whole buffer as before.

## TenRiff Code / CLI skin

Bundled `TenRiff_Code_CLI`: Cascadia Mono type, coral accent, warm-black panels, block-cursor notes, selection-style LNs, cursor-underline keys and the `pixels` hit burst. Menus use a 16-second loop of drifting spinner glyphs, the title a prompt that types a command, and gameplay a 24-second session transcript that types itself out. Supports 1K–16K and 7+1. It is an unofficial fan skin inspired by the Claude Code CLI; no Anthropic logos, mascots or trademark imagery are used, and every image and video is drawn by the bundled Pillow generators.

## Unchanged

Judgement, scoring, gauges, input, audio, replay and ranking policy, `version: 1`, existing skins and profile settings are unchanged, as is the behavior of skins without video fields. The four-row crop of 1080p H.264 menu videos also applies to the built-in loops. Physical input, long sessions and codec environments on other PCs remain separate verification.
