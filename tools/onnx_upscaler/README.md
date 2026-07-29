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
- input tensor: float32 NCHW [1, 3, 540, 960], RGB values in 0..1
- output name: rgb_residual_x2
- output tensor: float32 NCHW [1, 3, 1080, 1920]
- composition: clamp(bilinear_x2(input) + residual, 0, 1)

Models with different names, shapes, data types, scale factors, or final-image
outputs are not compatible yet. Load, contract, decode, or inference
failure leaves native BGA/BGI scaling active.

Relative model paths are resolved from the executable directory and current
working directory. The file is loaded only while mode is onnx; changing the
selected path creates a fresh WinML session.

## Experimental NPU preference

graphics.background_upscale_prefer_npu defaults to true, but has no effect while
the upscaler is off. When enabled, TenRiff first requests a Windows ML
DirectXMinPower device. This is an NPU preference, not an NPU guarantee: Windows
and the installed driver choose the actual accelerator. If the low-power session
cannot be created, TenRiff falls back to the existing DirectXHighPerformance
path and then the normal DirectX fallback.

## Verification

Build the smoke target and pass an explicit compatible model path:

    cmake --build build-dist --config Release --target onnx_upscaler_winml_smoke
    .\build-dist\Release\onnx_upscaler_winml_smoke.exe "D:\models\upscaler.onnx"

Running the smoke executable without a model path exits with code 2. The smoke
checks model loading, the fixed tensor contract, and end-to-end output shape; it
does not enforce a minimum FPS. ONNX models, checkpoints, training data, and
model-specific verification metadata must remain outside public TenRiff
packages.
