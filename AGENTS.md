# TenRiff Agent Notes

## Completed
- Initial CMake project scaffolding with doctest harness.
- Core BMS parser handling headers, dictionaries, and measure commands with strict/tolerant modes.
- Configurable channel-to-lane mapping defaulting to the combined 1P+2P 10-key layout.
- BMS chart normalization stage generating measure timings and sorted events with fractional #MEASURE support.
- Scheduling/timeline stage translating normalized events into absolute timing with BPM/STOP handling.
- Gameplay utility layer with SpeedManager (rate/HS calculations) and GaugeManager implementing the auto-shift gauge spec.
- Initial osu!mania loader capturing mode/key count, timing points, and columnized notes with validation helpers.
- Cross-platform launcher bootstrap scripts (.bat/.sh) that sanity-check assets/songs, seed default profiles, and surface exit-code hints.
- Timeline conversion now yields **sample-aligned int64 timestamps** so the audio thread can consume deterministic events.
- Timing/Input scaffolding added: ClockSync regression estimator, POD InputEvent, and lock-free SPSC queue ready for platform hooks.
- Low-latency menu blueprint covering menu state machine, SongIndexerThread, cached song index, and latency-first settings/inputs.

## TODO / Next Steps
- Flesh out audio/input/render subsystems per the development manual once chart pipeline is ready.
- Track staged implementation roadmap in `docs/roadmap.md` (Audio master clock → full song loop → key remap/8K-10K → FR/SR random → launcher).
- Hook the osu!mania loader into the shared normalization/scheduling pipeline and expose format selection in song select.
- Extend launcher to inspect real TenRiff binary metadata (version, build info), surface SDL2/VC++ runtime guidance, and collect/tail logs per exit code.
- Implement the menu state machine (Title/SongSelect/Gameplay/Result) with live audio clocking, async SongIndexerThread + cache, and latency-first settings/remap screens.

## Working Agreements
- When you complete or significantly advance a task, update the Completed/TODO lists so the next agent has accurate context.
- Keep instructions here concise and actionable; prefer bullet lists over long prose.
