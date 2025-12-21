# TenRiff Development Roadmap (staged)

This roadmap captures the recommended high-level order for building out the game loop while avoiding scope creep. Each stage locks in direction before layering extra features.

## 0) Fix the skeleton and master clock
- Treat **AudioThread as the master clock** for all timing-sensitive work.
- **InputThread** should timestamp events from RawInput/evdev and push them into an SPSC queue.
- Normalize chart timelines into **sample positions (int64)** so the audio thread can consume deterministic timestamps.
- **Render** only consumes snapshots to draw; judgements/scores are finalized on the audio side.

## 1) Make a full song playable end-to-end
- Minimal BMS loader (essential channels only) → note scheduling → judgement → result screen.
- Keep UI simple: **Title → Song Select → Play → Result** flow only.

## 1.5) Support both BMS and osu! beatmaps
- Add an osu! beatmap (mania) loader alongside BMS, sharing the normalized event model.
- Keep the scheduling/judgement path unified so chart format differences are isolated at load time.
- Expose format selection in song select and ensure replay/result screens show the originating format.

## 2) Key remap plus 8K/10K modes
- Implement key remapping UI per the existing “리맵 UI 플로우” spec (including NKRO test).
- Once this lands, the project becomes a solid personal practice tool.

## 3) Add two random modes first
- Start with **Full Random (FR)** and **Super Random (SR)**; defer **AR** until later.

## 4) Attach a launcher
- Handle folder checks, first-run config creation, and error code cataloging.
- Completing this makes the game self-contained on a local PC.
