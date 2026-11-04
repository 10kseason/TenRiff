# TenRiff 1.9.9.9-hotfix1 — OGG security, Common BGA and skin editor

- Backport stb_vorbis comment size/count, multiplication/alignment, codebook limits and partial-allocation cleanup fixes. Both probe and decode use the repaired parser. Preserve the license and record [upstream SHAs/dependencies](../external/stb_vorbis.TENRIFF.md).
- In Options > Graphics, select **Common BGA Video** and enable **Common BGA** to replace every chart's BGA and overlay with one silent looping video. BGA OFF takes priority. Defaults remain OFF/empty. [Usage](common-bga-20261104.en.md).
- Refresh the offline skin editor version, all 14 Graphics preview rows, Song Select/Result examples and Korean/English/Japanese help. Common BGA is a game profile setting, not a skin.json v1 field. Browser previews are illustrative; check saved skins in the game.

Keep the Media Foundation fallback. This PC has no Opus-in-OGG handler, so actual Opus playback compatibility is unverified. Normal mono/stereo Vorbis at several sample rates and a 65-second track produce identical PCM before/after. Unusually large codebooks are rejected under the upstream limit.

Extract the Windows ZIP into a new folder and run `launch_win.bat`. Open `tools/skin_editor/Launch-Skin-Editor.cmd` for the offline editor. Preserve existing installations, profiles and records. Fixed baseline stays **1.9.6**. No judgement, replay, online-record or audio-timing changes and no website/database deployment.

[Verification / 검증](release-1.9.9.9-hotfix1-gate.md)
