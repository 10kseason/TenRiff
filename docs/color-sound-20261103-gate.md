> Historical candidate verification. See [1.9.9.9 validation](release-1.9.9.9-gate.md) for the combined release gate.

# Local color/sound verification · 2026-11-03

This unpublished follow-up preserves the complete preceding local feedback candidate and release commit `28393891aad339aa8f7c21b8244a4e83a8d64422`. Fixed baseline 1.9.6, previous candidates, original sounds, releases, installations and profiles remain preserved. Use the separate `color-sound-local` profile.

| Check | Result / scope |
| --- | --- |
| MSVC x64 Release | Client, replay verifier, unit registry and native preview/smoke tools built serially with node reuse disabled |
| Final CTest | 3/3; 1,488 unit cases, zero integration skips. The optional external 10k-calc reference was unavailable |
| Focused behavior | 19 requested filters passed after repairing contextual help for the three new audio settings; initial failure retained in local evidence |
| Native menu flow | 632/632; synthetic isolated profile, no real account or Sites connection |
| Audio | 42/42: all five provided clips fully decoded and encoded to UI WAV buffers, four selectable codecs, corrupt input rejection, muted WASAPI and mock-ASIO scene ownership |
| Done duration | 144,624 decoded stereo frames at 48 kHz; identical full/bounded decoding and WAV size. The 4-second limit retains the supplied approximately 3-second clip |
| Visuals | 19 final native captures with hit verification: clear-colored lobby scores, records, result roles, FULL COMBO and four-language sound settings/result suffixes |
| UI/font preservation | 96 lobby layout expressions and four relevant font factories match the preceding candidate; ten result font factories match the public hotfix1 source |
| Pixel comparison | Six lobby cases have zero unexpected pixels outside score/FULL COMBO and the separately excluded header hover area. The ordinary result differs only in its clear-status region; all other pixels match the reference |
| Editor/catalog | Offline editor 37/37; native slot values/IDs unchanged, source references refreshed |
| Protected code | Nine gameplay, gauge, result-data, BEST comparison, replay and peer owner files byte-identical to the preceding candidate |
| Original preservation | All 1,459 preceding source files and five provided sound originals retain their recorded hashes |

Lobby score color follows the record actually displayed, rather than borrowing another record's best clear lamp. Verified whole-chart/LN completion controls FULL COMBO; online summaries and incomplete/unverified records cannot infer it from the maximum-combo number. Existing font objects, white numeric result score, cyan accuracy, judgement colors and ordinary UI roles are retained. Only semantic clear text and FULL COMBO use the existing gauge/lime palettes; FULL COMBO bloom is capped at 0.25.

Bundled sounds are found from the executable location. Optional settings preserve old saved `none`/custom choices. Dedicated slider/start/done cues suppress a generic click for the same action, and lower-priority interactions cannot discard a pending cold start/done decode. Completion cues require a successful, usable load/import/index/chart-ready boundary; failures, cancellation and empty results stay quiet. This does not install a service or alter gameplay timing/scoring or the replay/peer protocols.

The initial focused run found missing contextual help for IDs 25–27. Four-language help was added, the direct failing filter passed, and the final full CTest passed. An additional 8-frame hitmap diagnostic ended before foreground ownership was ready, so it produced no hitmap; the final 72-frame runs produced all twelve reports with verified native dispatch and no failures. No product change was needed for that diagnostic. Local packaging checks archive CRC and every member's SHA-256; detailed logs, captures, preservation manifests and archive hashes are kept outside the distributable source.

Physical listening/input, arbitrary user skins/files, long sessions, other PCs, real-account submission and actual two-PC play remain unverified. No publication, deployment, version/baseline promotion, installation replacement or profile migration was performed. UI effects retain the existing Windows system-output route; BGM retains the selected WASAPI/ASIO route.
