# KeyWeaver E+ import

KeyWeaver 1.4.0, commit `fb15fb2e44c3c14450e36877d4df96a865bb5553`, MIT (LICENSE).
The complete NK2 engine subset and its three public data headers are isolated as
`keyweaver_eplus`; only namespace/include paths are adapted. `provenance.json`
records source and imported hashes. Historical `nk2/` and `nk2e/` remain frozen.

TenRiff selects Faithful + phrase search/support/dense LN/source LN + full-field
and tap boost, exactly the upstream E+ - Full-field + taps recipe. Output is
restricted to explicit 8K/10K, including equal-key remix. No F boost or models.
The independent upstream audit remains enabled. The adapter preserves original
sample times, LN semantics and audio, keeps generated notes silent, restores tap
heads to retained source samples, and checks collisions at sample resolution.
Failure retains the source chart; no legacy pattern fallback is used.

Phrase spacing and the beat-based added-tap repeat guard use TenRiff's runtime
base BPM, as in the existing E adapter. The conversion boundary does not expose
the complete BPM-change map, so this is the upstream recipe with TenRiff timing
restoration, not a claim of identical output to KeyWeaver for every variable-BPM
chart. Sub-millisecond LNs that collapse upstream are rejected safely.

Stable preset identity: `fullfield_taps_eplus_v1`. Do not change this engine's
behavior for existing replays; future behavior needs a new identity.
