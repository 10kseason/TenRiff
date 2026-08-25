# NK3 ncnn Vulkan models

These files are deterministic deployment conversions of the project-owned NK3
ONNX models in the parent directory. TenRiff loads them with ncnn 20260526 and
runs their floating-point inference on Vulkan.

- `NK3-P64-hybrid.ncnn.*` keeps the P64 float score, context, and memory graph.
  Exact INT64 pressure bookkeeping and BOOL validity masks stay in the C++ host.
- `NK3-general-pattern-*K.ncnn.*` emits the two unique move/add role residuals.
  The host expands them to the ONNX contract's eight candidate roles using
  `[0, 0, 1, 1, 1, 1, 1, 1]`.
- Models use FP32 storage/arithmetic for deterministic ranking behavior.

Regenerate from the repository root with Python `onnx`, NumPy, and the official
pnnx 20260526 executable:

```powershell
python tools/convert_nk3_ncnn.py `
  --pnnx C:\path\to\pnnx.exe `
  --models-dir models `
  --output-dir models\ncnn
```

The converter also restores Reduction axes omitted by pnnx for these opset-18
graphs. Verify regenerated files against `NK3-NCNN-SHA256SUMS.txt` and rerun the
`nk3_onnx_smoke` executable with `TENRIFF_NK3_BACKEND=VULKAN`.
