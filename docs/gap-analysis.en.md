# TenRiff Manual Gap Analysis (v0.1)

- Reference documents: `개발메뉴얼(v0.1)/개발지시사항.txt`, `개발메뉴얼(v0.1)/Tenriff 런쳐 개발 지시사항.txt`,
  `개발메뉴얼(v0.1)/UI 개발지시사항.txt`, `개발메뉴얼(v0.1)/RAW 인풋과 멀티스레드 활용과 최적화에 대한 개발 지시사항 메뉴얼.txt`
- Analysis date: 2025-12-23
- Scope: `src/`, `docs/`, `launch_*.{bat,sh}`, `profiles/default/` (Songs folder excluded)

## Severity Criteria
- Critical: gaps that block the end-to-end play loop or violate core principles such as the audio master clock or determinism
- High: gaps where core features / UX requirements are missing and usability is significantly degraded
- Medium: important gaps that are not immediately blocking, but still need functionality, validation, or follow-up
- Low: quality or convenience improvements

## Summary (unresolved)
- Critical: 0
- High: 1
- Medium: 8
- Low: 1

## Items Already Satisfied / Partially Satisfied
- BMS parser / normalize / timeline (sample-time) foundation
- Basic 10-key channel mapping
- SpeedManager / GaugeManager baseline spec support
- BMS-family-only chart loader and indexer
- RawInput + InputThread + SPSCQueue + ClockSync scaffolding
- Input polling (1000 / 2000 / 4000 / 8000 Hz) + RenderThread separation
- WASAPI backend + AudioThread skeleton
- Launcher script bootstrap

## Detailed Gaps

### 1) No audio-master-clock based “play loop”
- Requirement: the AudioThread should be the master clock, with judgements / gauge / keysounds handled on the audio thread
- Current state: the audio callback already runs the input consumption and judgement / gauge update loop
- Gap: keysound / real-audio mixing is not implemented
- Severity: Medium
- Related files: `src/audio/AudioThread.*`, `src/input/InputThread.*`, `src/chart/BmsTimeline.*`
- Fix direction (summary): build the core loop where AudioThread consumes Timeline + InputQueue and updates judgement / keysound / gauge

### 2) Menu state machine (Title / SongSelect / Gameplay / Result) is incomplete (audio reuse)
- Requirement: menu state machine + SongIndexerThread + cache + live audio-clock retention
- Current state: menu state machine + **Windows D3D11 menu UI** + SongIndexerThread + cache are implemented
- Gap: menu-to-gameplay **audio device reuse** is incomplete
- Severity: Medium
- Related files: `src/app/MenuApp.*`, `src/render/MenuWindow.*`, `src/render/RenderThread.*`
- Fix direction (summary): connect audio-device retention / handoff during Menu -> GameSession transition

### 3) Missing input-event to judgement connection
- Requirement: InputThread -> SPSCQueue -> audio-thread event consumption / judgement
- Current state: audio callback already consumes input and updates judgement / gauge
- Gap: none (initial implementation complete)
- Severity: Resolved
- Related files: `src/input/*`, `src/audio/AudioThread.*`
- Fix direction (summary): consume the input queue and apply judgement / mask / combo rules in the audio callback

### 4) Rate / Hi-Speed application scope incomplete
- Requirement: Rate should affect schedule / judgement windows, while Hi-Speed should affect only visual scroll speed
- Current state: SpeedManager exists, but it is not yet linked to the timeline / schedule / real judgement path
- Gap: changing Rate does not scale actual playback / judgement timing
- Severity: High
- Related files: `src/game/SpeedManager.*`, `src/chart/BmsTimeline.*`
- Fix direction (summary): scale schedule sample time by rate and apply `scaleJudgeWindow` to judgement windows

### 5) Key remap + profile save / duplicate warning UI
- Requirement: save `keymap.json`, prevent duplicates, provide NKRO test UI
- Current state: **remap UI / save / duplicate handling / test screen implemented**
- Gap: none
- Severity: Resolved
- Related files: `src/app/MenuApp.*`, `src/config/Keymap.*`, `docs/menu.en.md`
- Fix direction (summary): add test coverage later

### 6) Result screen and replay saving
- Requirement: detailed result screen + Enter wait + replay saving
- Current state: **result screen / statistics display / Enter return + replay / result JSON saving implemented**
- Gap: none (initial implementation complete)
- Severity: Resolved
- Related files: `src/app/MenuApp.*`, `src/app/GameSession.*`, `src/gameplay/Replay.*`, `src/gameplay/ResultStats.*`
- Fix direction (summary): add a replay loader later for playback / verification

### 7) Chart input scope
- Requirement: BMS-family-only loading and indexing
- Current state: only `.bms/.bme/.bml/.pms` are supported; osu loading and import paths are removed
- Gap: none
- Severity: Resolved
- Related files: `src/chart/BmsParser.*`, `src/app/ChartLoader.*`, `src/app/SongIndex.*`
- Fix direction (summary): retain BMS parser, timeline, and real-chart regressions

### 8) Judgement rules (window / mask / LN handling) not implemented
- Requirement: PG / GR / GD / BD / PR windows, 30ms lane mask, LN retention / leave rules
- Current state: judgement / mask / LN rules are implemented in GameplayEngine
- Gap: none (initial implementation complete)
- Severity: Resolved
- Related files: `src/gameplay/GameplayEngine.*`
- Fix direction (summary): strengthen tests and tune judgement parameters

### 9) Config.json loading and CLI priority application not implemented
- Requirement: CLI > config.json priority, rate / HS / gauge application
- Current state: global (`config/config.json`) + profile config loading and CLI priority are applied
- Gap: none (initial implementation complete)
- Severity: Resolved
- Related files: `src/config/Config.*`, `profiles/default/config.json`, `config/config.json`
- Fix direction (summary): strengthen live refresh from the menu UI

### 10) Launcher feature expansion incomplete
- Requirement: show binary metadata, SDL2 / VC++ guidance, log tails by exit code
- Current state: only basic folder checks / default file creation are implemented
- Gap: insufficient diagnostics / guidance
- Severity: Medium
- Related files: `launch_win.bat`, `launch_linux.sh`, `개발메뉴얼(v0.1)/Tenriff 런쳐 개발 지시사항.txt`
- Fix direction (summary): expand launcher scripts + strengthen run-log summaries

### 11) Linux input / audio path not implemented
- Requirement: evdev + ALSA (or alternative) support
- Current state: only Windows RawInput / WASAPI exist
- Gap: no Linux execution path
- Severity: Medium
- Related files: `src/input/*`, `src/audio/*`
- Fix direction (summary): add evdev input thread and ALSA backend

### 12) Performance / latency metrics HUD and diagnostic logging not implemented
- Requirement: latency histogram, xruns, queue depth, etc. should be shown / logged
- Current state: only documents exist
- Gap: no instrumentation / diagnostic tools
- Severity: Medium
- Related files: `docs/latency.en.md`
- Fix direction (summary): collect audio-callback / input-queue metrics and expose them in the HUD

### 13) Unit test coverage is insufficient
- Requirement: key remap, judgement / shift cooldown, Rate window scaling, etc. tests
- Current state: only parser / normalize / Speed / Gauge partial tests exist
- Gap: missing tests for key new features
- Severity: Medium
- Related files: `tests/unit/*`
- Fix direction (summary): add unit tests for the new features

### 14) Partial documentation / code synchronization gap
- Requirement: reflect the latest progress in README / docs
- Current state: README is only a summary, and there is no detailed gap analysis
- Gap: gaps and priorities are not clearly documented
- Severity: Low
- Related files: `README.en.md`, `docs/*`
- Fix direction (summary): refresh the README summary + keep the gap-analysis document up to date

### 15) Keymap file format extension (layout / metadata) not reflected
- Requirement: `keymap.json` should explicitly include layout / bindings
- Current state: the layout field is included
- Gap: none (initial implementation complete)
- Severity: Resolved
- Related files: `profiles/default/keymap.json`, `src/config/Keymap.*`
- Fix direction (summary): add key remap UI and NKRO test

## Decisions Needed
- Which UI framework to use (for example SDL + ImGui, or a custom renderer)
- Which Linux audio backend to prioritize (ALSA vs JACK)
- The default rendering resolution and scaling policy for menu / gameplay
- ✅ Replay format (sample-time-based JSON, `profiles/<profile>/replays/*.json` + `profiles/<profile>/results/*.json`)
