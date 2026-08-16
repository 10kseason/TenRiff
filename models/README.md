# NK3 P64 hybrid ONNX model

`NK3-P64-hybrid.onnx` is TenRiff's deterministic tensor decision graph for the
NK3 key-mode converter. It is not a trained neural-network checkpoint and was
constructed from project-owned rules without training data or user charts.

- Input block: 64 note slices plus context, memory, pressure, and constraint tensors
- Output: fixed-point candidate scores, validity masks, addition counts, and next state
- Runtime: OpenVINO, strict NPU by default; GPU and CPU can be selected explicitly
- SHA-256: `C47E07D17A8CB51D3FFF2F15A4778240DE7E8FC7A847AB4A71269A2E60F14063`

The host beam solver remains authoritative for collision, long-note, minimum-gap,
hand-region, and novel-jack safety constraints.
