# TenRiff Docs Map

Language: [한국어](README.md) | [English](README.en.md) | [简体中文](README.zh-CN.md) | [日本語](README.ja.md)

If you have already read the root [`README.en.md`](../README.en.md), this document is the next-step index into the detailed documentation. It includes both design documents and the current-state document, so when you need to gather context quickly, the following order is the most efficient.

This codebase should also be read as a `vibe coding` work that grew through rapid iteration and experimentation.

## Recommended Reading Order
1. `docs/current-state.en.md`
   - Current product state, core subsystems, validated commands, and remaining manual verification items
2. `docs/baseline-1.1.2.en.md`
   - The `1.1.2 final stable` baseline document that defines what the current work should be stacked on top of
3. `docs/gameplay-guide.en.md`
   - How to start playing, choose songs, handle controls, and understand the HUD, judgements, and result screen from a practical player perspective
4. `docs/config.en.md`
   - Actual config / profile / keymap structure
5. `docs/localization.en.md`
   - Current English/Korean UI structure and the file boundaries to touch when adding more languages
6. `docs/menu.en.md`
   - Menu / state machine / song-select flow
7. `docs/core-loop.en.md`
   - Play loop and data flow
8. `docs/roadmap.en.md`
   - Long-term direction for future work
9. `docs/developer-extension-guide.en.md`
   - Developer guide explaining where to extend modes/mods, runtime migration, replay/result handling, and maintenance workflows

## Which Docs Are Source Of Truth
- `docs/current-state.en.md`
  - Summary of the current implementation state
- `docs/baseline-1.1.2.en.md`
  - The `1.1.2 final stable` baseline document that follow-up work should preserve
- `docs/config.en.md`
  - Based on the actual `config/config.json`, `profiles/<name>/config.json`, and `keymap.json`

## Historical / Design Docs
- `docs/menu.en.md`
  - Menu / state machine / low-latency direction design document
- `docs/core-loop.en.md`
  - Early design and data-flow explanation for the play loop
- `docs/localization.en.md`
  - Reference for the current UI localization structure and future language expansion
- `docs/latency.en.md`, `docs/modes.en.md`, `docs/gap-analysis.en.md`, `docs/roadmap.en.md`
  - Feature-specific design, analysis, and long-term direction documents
- `docs/developer-extension-guide.en.md`
  - Maintenance guide for mode/mod extension work in the current codebase

## Translation Coverage
- The root `README.md` also has [`README.en.md`](../README.en.md), [`README.zh-CN.md`](../README.zh-CN.md), and [`README.ja.md`](../README.ja.md).
- Major `docs/` documents are translated side-by-side as `.en.md`, `.zh-CN.md`, and `.ja.md` files.
- If a translation and the Korean original disagree, use the current code first, then `docs/current-state.md`, then `docs/config.md`.

## Acknowledgements
- Thanks to OpenAI Codex, ChatGPT, Claude Code, Gemini, and the guest testers who helped validate the project.

## Practical Rule
- When checking current behavior, start with `docs/current-state.en.md`.
- When deciding what baseline to build on, read `docs/baseline-1.1.2.en.md` together with it.
- Older design documents and current code can differ, so if there is a conflict, interpret things in the order of current code, then `docs/current-state.en.md`, then `docs/config.en.md`.
