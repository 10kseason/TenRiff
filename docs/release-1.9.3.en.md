# TenRiff 1.9.3: gameplay feedback and external model compatibility

This release continues published 1.9.2. Its implementation includes the previously local ONNX and gameplay/Safetensors follow-ups. The fixed source baseline stays 1.9.0, and existing tags, assets, installations and profiles are preserved. Publication, final commit and downloads are determined by the [1.9.3 release page](https://github.com/10kseason/TenRiff/releases/tag/1.9.3); this scope document is not evidence that release checks have passed.

## Gameplay and display

- Empty Poor still drains gauge, counts as POOR and keeps existing raw-score/combo effects, but is excluded from ordinary, detailed and segment accuracy. Actual note misses and BAD still reduce accuracy. New Normal/Hard records use R7 and EX uses R8; historical R1–R6 replays keep their original calculations and board identities. Timing windows and replay format are unchanged.
- Song Select displays the authored BMS RANK at the album art's lower right: 4/3/2/1/0 → VEZ/EZ/NM/HD/VHD. Missing or invalid values use EZ. RANK 4 is a metadata label; its existing Easy timing fallback is unchanged. Cache 20 refreshes metadata without editing charts.
- Classic, Studio and battle headers reserve a red level cell beside the artist. Editor practice and direct launch use the actual launched chart's PLAYLEVEL when no indexed match exists.
- BMS measure lines follow authored lengths, BPM/STOP, rate and scroll positions behind notes, without creating judgement or audio events. osu! measure generation is outside this change.
- Always FAST/SLOW retains the last real non-PG timing text and marker. Conditional mode keeps its 750ms expiry. Text/bar switches remain independent, and no initial FAST value is invented.
- New or missing indexing settings default to Safe. Explicit saved Fast choices remain; native LV/CR calculation stays separately OFF by default.

## External BGA models

The order is original image → FFmpeg RGB deblock → model inference at its native scale → residual composition or completed-image output → display fitting → crop if needed. General ONNX models may use arbitrary tensor names, fixed or fully dynamic spatial axes and a uniform integer x1–x4 scale, within the documented one-input/one-output RGB NCHW FP32/FP16 contract. Auto uses explicit output metadata, then the legacy LunaSR2 name pair, otherwise completed images; Residual and Complete Image are explicit overrides. Auto does not infer a model's semantics from pixel values.

Safetensors is an optional local conversion path: supported LunaSR2 C32 R8/R12/R16 x2 or exportable installed Spandrel RGB x1–x4 architectures are converted to numerically checked ONNX before WinML. A user-installed Python with torch, safetensors, onnx and onnxruntime is required; other architectures also need Spandrel. The isolated worker does not auto-install dependencies, download weights, load pickle files or execute model-provided Python. Unsupported models retain native rendering and log the cause. See the [complete model contract and setup](../tools/onnx_upscaler/README.md).

**No BGA model weights are included in runtime or source packages.** The selection remains empty, BGA Upscaler OFF and ONNX Output Auto by default. Existing NK3 key-conversion models are unrelated and remain bundled. Static BGA and Song Select images are supported; video AI remains disabled. Dynamic models use the original dimensions, while fixed models use padding/context tiles; model-specific size/operator/memory limits still apply.

## Compatibility and validation boundaries

The approved production leaderboard version 19 supports R7/R8 in separate boards, preserving old records/board identities and the fixed Stable grade ladder. Guest and unmarked historical records remain permanently ineligible for submission; switching to Online cannot promote an earlier Guest play. Sites remains `client_submitted`, not server replay verification. No further web/database change is required by this game release.

Follow the [release verification gate](release-1.9.3-gate.md) for final builds, extracted-source checks, CI, package hashes and public readback. Earlier local passing checks are candidate evidence only. Physical input/audio, long play, other PCs, actual production-account submissions, real Spandrel models and universal model quality/performance require separate evidence.
