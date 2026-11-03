> Historical candidate verification. See [1.9.9.9 validation](release-1.9.9.9-gate.md) for the combined release gate.

# Local candidate verification · 2026-11-03

Based on release commit `28393891aad339aa8f7c21b8244a4e83a8d64422` (1.9.9.8 hot fix 1). This is an unpublished local candidate. Fixed baseline 1.9.6, original release source, installed profiles and previous artifacts are preserved.

| Check | Result / scope |
| --- | --- |
| MSVC x64 Release | Client, unit tests, replay verifier and native preview/smoke tools built |
| Final CTest | 3/3; unit registry 1,481/1,481 with zero integration skips |
| Focused behavior | 25/25: score vs clear BEST, 1.2-second reveal, rank upgrade, cumulative gauge graph, file settings, async sound bounds, reference replay isolation, profile/preset round trips and arrows |
| Native menu flow | 632/632 on the functional candidate; the later default-position adjustment was checked by final unit/visual runs |
| Audio | 31/31 after the options-music correction: synthetic WAV/MP3/OGG/FLAC decode, bounded UI clips, corrupt-file rejection, endpoint/driver routing and scene ownership |
| Visuals | Final 10 cases include corrected Classic/Studio placement, editable skin controls, actual raw-history gauge projection and four-language coverage. Earlier 17 boundary captures remain in local evidence |
| Transition export | 30 native-rendered frames, 30 fps, exactly 1.000 seconds; production playback uses the same quantized timeline |
| Offline editor | 37/37. Existing native slot values and IDs unchanged; source references refreshed |
| Protected owners | Timing engine, gauge math, raw result schema/stats, record comparator, replay I/O/types/verifier and peer protocol: 9 files byte-identical to the release |

NEW RECORD is an eligible score increase of at least one point, independent of the official clear-first BEST. Rank targets use the existing categorical-accuracy thresholds, not guessed native-score thresholds. Source/profile/replay file formats retain their prior compatibility; new settings are optional and default safely.

The initial parallel build stalled and was replaced by a serial build with MSBuild node reuse disabled. Initial lambda-default and forward-declaration compilation failures were corrected. Initial test failures exposed missing new-setting help and outdated/new fixtures; those were repaired. Four loopback peer tests failed inside the restricted sandbox, then passed in the general-permission full suite; no peer implementation change was made. The initial Studio placement overlapped its score deck and was moved to X0%/Y28%. Earlier diagnostics remain in the candidate's local evidence directory.

No physical listening, physical key feel, arbitrary user files/skins, extended sessions, other PCs, real-account submissions or two-PC matches are claimed. No publication, deployment, baseline promotion, installation replacement or profile migration was performed. Local archives are verified by CRC and all-file SHA-256 before handoff.
