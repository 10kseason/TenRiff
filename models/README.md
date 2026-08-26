# NK3 ONNX models

TenRiff 1.5.0 combines two small ONNX model families for NK3 key-mode
conversion:

- `NK3-P64-hybrid.onnx` is the deterministic 64-slice decision graph. The
  default runtime is the converted ncnn model under `models/ncnn`, executed on
  Vulkan.
- `NK3-general-pattern-2K.onnx` through `NK3-general-pattern-18K.onnx` are
  fixed-target exports of one lane-shared schema-v3 pattern MLP. Its execution
  converted ncnn variants use the same weights on Vulkan.
- A 1K target uses P64 without the pattern MLP because schema v3 requires at
  least two target lanes.

The MLP contributes a bounded candidate-ranking residual only. P64 validity
masks and the host beam solver remain authoritative for collisions, long-note
overlap, minimum gaps, hand regions, impossible chords, and newly created
jacks. See [NK3-GENERAL-MLP-MODEL-CARD.md](NK3-GENERAL-MLP-MODEL-CARD.md) for
the training/generalization boundary and `NK3-ONNX-SHA256SUMS.txt` for hashes.
See `ncnn/README.md` and `ncnn/NK3-NCNN-SHA256SUMS.txt` for the deployment
conversion contract and derived-model hashes.

No raw chart, training cache, or checkpoint is distributed in this directory.
