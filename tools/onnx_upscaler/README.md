# TenRiff External ONNX / Safetensors Upscaler

TenRiff 1.9.3 supports residual and completed-image ONNX outputs, plus optional
local conversion of supported external Safetensors weights. Published 1.9.2 assets remain unchanged. **BGA model weights are never bundled in
runtime or source packages.** Select your own file in Options > Graphics > ONNX /
Safetensors, or drop it on that screen. The default path is empty and BGA Upscaler is OFF.
Selecting a different file resets ONNX Output to Auto; it does not enable inference.
Saved paths and existing LunaSR2 residual-x2 models remain compatible.

## Output selection

Options > Graphics > ONNX Output is saved as `graphics.background_upscale_output_mode`:

| Value | Interpretation |
| --- | --- |
| `auto` (default) | Use model metadata `tenriff.output_mode=image` or `residual`; without it, the exact legacy pair `rgb_lr` / `rgb_residual_x2` means residual, and other models mean completed images. |
| `residual` | Add the model output to the bilinear enlargement of the deblocked original, then clamp RGB to 0..1. |
| `image` | Use the model output as the completed RGB image, clamped to 0..1. Do not add the original again. |

Explicit selection overrides metadata and legacy names. Auto is a compatibility
rule, not a semantic classifier: dimensions and pixel values cannot reliably
distinguish residuals from completed images. For an unmarked residual model,
choose Residual. For a completed-image model using legacy residual names, choose
Complete Image. Invalid metadata in Auto keeps native rendering. Model/path/mode
changes replace the inference sessions and bitmap caches. Profile-preset imports
retain the local model path and its output mode together.

## Supported model contract

- Exactly one tensor input and one tensor output; **arbitrary tensor names**.
- NCHW RGB, input values in **0..1**, no mean/std or other normalization.
- FP32 or FP16 at each boundary, including mixed FP32/FP16. Internal INT8 QDQ
  can work with float boundaries; raw INT8/UINT8 boundaries are not supported.
- Batch is fixed 1 or symbolic (executed at 1); channels must be fixed 3.
- Input spatial axes are both fixed positive dimensions or both dynamic.
  Output spatial axes may likewise be fixed or both dynamic. Partially dynamic
  spatial axes, NHWC, extra inputs and multiple outputs are rejected.
- Actual output must be `[1,3,s*H,s*W]`, with uniform integer scale **1, 2, 3 or 4**.
  Scale is inferred from the evaluated tensor and checked for every tile.
- Source and fixed-input dimensions are at most 4096 per axis; completed images
  are at most 8192 per axis. Thus x4 can process originals up to 2048 per axis.
  These are safety bounds, not a promise that a model fits GPU memory.
- All operations must run on the installed Windows ML DirectX backend. Dynamic
  descriptors do not guarantee that a model accepts odd dimensions: models with
  internal size-multiple requirements need a compatible export.

`.pth`, arbitrary channel layouts, [-1,1] or 0..255 normalization,
noninteger/asymmetric scaling, and unsupported operators need model conversion;
selecting them does not make them compatible. Failure keeps native scaling.

## Optional Safetensors conversion

Safetensors contains tensor weights, not an executable architecture. Selecting a
`.safetensors` file invokes the packaged `convert_safetensors.py` on the image
worker before WinML loads its verified ONNX export. The original image stays
visible during conversion. **Python and model dependencies are optional, external,
and never installed or downloaded automatically.** ONNX loading does not need them.

Use a local Python with `torch`, `safetensors`, `onnx` and `onnxruntime`. Set
`TENRIFF_UPSCALER_PYTHON` to its absolute `python.exe` path when it is not on PATH
(a local virtual environment is supported). The process uses Python isolated mode
`-I`, so user-site packages and PYTHONPATH are ignored. Standard PATH `python.exe`
or `py.exe -3` is the fallback. Failure details go to the normal game log; the
Graphics settings note explains dependencies and keeps the native-image fallback.

Supported structures:

- Built-in **LunaSR2 C32 R8/R12/R16 x2**: exact stem/block/trunk/head tensor shapes
  and keys, optionally prefixed with `core.`. Deployment-form weights already
  contain the residual gain. Training-form `spatial`/`pointwise` weights require
  Safetensors metadata `lunasr2.residual_gain` with the original value in `(0,0.25]`.
  The converter does not guess this unrecorded scalar. Output is signed residual,
  fully dynamic in H/W, with `tenriff.output_mode=residual` metadata.
- Other **installed Spandrel** image-to-image architectures: RGB input/output,
  integer x1-x4 and `ModelTiling.SUPPORTED` only. The descriptor preserves its
  normalization, padding and cropping. Export uses fixed 256x256 context tiles
  to avoid falsely declaring shape-dependent Python tracing dynamic. Output is
  a complete image. Models needing global context, masks, extra inputs, unsupported
  ONNX operators, or unsuccessful numerical verification are rejected.

This is not universal Safetensors support. No model-side Python, pickle, `.pt`,
`.pth`, URL fetching or auto-discovered code is executed. Archives are capped at
1 GiB. Conversion runs in a hidden job-managed process, with a 120-second deadline
and cancellation on worker replacement/shutdown. Three image workers serialize
conversion. Only successfully checked exports enter `%LOCALAPPDATA%/TenRiff/upscaler-cache`;
the key includes source SHA-256, converter bytes and dependency versions. Cached
ONNX bytes are hash-checked before reuse. Temporary request/export files are
removed after completion or cancellation. Original weights are never modified.
The user may remove the cache while the game is closed to reclaim disk space.

The exporter checks ONNX structure and compares CPU inference with the selected
PyTorch architecture before publishing a cache entry. WinML still performs its
usual boundary/operator checks; CPU export success alone does not promise GPU
compatibility or visual quality. Explicit output-mode overrides still apply.

Architecture APIs: [Spandrel ModelLoader](https://chainner.app/spandrel/spandrel.ModelLoader.html),
[ImageModelDescriptor](https://chainner.app/spandrel/spandrel.ImageModelDescriptor.html),
[Safetensors tensor loading](https://huggingface.co/docs/safetensors/main/en/api/torch).

## Image processing order

Original image -> FFmpeg deblock -> model at its native scale -> residual
composition **or** completed-image output -> display fitting -> crop if needed.

FFmpeg applies `format=gbrp,deblock=filter=weak:block=8:planes=7,format=bgra` once
to the full original. Source alpha is restored exactly after filtering, then
bilinearly enlarged for premultiplied output in both modes. Both model input and
the residual base use the deblocked pixels. No original image is resized or
cropped before inference.

Dynamic models receive the complete original at its actual size. Fixed models
use replicated edge padding and tiles, preserving every original pixel. Context
per split axis is `min(32, fixed_input_axis / 4)` source pixels; the legacy
960x540 export keeps its 32-pixel context. Only owned tile interiors are assembled.
Models needing larger/global context may show seams; quality needs separate review.

The FFmpeg helper is resolved at `tools/ffmpeg-deblock/ffmpeg.exe` beside the game,
then through executable search. It runs hidden on the worker with restricted
handles, concurrent pipes, cancellation and a 10-second deadline. No temporary
images are written. Missing/filter failure retains native rendering.

Each upscaler caches at most three images and 96 MiB of completed pixels. Stale
work is cancelled between evaluations and never presented. A running WinML
evaluation cannot itself be interrupted. Static BGA and Song Select images are
supported; video AI remains disabled. There is no benchmark cutoff or guaranteed
frame rate. BGA Upscaler still requires its separate high-spec confirmation.

The default device is DirectXHighPerformance with DirectX fallback. The legacy
`background_upscale_prefer_npu` option requests DirectXMinPower, with GPU fallback;
it cannot explicitly select or verify an NPU. Quantization metadata
`tenriff.quantization` remains supported for logging internal INT8/QDQ models.
Relative model paths resolve from the executable directory and working directory.

## Verification

Build `onnx_upscaler_winml_smoke`, then use the **production loader and composition**:

```text
onnx_upscaler_winml_smoke.exe model.onnx image.png new-output-prefix [auto|residual|image]
```

It writes dimensions to `.json` and premultiplied pixels to `.bgra`, and refuses
to overwrite an existing result. The single-model-argument form retains the
legacy LunaSR2 x2 binding/warm-cache/BGRA-backpressure smoke. No arguments exits 2.

`python -B tools/onnx_upscaler/test_safetensors.py -v` covers synthetic deployment
and training-form residual pixels, dynamic sizes, cache reuse/corruption/source
changes, unsupported structures, missing dependencies, nonfinite tensors and
temporary cleanup. Build target `safetensors_process_smoke` verifies the same production
process helper's Unicode/quote handling, exit codes, timeout/cancellation and
descendant termination. It accepts `python.exe` and a new evidence directory.
No generated test weights belong in a package. Spandrel requires a separately
installed package and model-specific verification; Luna fixture success does
not establish compatibility of all Spandrel architectures.

`test_output_modes.py <smoke.exe> <new-output-dir> <ffmpeg.exe>` generates synthetic
models outside the source tree and compares GPU pixels with independent NumPy
references after FFmpeg filtering. It covers automatic/explicit/metadata modes,
arbitrary names, FP32/FP16/mixed boundaries, x1/x2/x3/x4, fixed tiling, alpha,
clipping, and rejected boundaries/scales/nonfinite output. These fixtures do not
establish trained-model visual quality or performance.

`test_native_pipeline.py` retains the old x2 original-size, detail, seam and
backpressure regression checks. `deblock_smoke` covers filter concurrency,
alpha, cancellation and missing-executable fallback. Generated models, training
data, user model paths and model-specific evidence must remain outside packages.
