# TenRiff External ONNX Upscaler

TenRiff does not ship an upscaler model. Users may select their own rights-cleared
ONNX file from Options > Graphics > ONNX Model, or drop an .onnx file on that
screen. Selecting a model sets graphics.background_upscale_mode to onnx and
stores its path in graphics.background_upscale_model_path.

## Supported model contract

The current renderer intentionally supports one explicit residual x2 contract:

- input name: rgb_lr
- input tensor: float32 NCHW [1, 3, 540, 960], RGB values in 0..1
- output name: rgb_residual_x2
- output tensor: float32 NCHW [1, 3, 1080, 1920]
- composition: clamp(bilinear_x2(input) + residual, 0, 1)

Models with different names, shapes, data types, scale factors, or final-image
outputs are not compatible yet. Load, contract, benchmark, decode, or inference
failure leaves native BGA/BGI scaling active.

Relative model paths are resolved from the executable directory and current
working directory. The file is loaded only while mode is onnx; changing the
selected path creates a fresh WinML session and a model-specific benchmark gate.

## Performance gate

The selected model must reach at least 35 FPS in the fixed 960x540 RGB x2
benchmark. The gate is cached per selected model path for the current process.
A failed model is disabled for that process without interrupting gameplay.

## Verification

Build the smoke target and pass an explicit compatible model path:

    cmake --build build-dist --config Release --target onnx_upscaler_winml_smoke
    .\build-dist\Release\onnx_upscaler_winml_smoke.exe "D:\models\upscaler.onnx"

Running the smoke executable without a model path exits with code 2. ONNX
models, checkpoints, training data, and model-specific verification metadata
must remain outside public TenRiff packages.