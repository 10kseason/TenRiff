# TenRiff 1.9.4 — Memory reclamation and play controls

Version 1.9.4 continues user-selected 1.9.3 (`bf544e17f22879011a04a7fd157e40b57f6598bb`). General fixed baseline 1.9.0, prior published assets, installations and profiles remain preserved.

- Decoded chart PCM is retained through play and pause instead of runtime budget eviction. Shutdown releases it after audio/decode workers stop. Startup prioritization and subsequent prefetch remain.
- Static BGA bitmaps remain beyond the former 256-image/192MiB LRU limits and clear on return to a menu. Forced indexer heap/working-set trims coalesce during loading, play and pause, then run at a menu collection point or an indexing request after session shutdown.
- TenRiff is native C++ without a managed GC. This policy covers those explicit cache evictions and forced trims; ordinary object lifetimes, video frame recycling, asynchronous cancellation and device-loss recovery remain. It does not prevent every allocation/free or OS paging.
- PCM accumulates until session shutdown; static BGA accumulates until returning to a menu, potentially across several charts in a continuous course. Very large charts can increase memory pressure. No measured stutter or physical latency improvement is claimed.
- Indicator Shape selects Dot or Barcode. Always keeps the scale/center visible; individual markers expire after 0.7 seconds of game time. Existing FAST/SLOW text retention remains.
- WASAPI output selection applies to gameplay and BMS previews. ASIO retains its driver selector; menu/result music uses the Windows default MCI output. An unavailable selected endpoint never silently changes to another output.
- Profiles assign/clear F1–F11 actions and show their current keys in hints. F5/F6 −10/+10 becomes ×0.5/×2, once per press, within 0.5–50. F3/F4 ±0.25, Song Rate and judgements remain unchanged.
- BMS measure lines attach to note lower edges for player/ghost and configured note height without changing chart timestamps.

See [input/indicator compatibility](input-feedback-20261017.en.md). BGA weights remain excluded, OFF/empty/Auto defaults remain, and Guest exclusion, R7/R8, historical replays and fixed grades are preserved. No website/database change.

See the [release verification procedure](release-1.9.4-gate.md) for gates and limitations. The [1.9.4 release](https://github.com/10kseason/TenRiff/releases/tag/1.9.4) is authoritative for publication and downloads.
