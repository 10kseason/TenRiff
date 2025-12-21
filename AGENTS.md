# TenRiff Agent Notes

## Completed
- Initial CMake project scaffolding with doctest harness.
- Core BMS parser handling headers, dictionaries, and measure commands with strict/tolerant modes.
- Configurable channel-to-lane mapping defaulting to the combined 1P+2P 10-key layout.
- BMS chart normalization stage generating measure timings and sorted events with fractional #MEASURE support.
- Scheduling/timeline stage translating normalized events into absolute timing with BPM/STOP handling.
- Gameplay utility layer with SpeedManager (rate/HS calculations) and GaugeManager implementing the auto-shift gauge spec.

## TODO / Next Steps
- Flesh out audio/input/render subsystems per the development manual once chart pipeline is ready.

## Working Agreements
- When you complete or significantly advance a task, update the Completed/TODO lists so the next agent has accurate context.
- Keep instructions here concise and actionable; prefer bullet lists over long prose.
