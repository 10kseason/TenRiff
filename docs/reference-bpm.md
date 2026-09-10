# Reference BPM

The song list and gameplay Hi-Speed reference use the BPM with the longest
accumulated running time in the chart. Repeated sections at the same BPM add
together. This is based on elapsed seconds, not the number of notes or beats.

- BMS channel `03`, extended/fractional `#BPMxx` through channel `08`, and
  fractional measure lengths use the same normalized timeline as gameplay.
- Counting starts at chart position zero and includes the final defined measure,
  including long-note tails and trailing measures declared by BGM/media commands.
  Undeclared trailing audio-file silence is not included.
- `#STOP` waiting time is excluded. `#SCROLL` does not alter tempo weighting,
  including zero and reverse scroll factors.
- Equal durations within one nanosecond choose the first tempo that actually
  advances chart time. A zero-duration chart falls back to its valid initial BPM;
  invalid/failed timelines provide no reference (`0`).
- Durations are accumulated before sample rounding. The menu's 1 kHz timeline
  and 44.1/48/96 kHz audio timelines therefore choose the same reference; Rate
  does not change the choice.

`ChartLoadResult.base_bpm` remains the initial header BPM for replay verification
and deterministic key/LN conversion. `reference_bpm` is separate so existing
replays are not reinterpreted by this display/scroll change.

Song-index cache version **15** invalidates older stored header BPM values. Both
Safe and Fast indexing compute the new reference using their existing timing/NPS
pass, without loading extra media payloads or adding a second timeline pass.
