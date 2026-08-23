# NK3 ONNX models

TenRiff 1.4.5.2 combines two small ONNX model families for NK3 key-mode
conversion:

- `NK3-P64-hybrid.onnx` is the deterministic 64-slice decision graph. OpenVINO
  runs it on strict GPU by default or strict CPU when
  `TENRIFF_NK3_DEVICE=CPU` is set.
- `NK3-general-pattern-2K.onnx` through `NK3-general-pattern-18K.onnx` are
  fixed-target exports of one lane-shared schema-v3 pattern MLP. Its execution
  preference is verified NPU, then verified GPU, then verified CPU.
- A 1K target uses P64 without the pattern MLP because schema v3 requires at
  least two target lanes.

The MLP contributes a bounded candidate-ranking residual only. P64 validity
masks and the host beam solver remain authoritative for collisions, long-note
overlap, minimum gaps, hand regions, impossible chords, and newly created
jacks. See [NK3-GENERAL-MLP-MODEL-CARD.md](NK3-GENERAL-MLP-MODEL-CARD.md) for
the training/generalization boundary and `NK3-ONNX-SHA256SUMS.txt` for hashes.

No raw chart, training cache, or checkpoint is distributed in this directory.
