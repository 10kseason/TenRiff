# NK3 generalized pattern MLP model card

## Runtime contract

- Schema: v3 shared-relation pattern MLP
- Input: 28 features per target lane
- Output: 8 candidate-role residuals per target lane
- Fixed inference batch: 32 solver states
- Host residual weight: 0.15
- Maximum model residual: 1.0 before host weighting
- Chord feature: source chord-size ratio
- Score transform: center the addition-role logit across each target-lane group before `tanh`
- Device preference: verified OpenVINO NPU, then GPU, then CPU
- Deployment targets: fixed-target ONNX exports for 2K through 18K

The lane encoder and pooled context are shared across target lanes. TenRiff
therefore exports the same trained weights at each supported target width; this
is an architectural target-count generalization adapter, not separate training
or fine-tuning for every target count.

## Training boundary

The replacement source weights declare a 10K training target. The supplied NPZ
filename is `NK3-10K-pattern-mlp-v3-e100-cuda.npz`, but it does not embed a
dataset manifest, training metrics, or independently verifiable provenance, so
the release does not claim those details. The source NPZ is not distributed;
its SHA-256 is
`9BB5FD17E6F6FB58835A87D5C7E8E6A51FCC314E3C78CBA28E3F8469E81F557A`.

The release tests every 1K-through-18K source and target pair for conversion
completion and structural safety (324 routes). Those tests validate integration
and safety, not musical quality across every key count. Manual playability tests
remain recommended, especially outside the declared 10K training target.

## Safety boundary

The model never directly emits chart notes. It only adjusts candidate order.
Deterministic P64 validity, beam-state transitions, safety retries, and final
quality inspection decide whether a candidate can be emitted. Unsafe output is
rejected rather than applied.

## Distribution

The public repository and release archives contain only the fixed-target ONNX
inference graphs, this model card, and hashes. They contain no source charts,
raw training samples, caches, or trainable checkpoint.
