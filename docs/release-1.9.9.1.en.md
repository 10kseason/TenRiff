# TenRiff 1.9.9.1

[한국어](release-1.9.9.1.ko.md) · [English](release-1.9.9.1.en.md) · [日本語](release-1.9.9.1.ja.md) · [简体中文](release-1.9.9.1.zh-CN.md)

Adds 21:9 display support, released-hold visibility and mine avoidance in LN Mix to 1.9.9. The general fixed baseline remains **1.9.6**; earlier releases, installations, profiles and charts are preserved. This document defines release scope, not completed publication. Publication and downloads follow the [1.9.9.1 release page](https://github.com/10kseason/TenRiff/releases/tag/1.9.9.1).

- **21:9 displays:** adds the 5120×2160 preset alongside 2560×1080 and 3440×1440. Full-screen backgrounds, BGA and dimming fill the side areas using the existing aspect-preserving cover crop. The authored 1920×1080 UI, notes and pointer mapping remain centered and uniformly scaled. Saved positions and explicit inset skin rectangles remain intact.
- **Released holds:** the remaining body scrolls until its original tail passes after release, for both player and ghost. The judged head, active glow and scoring do not return. Automatic results also wait for a missed final hold's tail to leave the field, including negative visual offsets. Explicit skip, abort and game-over behavior remain.
- **Mine-safe LN Mix v3:** generated holds leave at least 50ms before and after same-lane mines. Insufficient space leaves a tap; a later mine caps the tail. Other-lane mines do not block independent holds. Preserve authored holds, notes, keysounds and mines, plus existing seeded musical duration selection. No-mine charts produce the same output as v2.

New LN Mix play records **3** in the existing `long_note_mix_version` field. Historical v1/v2 replays reconstruct their original layouts; ghosts from a different version cannot change a new play's layout. **v3 replays require the 1.9.9.1 verifier; the 1.9.9 verifier rejects them.** No replay fields, judgement ruleset IDs or peer v7 changes are introduced. Release judgement, fixed timing, score, gauge, combo, grades and existing NK2 E behavior remain.

LN Mix v2/v3 retain local history/replay only and stay excluded from legacy BEST and Sites submissions. Guest/custom exclusions, the `client_submitted` boundary and existing 16K submission support remain. No website, database or skin-editor deployment or existing-installation overwrite is included.

The [2026-10-24 candidate note](ultrawide-ln-20261024.md) is development history. Candidate evidence and fresh release checks remain separate in the [release gates](release-1.9.9.1-gate.md). Automated render/input-handler checks do not establish actual ultrawide monitor behavior, physical input/IME/listening, long play, other-PC operation or all external skins.

The bundled offline skin-editor catalog synchronizes source references after the background helper change while retaining editable IDs and values. No public web skin-editor deployment is included.
