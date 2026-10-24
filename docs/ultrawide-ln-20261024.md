> **Historical note · superseded by the 1.9.9.1 scope:** unpublished status and candidate evidence below describe development on 2026-10-24. See [1.9.9.1](release-1.9.9.1.en.md) and its [release gates](release-1.9.9.1-gate.md) for current scope, compatibility and separate verification. This is not a publication claim; the original candidate directory is preserved.

# Local 1.9.9 follow-up: ultrawide displays and long notes

This unpublished candidate starts from 1.9.9 (`79433ba`). The general pinned baseline stays 1.9.6; released sources, packages, profiles and charts are preserved. [한국어](ultrawide-ln-20261024.ko.md) · [日本語](ultrawide-ln-20261024.ja.md) · [简体中文](ultrawide-ln-20261024.zh-CN.md)

- **21:9:** 2560×1080 and 3440×1440 remain supported; 5120×2160 joins the preset list. Full-screen backgrounds, BGA and dimming extend into the side areas. The authored 1920×1080 UI, note proportions, pointer mapping and saved skin positions stay centered and uniformly scaled. Background art keeps its aspect ratio with the existing cover crop. Explicit inset skin rectangles stay unchanged.
- **Released holds:** releasing an active long note no longer deletes its remaining body immediately. The body scrolls until its original tail passes, without restoring the head, active glow or scoring. Player and ghost use the same rule. Existing release judgement, timing windows, gauge, combo and replay input stay unchanged.
- **Mine-safe LN Mix:** newly generated holds leave at least 50 ms before and after same-lane mines. A candidate without enough room stays a tap; a later mine limits its maximum tail. Mines in other lanes do not block independent holds. Original notes, holds, keysounds and mines are preserved. Selection still uses the existing seeded musical-duration algorithm; no-mine output matches v2.

The existing `long_note_mix_version` field records **3** for new play. Saved v1/v2 replays retain their original generation, and a ghost with a different LN Mix version cannot change the new run's chart. New v3 replays require this candidate's verifier; released 1.9.9 rejects them. Replay fields, judgement ruleset IDs and peer protocol are unchanged. LN Mix v2/v3 retain the existing online/BEST exclusion. No website deployment or database change is included.

Automated geometry, HUD, mine-boundary, saved replay and native checks are recorded in the candidate's sibling `evidence` directory. These checks do not establish physical 21:9-monitor behavior, human input, long-play quality or compatibility with every external skin.

Automatic results also wait for the last released hold to leave the field, including a negative visual offset. Explicit skip, abort and game-over behavior remain available.
