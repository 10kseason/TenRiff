# TenRiff 1.9.0.2 local feedback candidate · 2026-10-15

This local candidate is based on published 1.9.0.1. **Local checks passed; it is not published.** The fixed baseline remains [1.9.0](baseline-1.9.0.en.md). Previous releases, installations, profiles and original charts are preserved. Package and independently extracted source-build results belong to the separate local verification record; publication is a separate step.

## Lobby and Song Select

- Display Options in green.
- Remove ⑤ from Song Select, display levels in red and scores in white. This changes presentation, not the original chart or difficulty-table data.

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

The Release build, final CTest 3/3 (1,224 cases, 0 integration skips), editor 35/35, and native MenuApp/GameSession 307/307 (including 22 search key-handling checks) passed. Six native render fixtures/360 Presents and 86 geometry hit checks passed. FHD/960×540 capture review confirmed green Options, red levels, white scores and removal of ⑤.

The initial build failed on unsupported test diagnostic CAPTURE; those diagnostic lines were removed. The first native search run missed the minus key SC_0C; a search-specific key-code comparison corrected it before the final passing checks above. Initial failure logs, original inputs and existing records are preserved.

Consult the separate local verification record for package and independently extracted source-build results. Nothing has been published. Detailed results and logs are recorded separately in local evidence. Internal key dispatch does not establish physical-device input coverage; physical devices, input latency, long play and other PCs remain unverified. The fixed baseline remains 1.9.0.

[BMS timing and replay compatibility from 1.9.0.1](feedback-20261015.en.md) remain: affected timing-v2 records stay local until web support is added. This candidate does not deploy a website/API/database, promote the fixed baseline or overwrite an installation.

Language: [한국어](feedback-1.9.0.2.ko.md) | English | [日本語](feedback-1.9.0.2.ja.md) | [简体中文](feedback-1.9.0.2.zh-CN.md)
