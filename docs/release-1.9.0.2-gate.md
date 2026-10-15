# TenRiff 1.9.0.2 release scope and verification gates

Requested release line: **1.9.0.2**, 2026-10-15, based on published 1.9.0.1. The user-pinned baseline remains **1.9.0**; this release does not advance it. Preserve previous tags, release assets, installations, profiles and original chart/audio inputs. [Baseline](baseline-1.9.0.en.md) · [한국어](feedback-1.9.0.2.ko.md) · [English](feedback-1.9.0.2.en.md) · [日本語](feedback-1.9.0.2.ja.md) · [简体中文](feedback-1.9.0.2.zh-CN.md).

**The [1.9.0.2 release page](https://github.com/10kseason/TenRiff/releases/tag/1.9.0.2) is the sole authority for publication status and available downloads.** These source documents define scope and record local evidence; they do not assert that publication, PR/main CI or public asset verification has completed. The release page and checksum asset identify the final source and delivered archives.

## Changes and compatibility

- Make the lobby Options icon, border and glow green while retaining its white text and existing icon alignment.
- Show green ⑤ only when the built-in 5K Aery difficulty table is selected. Omit it under default Lv and other/custom tables; levels stay red and scores stay white. Preserve stored difficulty-table labels, original charts and record identities.
- Add case-insensitive `lv:16to20`, `Lv:16`, `lv:19-` and `lv:18+` searches for inclusive range, exact level, maximum and minimum. Combine them with ordinary search terms and existing filters. Use the selected table's numeric level, or the chart/BMS level under default Lv; unranked/nonnumeric table entries do not fall back to native levels. Share the numeric filter's integer interpretation: `st18` and `18+` both mean level 18. Incomplete/malformed expressions remain literal search terms.
- Accept shifted `:` and `+` and the minus key through the menu search input path without letting modifiers cancel editing or punctuation change gameplay speed. Stored key names and global keymap format remain unchanged.
- Default new/missing debounce settings to 0ms. Preserve saved values and the historical 5ms-to-8ms migration independently of the new default.
- Adjust all five audio-volume controls in one-percentage-point steps, including sliders. Keep master/BMS/TenRiff music ranges at 0–100%, BGM/keysounds at 0–200%, master default at 10%, other defaults at 100%, and saved values intact.

Retain the [1.9.0.1 changes and compatibility policy](release-1.9.0.1-gate.md), including preview ×0.75, assignable Hi-Speed controls, scoped XInput and native dialogs. Numerical R1–R6 rules, replay contracts and original files remain. **Affected BMS timing-v2 records remain local until online support is added.** No web/API/database deployment, ranking migration, baseline promotion or installation overwrite is part of this release.

## Recorded local evidence

Before the final Aery ⑤ restoration, the development candidate and a fresh, independently extracted source build both passed:

| Check | Development build | Independent extracted-source build |
| --- | --- | --- |
| Windows Release configure/build | Passed | Passed in a new build directory |
| CTest | 3/3; 1,224 cases; 0 integration skips | 3/3; 1,224 cases; 0 integration skips |
| Skin editor | 35/35 | 35/35 |
| Native MenuApp/GameSession | 307/307 | 307/307 |

Native smoke includes 22 search key-handling checks through the actual MenuApp internal input path. It uses internal input dispatch, not physical OS key or controller input. Before that follow-up, six native render fixtures passed 360 Presents and 86 geometry hit checks. Those captures predate the Aery-specific restoration and do not establish its final behavior. Geometry checks do not establish OS click delivery; Presents do not establish pixel equality across all configurations.

Local package CRC, extracted-file hashes, source-file-set, clean-default-profile and defined privacy-pattern checks passed before publication preparation. Source comparison allowed CRLF/LF normalization; it was not a claim that every source file matched raw Git blob bytes. The independent build used the exact extracted ZIP bytes. Refresh the final source package after the Aery follow-up and documentation changes and record its final source correspondence and hashes separately.

Complete logs and initial failures remain in local evidence outside the public source bundle. Local checks do not substitute for the remaining publication gates below.

## Initial failures and retained uncertainty

- Unsupported `CAPTURE` diagnostics caused the first test build to fail. Removing only those diagnostic lines retained the assertions; the subsequent build passed.
- The first native search run missed minus key `SC_0C` and passed 306/307. A search-specific key-code comparison fixed the input path; final native and full regression checks passed.
- The first packaging gate failed the existing `skin lane number shortcuts honor visible order bounds and modal ownership` test once. The same test executable then passed the focused retry without a code change, followed by the full packaging CTest retry and independent build checks. **The root cause remains unconfirmed.** Preserve the initial failure; retries do not prove that the intermittent cause was fixed or justify excluding the test.

## Remaining publication gates

- Final Release build and relevant regression checks after restoring Aery ⑤; native captures for built-in 5K Aery at FHD/small sizes, plus default Lv and custom tables to verify suppression elsewhere.
- Final client/source ZIP CRC, exact extracted-file hashes, source-to-final-Git correspondence, safe defaults and privacy checks after publication preparation; retain independent-build source equivalence evidence.
- Successful PR and post-merge main CI for ASan and OpenVINO, recorded against their exact revisions.
- Fresh draft and anonymous public downloads of client ZIP, source ZIP and SHA256SUMS; compare hashes, sizes and CRC.
- Confirm previous release/tag/asset identities remain unchanged. Never move an existing published tag or replace a prior release asset.

Completion and exact identities belong in the version-specific release record. This gate does not mark any remaining item passed in advance.

## Remaining limits

Physical keyboard layouts, Xbox/XInput hardware, input/audio latency, full-song listening, long play, other PCs and end-to-end multiplayer remain unverified by these checks. The intermittent skin-shortcut test cause is unresolved. Online timing-v2 support, plugin runtime/API/SDK and major GameSession restructuring remain separate work. Any future fixed-baseline change requires explicit user selection and synchronized compatibility evidence.
