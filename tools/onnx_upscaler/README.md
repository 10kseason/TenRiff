# TenRiff External ONNX Upscaler

TenRiff does not bundle BGA upscaler models in runtime or source releases. The model path defaults to empty and inference defaults to OFF; saved custom paths are preserved. Dynamic spatial residual-x2 models use the original image dimensions, while the fixed 960x540 export remains compatible through tiles. Select a rights-cleared external ONNX file from Options > Graphics > ONNX Model, or drop an .onnx file on that
screen. Selecting a model stores only its path in
graphics.background_upscale_model_path. It does not enable inference.

Use the separate BGA Upscaler row to switch graphics.background_upscale_mode
between off and onnx. Enabling requires confirmation of the high-spec warning.
There is no automatic performance benchmark gate; users remain responsible for
checking whether the selected model is fast enough on their system.

## Image processing order

Original image -> FFmpeg deblock -> AI x2 residual composition -> scale to the display area ->
crop only if the display aspect ratio requires it.

FFmpeg applies `format=gbrp,deblock=filter=weak:block=8:planes=7,format=bgra` once to the full original. The filter uses planar RGB without chroma subsampling; source alpha is restored exactly. Both model input and the bilinear base use these filtered pixels. The executable is resolved at `tools/ffmpeg-deblock/ffmpeg.exe` beside the game, then through executable search. It runs hidden on the worker with restricted inherited handles, concurrent input/output pipes, cancellation and a 10-second deadline. Missing/filter failure retains native rendering. No image files are written.

WIC decodes the authored image dimensions without pre-cropping or resizing.
Dynamic spatial models receive the entire original at its actual width and height
in one inference, without padding or tiling. Fixed-size models see original pixels: smaller images use replicated edge
padding, and larger images use tiles with 32 source pixels of context on each
split edge. Owned tile interiors are assembled into an image exactly twice the
original width and height. Padding/context removal does not discard original
image content. The renderer then cover-fits that completed image to the gameplay
background or Song Select preview area. A 640x480 image becomes 1280x960 before
display fitting; it is never first converted to 960x540.

The x2 bilinear base is sampled from the full deblocked original image, and the AI residual
is added in original coordinates, preserving alpha. Tiling preserves the model
file contract, but models requiring more than 32 pixels of context or global
image context can still show tile-dependent differences. Model-specific visual
quality must be evaluated separately.

Images up to 4096 pixels per axis are eligible, including FHD/4K sources. Larger
images keep native rendering rather than being silently downsampled for AI.
The maximum restored axis is 8192 pixels. Each upscaler caches at most three
images and 96 MiB of completed pixel data. Larger originals require more tile
inferences for fixed models; dynamic models require more memory for a single
full-image inference. The same 4096-per-axis source limit applies to both paths;
it does not guarantee that every model fits the GPU at that size. Changing the
source cancels stale work between inferences. A running WinML evaluation itself
cannot be interrupted, and a stale completed image is never presented.

## Supported model contract

The renderer accepts these explicit residual x2 contracts:

- input name: rgb_lr
- input tensor: float32 or float16 NCHW, RGB values in 0..1
- output name: rgb_residual_x2
- output tensor: float32 or float16 NCHW
- fixed spatial boundaries: input [1, 3, 540, 960], output [1, 3, 1080, 1920]
- dynamic spatial boundaries: input [1, 3, H, W], output [1, 3, H2, W2], with
  both spatial axes symbolic/unknown at both boundaries; the evaluated output
  must be exactly [1, 3, 2 * source_height, 2 * source_width]
- composition: clamp(bilinear_x2(deblocked_original) + residual at original coordinates, 0, 1)

The input and output data types are detected independently, so FP32, FP16, and
mixed FP32/FP16 boundaries are accepted. Batch and channel axes must be fixed
at 1 and 3. Partial dynamic spatial shapes, other fixed shapes, different names,
other data types, scale factors, or final-image outputs are not compatible.
Dynamic descriptor symbols alone do not establish the scale: the actual output
shape is checked after inference before residual composition.
Load, contract, decode, or inference failure leaves native BGA/BGI scaling active.

INT8 QDQ models are supported when their external input/output boundaries remain
FP32 or FP16. Models produced by `quantize_int8.py` carry
`tenriff.quantization=static_s8s8_qdq`; TenRiff detects that metadata and logs
the model as internal INT8/QDQ instead of treating it as an ordinary FP32 model.
Raw INT8/UINT8 input or output boundaries are detected but rejected because the
current public contract has no scale/zero-point fields.

Relative model paths are resolved from the executable directory and current
working directory. The file is loaded only while mode is onnx; changing the
selected path creates a fresh WinML session.

## Legacy low-power DirectX preference

graphics.background_upscale_prefer_npu defaults to false and has no effect while
the upscaler is off. The default path requests a Windows ML
DirectXHighPerformance GPU device. When the experimental low-power preference is
enabled, TenRiff first requests DirectXMinPower. Legacy WinML cannot explicitly
select or verify an NPU, so this path must not be treated as NPU success. If the
low-power session cannot be created or evaluate the model, TenRiff falls back to
DirectXHighPerformance and then the normal DirectX fallback.

Static image BGA remains eligible for asynchronous ONNX upscaling. Dynamic video
BGA stays on native decoded frames because an older asynchronous inference result
can otherwise overwrite a newer frame and make the background visibly jump.

## Verification

Build the smoke target and pass an explicit compatible model path:

    cmake --build build-dist --config Release --target onnx_upscaler_winml_smoke
    .\build-dist\Release\onnx_upscaler_winml_smoke.exe "D:\models\upscaler.onnx"

Running the smoke executable without a model path exits with code 2. The smoke
checks model loading, FP32/FP16 tensor binding, native-size x2 output, and
the internal BGRA request backpressure (gameplay video inference remains disabled);
it does not enforce a minimum FPS. For an optional original-image readback use
`onnx_upscaler_winml_smoke.exe model.onnx image.png new-output-prefix`.
The resulting `.json` and premultiplied `.bgra` files record exact output dimensions
and pixels. Existing output files are not overwritten.

`test_native_pipeline.py <smoke-exe> <new-output-dir> <ffmpeg-exe>` generates synthetic ONNX
fixtures outside the source tree, compares GPU outputs against an independent
whole-image reference after an independent FFmpeg filter invocation, and covers square/4:3/portrait/odd/FHD sizes, tile seams,
alpha and FP32/FP16/mixed boundaries. It also checks dynamic original-size
inference on odd, portrait and FHD images and rejection of wrong batch/channel/
rank/scale and partial-dynamic contracts. This verifies the processing contract, not
trained-model quality or real-time performance. ONNX
models, checkpoints, training data, and
model-specific verification metadata must remain outside public TenRiff
packages.

`deblock_smoke` verifies concurrent workers, RGB filtering, alpha preservation, invalid input and cancellation; `--missing` also verifies missing-executable fallback in an isolated directory without FFmpeg.
