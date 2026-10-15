> 2026-10-16 follow-up: [FFmpeg deblock and LunaSR2 x2](deblock-lunasr-20261016.en.md) — Original → FFmpeg deblock → LunaSR2 AI x2 restoration/residual composition → display fitting → optional crop

# Restore BGA at its original resolution before fitting

2026-10-16

This local follow-up starts from release 1.9.1.1 (`ea8bdba`) plus the verified UI candidate. It retains fixed grades, brightness, loading, languages and the released BMS/online/input/volume behavior.

**Original image → AI x2 restoration and residual composition → fit to display area → crop if needed.** The previous pre-inference 16:9 crop and 960×540 resize are removed. A 640×480 image is restored to 1280×960; 300×900 becomes 600×1800 before display fitting.

The existing fixed 960×540 input / 1920×1080 residual model contract remains compatible. Original pixels are padded by edge replication or processed in tiles with 32 pixels of context on split edges. Tile interiors are assembled at original x2 coordinates without discarding source content. Notes, HUD and BGA brightness retain their existing order.

Source dimensions are bounded at 4096 per axis (8192 restored). Oversized images or failed models retain native rendering, never an implicit AI downsample. Static images only; video inference remains disabled. Each instance caches at most three images/96 MiB and checks stale requests between tiles.

Large originals require more inference and memory. Models needing wider or global context may show tile-dependent differences. Synthetic tests verify pixels, dimensions, alpha and FP32/FP16 contracts, not trained-model quality or real-time FPS.
