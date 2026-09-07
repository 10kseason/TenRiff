# TenRiff 1.7.1 Gameplay & Multiplayer Polish

This patch carries the client follow-up described in [gameplay-polish-followup.md](gameplay-polish-followup.md)
and the Song Select difficulty-table control in [menu-visual-polish.md](menu-visual-polish.md).
It does not alter scoring formulas, judgement rules, replay formats, the network protocol or server code.

## Release scope

- Full-room multiplayer HUD/results, competition ranks for ties, missing-score states and all-opponent spectator completion.
- Score comparison scales with the displayed scores and sits above the opponent list.
- P-GREAT-only pop/rainbow effects; solid lower judgements; independent judgement/combo X/Y settings and one-sided FAST/SLOW.
- Render-time key-beam decay, optional gameplay audio normalization (OFF by default), ten pastel Options cards and readable LEVEL labels.
- Song Select difficulty-table name, URL editor, local JSON picker and native-LV reset using the existing table import/indexing path.

## Verification gates

The playable and source archives are produced from the curated Git checkout. Before publication:

1. Build the Windows x64 client, replay verifier and all Release test targets from that checkout.
2. Pass all three Release CTest entries. The packager reruns these checks and refuses a failed/empty test inventory.
3. Pass both MSVC AddressSanitizer CTest entries using the existing preset and its documented integration exclusions.
4. Inspect the exact Git diff and scan changed files and archives for credentials, runtime profiles/logs, unsafe paths and developer-only binaries.
5. Verify archive CRCs, SHA-256 sums and x64 PE headers; rebuild and test the extracted source archive.
6. Push the commit and annotated tag, verify remote identities and CI, upload assets and compare their remote digests/sizes.

```powershell
cmake --build build --config Release --target tenriff tenriff_replay_verifier bms_parser_tests nk3_onnx_smoke gameplay_judgement_benchmark --parallel 4
ctest --test-dir build -C Release --output-on-failure
cmake --build build/asan --config RelWithDebInfo --target bms_parser_tests gameplay_judgement_benchmark --parallel 4
ctest --test-dir build/asan -C RelWithDebInfo --output-on-failure
```

## Manual evidence and limitations

The actual D3D11 renderer was exercised with synthetic fixtures: 960x540 Options card selection,
eight-player results, three/eight-player live HUDs, P-GREAT/GREAT/GOOD feedback, one-sided timing
labels and independently moved judgement/combo text. A large window requested as 1080p was fitted
to the desktop by Windows. The Song Select table card was checked at small/large sizes, including
URL-editor open/cancel, native-LV reset and the File hit target. These checks use no accounts or records.

Unit/integration tests cover room projection, ties/missing scores, overflow-safe score comparison,
five-column navigation/routes, config persistence, normalization channel balance/buffer independence,
deterministic effect decay and the actual GameSession input-to-voice-to-mixer path.
Real remote multiplayer, hardware audio, every imported skin and sustained 144 FPS remain manual
validation boundaries. The table editor's actual remote import and file-picker persistence were not
manually exercised with user data; the existing table importer has deterministic fixture tests.

The reported 1.7.0 LN release double-sound could not be reproduced. Both normal/release-judged holds
produce one head sound and no additional release sound in the production mixer test for early,
exact and late releases. This release does not claim that report is fixed.

## Assets

- `TenRiff-1.7.1.zip` — Windows x64 client, replay verifier and runtime assets.
- `TenRiff-1.7.1-source.zip` — standalone source and bundled build dependencies.
- `TenRiff-1.7.1-SHA256SUMS.txt` — SHA-256 hashes for both ZIPs.
