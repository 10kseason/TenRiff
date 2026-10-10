# TenRiff 1.8.7 Fixed Stable Baseline

Language: [Korean](baseline-1.8.7.md) | English | [简体中文](baseline-1.8.7.zh-CN.md) | [日本語](baseline-1.8.7.ja.md)

The baseline for follow-up work, pinned on 2026-10-10, is **1.8.7**. Earlier `baseline-*` documents are historical records of their named versions. The current reference is this document, [current state](current-state.en.md), and the code and regression tests at the pinned commit. Pinning the baseline does not stop development; it means the starting point for comparisons and compatibility decisions does not move automatically.

## Release Identity

- Pinned Git tag: `1.8.7`
- Pinned source commit: `6b060ad47b950ad9e0dbc516fbb40189e8e21347` (PR #81 merge)
- Release date: 2026-10-09. Baseline document pin date: 2026-10-10.
- [Version-specific release](https://github.com/10kseason/TenRiff/releases/tag/1.8.7). `latest`, a moving branch HEAD, and web deployment versions do not identify this baseline.
- Baseline platform: Windows GUI. Product chart scope: BMS family (`.bms/.bme/.bml/.pms`).
- The following SHA-256 values identify the existing published assets. Documentation changes are not presumed to be included in these ZIPs; do not replace the tag, ZIPs, or checksum file.

| Asset | SHA-256 |
|---|---|
| `TenRiff-1.8.7.zip` | `270ace26dd912228083314ccc99bb27f4e6fd1e3d617f45cffc3f82bb46d3476` |
| `TenRiff-1.8.7-source.zip` | `4c22c89827e6c460ed951c178c0371355636ee6dd2836dece52b028aeeca5ad1` |
| `TenRiff-1.8.7-SHA256SUMS.txt` | `23ff5a092690fb40b5c486eda931db47dc034f5f2328de14ca9987e26a11be99` |

## Product And Packaging Contract

- The default flow is `Title -> Song Select -> Gameplay -> Result`. Preserve local records regardless of online service failures.
- Preserve BMS parsing, the sample timeline, variable scroll, long notes, landmines, keysounds, and deterministic replay re-verification. External `.osu` charts are outside the current product scope.
- NK3 keeps P64 and the host beam safety solver authoritative. The generalized pattern MLP is used only when converting a non-10K source to 10K.
- Support native, TenRiff `skin.json`, and LR2 skins. Finished skins belong in `skins/`, the minimal template in `examples/skins/TenRiff-Example`; a profile skin with the same name takes precedence over the bundle.
- The runnable ZIP includes `Mainmusic/`, bundled skins, and NK3 runtimes/models. It excludes Songs, BGA upscaler models, and standalone BMS key-converter executables. The external upscaler defaults to OFF; selecting a model alone does not enable it.
- Public source covers the client, verifier, and build materials. Server implementations, deployment secrets, accounts, user profiles/logs/replays/results, and upload keys are excluded from distribution. Follow the [source package boundary](../README_SOURCE_PACKAGE.md).

## Input, Audio And Rules Contract

- Input timestamps use the audio playback head as their reference. Positive input calibration does not shrink with callback phase; pair each WASAPI position sample with the QPC at which that position was observed. Preserve the ASIO fallback.
- Distinguish saved RawInput/Polling settings from runtime fallback; do not overwrite the saved backend with the fallback result. Support left/right Shift and primary/secondary keys. Reassigning a key within the same key mode moves it out of its previous slot. An explicitly cleared primary key remains unassigned after saving.
- WASAPI is the default output; ASIO is optional. Open gameplay audio after chart parsing and sample-rate selection, and limit only the loading screen to 60FPS. Restore the configured frame behavior when play starts.
- Live keysounds respond immediately to physical input. They are not guaranteed to have the same timing as replay keysounds, because the replay stores only calibrated samples.
- Song ending mode is `audio_ui.play_to_end=true` (default, Listen to End) or `false` (Skip Outro). Save and submit normally completed results without another key press; input that skips the remaining outro does not change the score or replay.
- New standard plays use canonical R4. R1/R2/R3 replays, ghosts, and the verifier retain their original rules. Follow the [judgement rules](judgement-windows.md) for BMS RANK/Judge Mod timing, LN release, and gauge/score calculations.
- Preserve replay evidence v3's chart SHA-256, ruleset and result binding, deterministic input re-execution, and the boundary for verified local bests. Record the actual seed for ordinary Random and reuse it on re-execution.

## Presentation And Controls Contract

- Provide Studio Deck for Native/LR2 solo play; preserve Classic selection and the Classic fallback for external image skins, ghosts, and multiplayer. Retain riff-map visibility, dark HUD backings, and fitting of complete song titles.
- FAST/SLOW persists for 750ms and is not cleared by a subsequent PG. Text and bar positions are independent of judgement position; the bar can be conditional or always visible.
- Options has 8 cards. Open Mods from key-mode settings and NKRO Test from key settings. During play, `F5/F6` halve/double Hi-Speed, `F7/F8` adjust visual timing, `F9` captures, `F10` opens accounts, and `F11` opens chat. Distinguish menu `F5` reindexing from its gameplay action.
- GPU-batch supported note images in their original order with VSync OFF. Preserve existing paths for VSync ON, small batches, unsupported transforms/crops, and procedural LN/landmines. Do not reduce note visibility, image quality, or timing fidelity for performance.
- The HUD is a presentation layer. Visual changes must not change judgement, input, audio, replay, ghost, gauge, score, or note-batching contracts.

## Online And Trust Boundaries

- Direct-IP/LAN multiplayer for up to 8 players and a shared Rate requires compatible protocol v6 builds. Peer scores are `UNVERIFIED CLAIM`, not server-authoritative verification. Follow [multiplayer](multiplayer.md) and the [Rate guide](multiplayer-rate-build.ko.md).
- GPT Sites leaderboards contain `client_submitted` records. Do not label them as results independently verified by server-side replay execution. Connections/uploads retain the user's existing settings and consent boundaries.
- Distinguish the self-hosted account/chat/ranking path from Sites. Its challenge, exact chart-byte, and external-verifier re-verification policies are not guarantees that also apply to Sites. The historical compatibility label `TenRiff Server v1.1.0` is not evidence of current server operation or end-to-end compatibility validation.
- Preserve DPAPI session protection, masked password input, approval before opening chat URLs, and remote HTTPS requirements for the self-hosted path. Server operation, certificates, secret storage, and retention policies remain separate operational responsibilities.
- Self-hosted ports and server authentication policies follow the operational boundaries in the [server plan](ranked-integrity-plan.en.md). Public client source alone does not revalidate the server implementation or its live operation; do not interpret those port policies as Sites hosting settings.

## Verification And Limits

The following numbers are **release verification records from 2026-10-09**. They do not mean the game checks were rerun for this documentation pin. [1.8.7 release verification](release-1.8.7-gate.md) describes the checked scope and evidence locations.

- Release and independently extracted equivalent-source CTest: 3/3 each, 1,045 cases, 0 integration skips; editor: 33/33 each.
- Actual app/session: 84/84; silent WASAPI shared output: 23/23; both HUD sprite gates: 7/7 each; 17 native menus/939 hit checks; 4 presentation modes: 180/180 Presents each.
- 28 title cases/2,520 measured Presents; 6 A/B rounds on fixed synthetic scenes against 1.8.5 passed the p50 +0.03ms ceiling and p95/p99 gates; PR/main ASan and OpenVINO CI passed.
- ZIP CRC, 573 client/1,057 source file hashes, all 1,057 source files matching merged Git blobs, 0 privacy findings, and redownload comparisons of 3 public assets passed. The final ZIP's source tree matches the independently built candidate, but the final ZIP itself was not independently rebuilt again.
- Physical input, audible output, the reporter's device, Discord, long play, other PCs, online multiplayer, and preemption inside the API remain unverified. The optional external Python reference comparison was not run either.
- Remaining limits include Classic p50 +0.1939ms in a separate dynamic test, small text for very long titles, extreme HUD positions, untested LR2 variants, and differences between web Canvas and in-game presentation. Pinning the baseline does not declare these limits resolved.

## Compatibility And Update Rule

- Start follow-up work in a new branch/working copy from the commit above and compare against 1.8.7. Name the comparison version when measuring performance against another version. Preserve existing releases, installations, profiles, and experimental working copies.
- A moving `main`/`latest` or a new release alone does not automatically change the pinned baseline. When the user explicitly requests a baseline change, update the new version document and all four languages of README, documentation index, current state, and roadmap together; mark earlier documents as historical.
- Changes to replay/score/chart identity, account storage, APIs/ports/protocols require explicit compatibility decisions, necessary migrations, and cross-compatibility validation.
- Distinguish follow-up documentation commits from the release source commit. Do not move the `1.8.7` tag or regenerate/overwrite published assets under the same names.
- For settings, consult [config](config.en.md); for play, the [gameplay guide](gameplay-guide.en.md); for record trust boundaries, [score integrity](score-integrity.md) and [ranked integrity](ranked-integrity-plan.en.md).
