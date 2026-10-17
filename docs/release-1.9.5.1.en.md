# TenRiff 1.9.5.1 — Judgement line, FAST/SLOW and file pickers

Skin Settings offers judgement-line Type 1/2. Type 1 remains the default. Type 2 visually seats on-time notes inside a green band, accounting for image aspect/rotation, shape, holds, ghosts and measure lines. Judgement timing, score, gauge and replay rules are unchanged.

FAST/SLOW remains for 0.35 seconds from the last hit outside the maximum window. A maximum judgement neither clears it immediately nor extends its lifetime. Another outside-window hit replaces it immediately. The Always scale and 0.7-second dot/barcode lifetime remain separate.

Photo, folder, model and portable-setting pickers create a fresh dialog each time. An overlapping explicit request cancels the previous app-owned picker, with only one nested replacement allowed to prevent recursive reopen loops. Only the game window is the owner, and its enabled state is restored on completion. Shell interfaces are released before COM shutdown and stale modal input is discarded. Other Explorer windows and processes are never closed.

New master volume defaults to 30%; saved values remain. The built-in song library is wider and central BEST text scales to about 79%. The album badge gains a localized judgement-difficulty prefix. Explicit custom lobby coordinates remain.

Includes 1.9.5 output routing, random order, profile keys, deferred reclamation, ONNX/Safetensors, Guest exclusion and historical replay compatibility. No BGA weights ship; OFF/empty/Auto defaults and existing NK3 models remain. The fixed baseline stays 1.9.0. No website/database changes.

[Verification scope](release-1.9.5.1-gate.md) · [Release](https://github.com/10kseason/TenRiff/releases/tag/1.9.5.1)
