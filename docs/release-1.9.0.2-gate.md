# TenRiff 1.9.0.2 release scope and verification gates

Requested release line: **1.9.0.2**, 2026-10-15, based on published 1.9.0.1. The user-pinned baseline remains **1.9.0**; this release does not advance it. Preserve previous tags, release assets, installations, profiles and original chart/audio inputs. [Baseline](baseline-1.9.0.en.md) · [한국어](feedback-1.9.0.2.ko.md) · [English](feedback-1.9.0.2.en.md) · [日本語](feedback-1.9.0.2.ja.md) · [简体中文](feedback-1.9.0.2.zh-CN.md).

**The [1.9.0.2 release page](https://github.com/10kseason/TenRiff/releases/tag/1.9.0.2) is the sole authority for publication status and available downloads.** These source documents define scope and record local evidence; they do not assert that publication, PR/main CI or public asset verification has completed. The release page and checksum asset identify the final source and delivered archives.

## Changes and compatibility

- Make the lobby Options icon, border and glow green while retaining its white text and existing icon alignment.
- Show green ⑤ only when the built-in 5K Aery difficulty table is selected. Omit it under default Lv and other/custom tables; levels stay red and scores stay white. Preserve stored difficulty-table labels, original charts and record identities.
- Color the complete registered symbols `4UE`, `6UE`, `8UE` and Revive10 `Ⓡ` with existing key-mode colors: 4K `#FF9BC5`, 6K `#8FB8FF`, 8K `#FFD479`, 10K `#63E9F2`. Eligibility requires the registered URL, exact table symbol and a nonempty level; numeric row levels remain red. Revive10 uses `Ⓡ`, not `10`. Preserve green Aery 5K ⑤ and the existing 7K Aery/Stella/Satellite presentation. Exclude default Lv and unrecognized custom symbols.
- Move native Song Select Sort/Filter and Difficulty Table selector boxes from the lower center into the header beside Song Library. Use the freed center space for a taller album cover while preserving the best-record area and header song count. This applies to the default compact layout; authored native/legacy left/center/header rectangles keep their prior placement. Song sources and records remain unchanged.
- Add case-insensitive `lv:16to20`, `Lv:16`, `lv:19-` and `lv:18+` searches for inclusive range, exact level, maximum and minimum. Combine them with ordinary search terms and existing filters. Use the selected table's numeric level, or the chart/BMS level under default Lv; unranked/nonnumeric table entries do not fall back to native levels. Share the numeric filter's integer interpretation: `st18` and `18+` both mean level 18. Incomplete/malformed expressions remain literal search terms.
- Accept shifted `:` and `+` and the minus key through the menu search input path without letting modifiers cancel editing or punctuation change gameplay speed. Stored key names and global keymap format remain unchanged.
- Default new/missing debounce settings to 0ms. Preserve saved values and the historical 5ms-to-8ms migration independently of the new default.
- Adjust all five audio-volume controls in one-percentage-point steps, including sliders. Keep master/BMS/TenRiff music ranges at 0–100%, BGM/keysounds at 0–200%, master default at 10%, other defaults at 100%, and saved values intact.

Retain the [1.9.0.1 changes and compatibility policy](release-1.9.0.1-gate.md), including preview ×0.75, assignable Hi-Speed controls, scoped XInput and native dialogs. Numerical R1–R6 rules, replay contracts and original files remain. **Affected BMS timing-v2 records remain local until online support is added.** No web/API/database deployment, ranking migration, baseline promotion or installation overwrite is part of this release.

## Recorded local evidence

Before the final Aery ⑤, registered-table symbol-color and Song Select layout follow-ups, the development candidate and a fresh, independently extracted source build both passed:

| Check | Development build | Independent extracted-source build |
| --- | --- | --- |
| Windows Release configure/build | Passed | Passed in a new build directory |
| CTest | 3/3; 1,224 cases; 0 integration skips | 3/3; 1,224 cases; 0 integration skips |
| Skin editor | 35/35 | 35/35 |
| Native MenuApp/GameSession | 307/307 | 307/307 |

Native smoke includes 22 search key-handling checks through the actual MenuApp internal input path. It uses internal input dispatch, not physical OS key or controller input. Before that follow-up, six native render fixtures passed 360 Presents and 86 geometry hit checks. Those captures predate these display follow-ups and do not establish their final behavior. Geometry checks do not establish OS click delivery; Presents do not establish pixel equality across all configurations.

The final runtime Release build completed. The latest visual verification passed **24 fixtures, 1,440 Presents, 381 geometry checks and 24 table-prefix pixel checks**. These cover layout/color/pixel checks and render liveness. OS `native_dispatch` flags were not all true; do not treat geometry success as proof that every OS click or key-delivery check passed.

Local package CRC, extracted-file hashes, source-file-set, clean-default-profile and defined privacy-pattern checks passed before publication preparation. Source comparison allowed CRLF/LF normalization; it was not a claim that every source file matched raw Git blob bytes. The independent build used the exact extracted ZIP bytes. Refresh the final source package after the Aery/table-symbol/layout follow-ups and documentation changes and record its final source correspondence and hashes separately.

Complete logs and initial failures remain in local evidence outside the public source bundle. Local checks do not substitute for the remaining publication gates below.

## Initial failures and retained uncertainty

- Unsupported `CAPTURE` diagnostics caused the first test build to fail. Removing only those diagnostic lines retained the assertions; the subsequent build passed.
- The first native search run missed minus key `SC_0C` and passed 306/307. A search-specific key-code comparison fixed the input path; final native and full regression checks passed.
- The first packaging gate failed the existing `skin lane number shortcuts honor visible order bounds and modal ownership` test once. The same test executable then passed the focused retry without a code change, followed by the full packaging CTest retry and independent build checks. **The root cause remains unconfirmed.** Preserve the initial failure; retries do not prove that the intermittent cause was fixed or justify excluding the test.

- The first `smoke-layout` passed **305/307**: a 15-second watchdog aborted before replay input (`aborted=true`, all judgements zero); the replay claims mismatch was downstream. Initialization delay versus audio-clock stall remains unresolved for that run. Diagnostic-only changes retained timeouts and assertions.
- The next `smoke-layout-final` passed **304/307**. Timing passed (`timeout=0`, initialization 14ms, run 3,492ms, `max_sample=153467`, replay verified); `keymap.last_key_stops_capture`, `keymap.sequential_profile_readback` and `keymap.secondary_advances_in_same_slot` failed. Inspection confirmed that capture clicks refresh live `GetAsyncKeyState`; held W/E/R can suppress the first synthetic down, keep capture active and make the secondary click ineffective. **The exact held key was not recorded, so this causal path is not proven for that run.** Test-only isolation stops/drains the input thread, clears pressed/owner state before synthetic capture, and restarts afterward. Product code and the held-repeat assertion are unchanged. Final isolated rerun and independent checks remain pending.

## Remaining publication gates

- Complete the final isolated native rerun and independent-source build/regression checks after the Aery/table-symbol/layout follow-ups and diagnostic addition. Retain the 305/307 and 304/307 failures and distinguish it from the earlier passing candidate and completed 24-fixture visual checks. Preserve the authored-layout and header-count/best-record boundaries. Keep the eight-table live header/data hashes and counts in separate local audit evidence; do not bundle full remote table data.
- Final client/source ZIP CRC, exact extracted-file hashes, source-to-final-Git correspondence, safe defaults and privacy checks after publication preparation; retain independent-build source equivalence evidence.
- Successful PR and post-merge main CI for ASan and OpenVINO, recorded against their exact revisions.
- Fresh draft and anonymous public downloads of client ZIP, source ZIP and SHA256SUMS; compare hashes, sizes and CRC.
- Confirm previous release/tag/asset identities remain unchanged. Never move an existing published tag or replace a prior release asset.

Completion and exact identities belong in the version-specific release record. This gate does not mark any remaining item passed in advance.

## Remaining limits

Physical keyboard layouts, Xbox/XInput hardware, input/audio latency, full-song listening, long play, other PCs and end-to-end multiplayer remain unverified by these checks. The intermittent skin-shortcut test cause is unresolved. Online timing-v2 support, plugin runtime/API/SDK and major GameSession restructuring remain separate work. Any future fixed-baseline change requires explicit user selection and synchronized compatibility evidence.
