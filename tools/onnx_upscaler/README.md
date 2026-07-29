# TenRiff External ONNX Upscaler

TenRiff does not ship an upscaler model. Users may select their own rights-cleared
ONNX file from Options > Graphics > ONNX Model, or drop an .onnx file on that
screen. Selecting a model stores only its path in
graphics.background_upscale_model_path. It does not enable inference.

Use the separate BGA Upscaler row to switch graphics.background_upscale_mode
between off and onnx. Enabling requires confirmation of the high-spec warning.
There is no automatic performance benchmark gate; users remain responsible for
checking whether the selected model is fast enough on their system.

## Supported model contract

The current renderer intentionally supports one explicit residual x2 contract:

- input name: rgb_lr
- input tensor: float32 or float16 NCHW [1, 3, 540, 960], RGB values in 0..1
- output name: rgb_residual_x2
- output tensor: float32 or float16 NCHW [1, 3, 1080, 1920]
- composition: clamp(bilinear_x2(input) + residual, 0, 1)

The input and output data types are detected independently, so FP32, FP16, and
mixed FP32/FP16 boundaries are accepted. Models with different names, shapes,
other data types, scale factors, or final-image outputs are not compatible yet.
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

## Experimental NPU preference

graphics.background_upscale_prefer_npu defaults to false and has no effect while
the upscaler is off. The default path requests a Windows ML
DirectXHighPerformance GPU device. When the experimental preference is enabled,
TenRiff first requests DirectXMinPower. This is an NPU preference, not an NPU
guarantee: Windows and the installed driver choose the actual accelerator. If
the low-power session cannot be created or evaluate the model, TenRiff falls
back to DirectXHighPerformance and then the normal DirectX fallback.

Dynamic video BGA uses one in-flight inference request. New decoded frames are
temporarily rejected while the accelerator is busy so a completed frame is not
discarded only because a newer request ID arrived.

## Verification

Build the smoke target and pass an explicit compatible model path:

    cmake --build build-dist --config Release --target onnx_upscaler_winml_smoke
    .\build-dist\Release\onnx_upscaler_winml_smoke.exe "D:\models\upscaler.onnx"

Running the smoke executable without a model path exits with code 2. The smoke
checks model loading, FP32/FP16 tensor binding, end-to-end output shape, and
dynamic-video request backpressure; it does not enforce a minimum FPS. ONNX
models, checkpoints, training data, and
model-specific verification metadata must remain outside public TenRiff
packages.
