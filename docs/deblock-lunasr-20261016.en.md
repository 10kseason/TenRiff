# FFmpeg deblock and external ONNX x2

2026-10-16 · TenRiff 1.9.2. [Release scope](release-1.9.2.en.md)

**Original → FFmpeg deblock → external AI x2 restoration/residual composition → display fitting → crop if needed**

The built-in FFmpeg `deblock=filter=weak:block=8:planes=7` runs once on the complete original RGB image. Planar RGB avoids YUV chroma subsampling; source alpha is preserved. AI input and the bilinear x2 base use the same deblocked pixels.

Dynamic spatial models evaluate the entire original H/W at once. Existing fixed 960×540 input/1920×1080 residual models remain compatible through edge padding and tiles with 32 pixels of context. Neither path resizes or crops to 16:9 before inference; evaluated output must be exactly original x2. Tensor names are `rgb_lr` and `rgb_residual_x2`, with float32/float16 NCHW RGB boundaries. Dynamic models still require batch/channel axes of 1/3.

**Neither public runtime nor source package includes BGA upscaler weights.** The default model path is empty; legacy empty selections stay empty. Select or drop an external ONNX file in Graphics to save its path only. Upscaling defaults to OFF and retains its separate enable confirmation. Saved custom paths are preserved; existing NK3 key-conversion models remain bundled.

Static BGA and Song Select images are eligible; video AI stays disabled. Source axes are limited to 4096 pixels and each instance caches at most 3 images/96 MiB. Dynamic models need memory for the complete original at once, so the dimension limit does not guarantee GPU memory sufficiency. FFmpeg runs hidden on the worker and stops on cancellation or a 10-second deadline. Missing executables, filter errors and model failures retain native rendering. A running WinML evaluation itself cannot be interrupted, but stale results are never displayed.

FFmpeg is resolved from `tools/ffmpeg-deblock/ffmpeg.exe` beside the game, then PATH. See [THIRD_PARTY_NOTICES](../THIRD_PARTY_NOTICES.md) for dependency notices. External originals, earlier candidates, installations and profiles are preserved. Model quality/performance and long-session behavior need separate verification.

[Processing contract and test tools](../tools/onnx_upscaler/README.md) · [1.9.2 verification gate](release-1.9.2-gate.md)
