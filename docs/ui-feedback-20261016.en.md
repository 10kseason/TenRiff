# Home, graphics, loading and fixed grades — 1.9.1.1 follow-up

2026-10-16 · `ea8bdba0fb8aa533f0e684b64fc64515e2eb2e41`

This local candidate starts from the user-selected **1.9.1.1 release commit** and supersedes the mistaken 1.9.0-based UI candidate. It retains all 1.9.1.1 BMS TOTAL, BPM/STOP timing, RANDOM branch and online-record support, unified volumes, input behavior and language switching. Selecting this task base does not promote the pinned baseline or replace a published tag.

- Remove the V-Sync option row while retaining refresh selection and saved `graphics.vsync` compatibility.
- Add BGA brightness from 0% to 100% in 1% steps, default 100%. Dim the chart BGA before drawing notes and HUD.
- Retain the existing lower-right Home button: English → 한국어 → 日本語 → 中文, saved in the profile.
- LR2/TenRiff imports, chart loading/indexing and difficulty-table loading share one central loading panel. Unknown progress uses an animated segment.
- Grades use unrounded ordinary accuracy. Score, judgement, gauge, replay and numeric ranking rules stay intact; source records are not rewritten. No record/abort remains -- and autoplay remains AUTO.
- Website Stable means formal operating status and this fixed grade table, not server replay re-execution.

| Accuracy | Grade |
| --- | --- |
| >=99% | S+++ |
| >=98%, <99% | S++ |
| >=97%, <98% | S+ |
| >=95%, <97% | S |
| >=93%, <95% | A+ |
| >=90%, <93% | A |
| >=85%, <90% | B |
| >=80%, <85% | C |
| >=70%, <80% | D |
| Survived, <70% | E |
| Gauge failure | F |
