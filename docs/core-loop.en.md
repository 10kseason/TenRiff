# Core Play Loop (Initial Implementation)

This document summarizes the structure and data flow of the currently implemented **core play loop**.

## Core Flow
1. **InputThread** collects RawInput or polling input and passes it to `SPSCQueue`
2. In the **AudioThread** callback, `ClockSync` converts input timestamps into sample time
3. **GameplayEngine** consumes input / timeline data to update judgements, gauge, and statistics
4. The audio buffer is currently handled as **silence (fill 0)** (keysound / music mixing comes later)

## Main Components
- `config/Config.*`
  - Loads `config.json` through the **SimpleJson** parser
  - Applies the `audio/input/judge/speed/gauge/ui/offsets` sections
- `config/Keymap.*`
  - Loads `keymap.json` and builds the default keymap
  - Converts key strings to keycodes via `KeycodeMap`
- `gameplay/GameplayChart.*`
  - Converts BMS / OSU timelines into **sample-time-based note events**
  - When `rate` is applied, scales the schedule with `t' = t / rate`
- `gameplay/GameplayEngine.*`
  - Applies judgement windows (`PG / GR / GD / BD`) and the 30ms mask
  - Applies lane masks on POOR events
  - **Hold rule**: early release is BAD
  - **Hold tail rule**: only osu!mania holds and BMS `#LNMODE 2` charge notes evaluate release timing against the normal judgement window (head/tail 50:50)
  - Normal BMS long notes are auto-processed at the end if held through to the end, and do not use tail-release timing judgement
  - Collects result statistics (combo, judgement counts, average / standard deviation)
- `app/GameSession.*`
  - CLI options -> config application -> chart loading -> input / audio thread startup
  - Input queue consumption + judgement updates in the audio callback

## Initial Judgement Policy To Note
- **Hold tail judgement** applies only to osu!mania holds and BMS `#LNMODE 2` charge notes; early release is treated as BAD

## Planned Future Connections
- Menu state machine (Title / SongSelect / Gameplay / Result)
- SongIndexerThread + cache
- Key remap UI + NKRO test screen
- Result screen + replay / result JSON saving
- Launcher expansion and log / environment diagnostics
