# TenRiff Agent Notes

## Completed
- Initial CMake project scaffolding with doctest harness.
- Core BMS parser handling headers, dictionaries, and measure commands with strict/tolerant modes.
- Configurable channel-to-lane mapping defaulting to the combined 1P+2P 10-key layout.
- BMS chart normalization stage generating measure timings and sorted events with fractional #MEASURE support.

## TODO / Next Steps
- Build the scheduling/timeline stage that converts normalized events into absolute timing.
- Flesh out audio/input/render subsystems per the development manual once chart pipeline is ready.

## Working Agreements
- When you complete or significantly advance a task, update the Completed/TODO lists so the next agent has accurate context.
- Keep instructions here concise and actionable; prefer bullet lists over long prose.
