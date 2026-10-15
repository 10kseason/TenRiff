# TenRiff 1.9.0.2 release feedback · 2026-10-15

This **1.9.0.2 release candidate** is based on published 1.9.0.1. The fixed baseline remains [1.9.0](baseline-1.9.0.en.md); previous releases, installations, profiles and original charts are preserved. Only the [1.9.0.2 release page](https://github.com/10kseason/TenRiff/releases/tag/1.9.0.2) determines publication status and available downloads. The [release gate](release-1.9.0.2-gate.md) distinguishes local evidence from remaining publication checks.

## Lobby and Song Select

- Display Options in green.
- Show green ⑤ only when the built-in 5K Aery difficulty table is selected. Default Lv and other tables omit ⑤; levels stay red and scores stay white. Original chart and difficulty-table data are preserved.

## Level search

Use these expressions in the search box. Both `lv` and `to` are case-insensitive, and range endpoints are inclusive.

| Input | Included levels |
| --- | --- |
| `lv:16to20` | 16 through 20 |
| `Lv:16` | Exactly 16 |
| `lv:19-` | At most 19 |
| `lv:18+` | At least 18 |

With a difficulty table selected, search uses **that table's level**. Under default Lv it uses the chart/BMS level. Unranked or nonnumeric table entries do not fall back to a BMS level. Combine ordinary terms with spaces, such as `artist lv:16to20`, to require both conditions. Incomplete or malformed `lv:` expressions remain ordinary search terms. Search uses the same integer interpretation as the numeric level filter: table labels `st18` and `18+` both match numeric level 18.

## Input and volume settings

- New profiles and missing `input.debounce_ms` values now default to **0ms**, previously 8ms. This default change does not reset saved values. The historical **5ms → 8ms migration remains**; compatibility with older settings is separate from the new default.
- All five audio UI volumes—master, BMS song, TenRiff music, BGM and keysounds—use **1 percentage-point steps (0.01 in saved values)** for finer adjustments.

| Setting | Range | New default |
| --- | --- | --- |
| Master `audio.volume` | 0–100% | 10% |
| BMS song `audio.gameplay_volume` | 0–100% | 100% |
| TenRiff music `audio.menu_music_volume` | 0–100% | 100% |
| BGM `audio.bgm_volume` | 0–200% | 100% |
| Keysounds `audio.keysound_volume` | 0–200% | 100% |

Saved volumes and existing output multipliers, including preview ×0.75, are retained. See the [configuration reference](config.en.md).

## Validation and compatibility

These results describe the candidate before the final Aery ⑤ restoration. The development build and independently extracted source build each passed the Release build, CTest 3/3 (1,224 cases each, 0 integration skips), editor 35/35 and native MenuApp/GameSession 307/307. Native smoke includes 22 search key-handling checks. The earlier render passed six fixtures/360 Presents/86 geometry hit checks. Post-restoration build and Aery FHD/small-window, default Lv and custom-table display checks remain separate release gates.

The initial build failed on unsupported test diagnostic CAPTURE; those diagnostic lines were removed. The first native search run missed the minus key SC_0C; a search-specific key-code comparison corrected it before the final passing checks above. Initial failure logs, original inputs and existing records are preserved.

The first packaging gate failed an existing skin lane-number shortcut test once. A focused retry with the unchanged test executable, the full CTest retry and independent build checks passed, but **the root cause remains unconfirmed**. Preserve the original failure; passing retries do not establish a fix for the intermittent cause.

Local package/independent-build evidence and initial failures remain in separate evidence. Final package correspondence, PR/main CI and public asset checks follow the [release gate](release-1.9.0.2-gate.md); this document does not mark them passed in advance. The release page determines actual publication status. Internal key dispatch and geometry checks do not establish physical-device input coverage; hardware, latency, long play and other PCs remain unverified. The fixed baseline remains 1.9.0.

[BMS timing and replay compatibility from 1.9.0.1](feedback-20261015.en.md) remain: affected timing-v2 records stay local until web support is added. This candidate does not deploy a website/API/database, promote the fixed baseline or overwrite an installation.

Language: [한국어](feedback-1.9.0.2.ko.md) | English | [日本語](feedback-1.9.0.2.ja.md) | [简体中文](feedback-1.9.0.2.zh-CN.md)
