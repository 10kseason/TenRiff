# TenRiff 1.7.0 UI Update release gate

1.7.0 modernizes the native Home, Song Select, single-player Result and shared
settings UI. It preserves existing judgements, scores, replay evidence and online
record protocols. Custom lobby/title skins retain their layout paths.

## Contents

- Quiet panels, consistent spacing and a prominent primary action.
- Score-first result summary, readable judgement counts and centered actions.
- Readable-width settings lists with cached, wrapped and paginated guidance.
- Skin preview with legible labels on light color swatches.
- Wider Session Mix navigation and accurate empty-library presentation.
- An explicit developer-only `menu_visual_preview` build target, not included in
  the playable archive.

## Verification

- Curated release source checkout: Windows x64 Release client and replay verifier built.
- Release CTest: 3/3 passed (`nk3_onnx_smoke`, `gameplay_judgement_benchmark`, `bms_parser_tests`).
- MSVC AddressSanitizer CTest: 2/2 passed in `RelWithDebInfo` (`gameplay_judgement_benchmark`, `bms_parser_tests_asan`). The existing ASan preset excludes its documented Windows integration cases and NK3 runtime checks.
- Privacy/credential scan of all 32 changed release files: no findings.
- The packaging script reruns every registered Release CTest before creating archives.

Commands (from the release checkout):

```powershell
cmake --build build --config Release --target tenriff bms_parser_tests nk3_onnx_smoke gameplay_judgement_benchmark --parallel 4
ctest --test-dir build -C Release --output-on-failure
cmake --build build/asan --config RelWithDebInfo --target bms_parser_tests gameplay_judgement_benchmark --parallel 4
ctest --test-dir build/asan -C RelWithDebInfo --output-on-failure
```

## Manual evidence and boundaries

The user supplied 1920x1080 screenshots of native Home, Song Select and Result.
The actual D3D11 renderer was also inspected using synthetic preview data at
1280x720 and a large window: settings and skin layout, guidance page navigation,
settings hit targets, result button alignment and performance-overlay separation.
The final empty-library text/badge adjustment passed build checks but was not
manually rechecked. Custom skins, multiplayer, all real settings persistence flows,
and the full UI audit matrix were not manually retested for this release.

Timing bars remain an estimate from mean and standard deviation, not an observed
per-hit distribution. No new result-analysis capability is claimed.

## Assets

- `TenRiff-1.7.0.zip` — Windows x64 client and replay verifier.
- `TenRiff-1.7.0-source.zip` — standalone source tree and bundled dependencies.
- `TenRiff-1.7.0-SHA256SUMS.txt` — SHA-256 hashes of both archives.
