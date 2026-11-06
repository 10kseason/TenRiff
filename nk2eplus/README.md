# KeyWeaver E+ import

KeyWeaver 1.4.0, commit `fb15fb2e44c3c14450e36877d4df96a865bb5553`, MIT (LICENSE).
The complete NK2 engine subset and its three public data headers are isolated as
`keyweaver_eplus`. TenRiff adds a 7K target and copies phrase-search histories
only after candidate ranking. `provenance.json` records source, import and local
adaptation hashes. Historical `nk2/` and `nk2e/` remain frozen.

TenRiff selects Faithful + phrase search/support/dense LN/source LN + full-field
and tap boost. Explicit 7K, 8K and 10K targets also remix equal-key charts.
The independent upstream audit remains enabled. The adapter preserves original
sample times, LN semantics and audio, keeps generated notes silent, and checks
collisions at sample resolution. Base E support taps at LN tails use the exact
retained tail sample. Tap boost uses retained heads. A safety failure retains
the source chart.

Phrase spacing and the beat-based added-tap repeat guard use TenRiff's runtime
base BPM, as in the existing E adapter. The conversion boundary does not expose
the complete BPM-change map, so this is the upstream recipe with TenRiff timing
restoration, not a claim of identical output to KeyWeaver for every variable-BPM
chart. Sub-millisecond LNs that collapse upstream are rejected safely.

Replays use `fullfield_taps_eplus_v2` for 7K and for 8K/10K conversions that
restore LN-tail accents. Other successful 8K/10K conversions retain
`fullfield_taps_eplus_v1`. Replaying v1 keeps its original fallback behavior.
