> **Historical document — local candidate before 1.9.9.5:** Behavior, unpublished status and check counts below describe that candidate only. Candidate evidence is not fresh release evidence. Current scope/colors follow [1.9.9.5](release-1.9.9.5.en.md); fresh verification follows the [public release gates](release-1.9.9.5-gate.md).

# Simple long notes, EX palette and all-combo art (local candidate)

This isolated candidate continues released 1.9.9.3 without changing the version or fixed 1.9.6 baseline.

- Native simple_rect, square, circle, diamond, arrow, triangle, pentagon and hexagon use flat lane color for LN heads/bodies/tails. Default rect/Luma Keys, ordinary taps, imported authored images/sprites, opacity, width and taper stay intact. Bundled SimpleSquare selects simple_rect.
- EX alone uses 18% lower saturation and six non-purple stops: pink, red, yellow, green, cyan, blue. There is no blue-to-pink wrap that would interpolate purple. Shared EX grades S+ through S+++, clear cards/badges, Shift Start and gauges follow this palette. EZ/NM/HD, E/F lavender and general judgement colors remain unchanged.
- ALL COMBO replaces the small COMBO label below the count only after the entire chart completes, including the last LN tail. Player and ghost are evaluated independently. Failed, aborted, automated, Auto Scratch, No Fail and partial practice runs do not qualify. Existing empty-POOR combo semantics remain.
- Fresh completed results also show the image in the max-combo cell, including when the outro delay is zero. Historical results do not infer the new badge. Legacy FULL COMBO classification and stored formats remain unchanged.
- Art is cached once per rendering device, falls back to text when missing, and respects existing combo placement/scale/count collision handling. Scoring, timing, audio, replay and peer v7 stay unchanged.

See [asset provenance](../assets/menu/achievements/README.md) and the candidate's verification report. Physical input, long sessions, other PCs and arbitrary skins are unverified.


The all-combo image has a rounded border and a subtle yellow glow.
