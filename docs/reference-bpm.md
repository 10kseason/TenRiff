# Reference BPM

[한국어](reference-bpm.ko.md) · [日本語](reference-bpm.ja.md) · [简体中文](reference-bpm.zh-CN.md)

The song list and gameplay Hi-Speed reference use the BPM with the longest
accumulated running time in the chart. Repeated sections at the same BPM add
together. This is based on elapsed seconds, not the number of notes or beats.

- BMS channel `03`, extended/fractional `#BPMxx` through channel `08`, and
  fractional measure lengths use the same normalized timeline as gameplay.
- Counting starts at chart position zero and includes the last measure containing
  a nonzero event, including long-note tails and BGM/media commands. Trailing
  length-only (`02`) or all-`00` measures do not count. This prevents empty editor
  padding after a 0.99 BPM outro from replacing Andro's 165 BPM reference.
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

Song-index cache version **22** invalidates stored references that included empty
trailing measures. This rebuilds derived metadata, without changing records. Both
Safe and Fast indexing compute the new reference using their existing timing/NPS
pass, without loading extra media payloads or adding a second timeline pass.

The scheduled chart duration, note times, scroll segments, and replay timing
versions remain unchanged; only the display/Hi-Speed reference excludes padding.
