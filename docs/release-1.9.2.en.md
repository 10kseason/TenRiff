# TenRiff 1.9.2 changes and compatibility

2026-10-16 · Based on release 1.9.1.1, `ea8bdba0fb8aa533f0e684b64fc64515e2eb2e41`. The fixed baseline stays 1.9.0; earlier tags, assets, installations and profiles are preserved. Check the [1.9.2 release page](https://github.com/10kseason/TenRiff/releases/tag/1.9.2) for publication and downloads. This document describes behavior, not completed verification.

## Guest and Online modes

Every launch opens the F10 account window with **Guest Mode** and **Online Mode** choices. Every launch starts as Guest; dismissing the startup choice with Esc also leaves Guest selected. F10 reopens the window. Online selection lasts only for the current process and is not saved for the next launch.

Only a fresh play started in Online mode can be submitted, subject to existing connection and eligibility checks. Guest plays stay local permanently: switching to Online or connecting an account later never promotes them. Historical records without an origin marker also cannot upload; local history and replay playback remain available.

Any switch to Guest irrevocably revokes the current play's upload permission. Switching back to Online affects the next fresh play, not that play. Both Sites and the separate Ranked submission path enforce this boundary. Guest transition invalidates pending uploads and waits for any POST that already started to finish before the transition returns. It does not withdraw or delete records the server already received.

No server/DB format or numerical ranking change is included. Sites remains a `client_submitted` community leaderboard. These client permission checks are not server replay verification or anti-cheat certification. Existing restrictions such as LN Mix v2, autoplay and unsupported rules remain.

## Graphics, skins and menus

- Remove the V-Sync option row; retain refresh selection and legacy `graphics.vsync` values.
- BGA brightness uses 0–100% in 1% steps, default 100%, after background composition and before notes/HUD.
- LR2/TenRiff skin import, chart loading/indexing and difficulty-table loading use the shared loading bar.
- The lower-right Home language button cycles English → Korean → Japanese → Chinese and saves the choice.
- Remove **Export Skin Preset** from Skins. Import, file drop, full-profile export and its internal skin packaging remain.

The fixed grade ladder uses unrounded ordinary accuracy: 99% S+++ / 98% S++ / 97% S+ / 95% S / 93% A+ / 90% A / 85% B / 80% C / 70% D / surviving below 70% E / gauge failure F. No record or abort stays `--`; autoplay stays `AUTO`. Score, judgement, gauge and numerical rank calculations remain unchanged. Website Stable means formal operating status and this same grade display policy.

## External AI upscaling

**Original → FFmpeg deblock → AI x2 restoration/residual composition → fit display area → crop if needed.** The built-in weak/block-8 deblock filter runs once on the complete RGB image and preserves alpha. Model input and the bilinear x2 base use the same filtered pixels.

Dynamic spatial models receive the complete original H/W in one evaluation. Fixed 960×540 input models remain compatible through original-pixel padding and tiles with 32 pixels of context. There is no pre-inference resizing or 16:9 crop. Evaluated residual output must be exactly x2; the 4096-pixel source-axis limit and three-image/96 MiB cache per instance remain. Static BGA and Song Select images are eligible; video AI remains disabled.

**Neither the public runtime nor source package contains BGA upscaler weights.** The model path defaults to empty and upscaling defaults to OFF. Select or drop an external ONNX file in Graphics, then enable it separately. Saved user paths/settings are retained; existing NK3 key-conversion models remain bundled. FFmpeg is resolved from `tools/ffmpeg-deblock/ffmpeg.exe` beside the game, then PATH. Missing executables, filter errors or model failures retain native rendering.

[Detailed processing contract](deblock-lunasr-20261016.en.md) · [Verification procedure and remaining checks](release-1.9.2-gate.md)
