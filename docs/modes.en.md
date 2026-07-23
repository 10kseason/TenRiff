# Mode System (Format / Key / Gauge / Random / Mods)

This document summarizes the implemented mode system, lane-transform/random rules (Mirror / FR / SR), and note-structure mods.

## Configuration Location
- Global: the `mode` section in `config/config.json`
- Profile: the `mode` section in `profiles/<name>/config.json`

```json
"mode": {
  "format": "auto",
  "key_mode": "none",
  "gauge": "normal",
  "random": "off",
  "random_seed": 0,
  "mods": [],
  "enable_osu_charts": false,
  "ghost_battle_enabled": false,
  "autoplay_enabled": false,
  "practice_no_fail_enabled": false,
  "one_miss_fail_enabled": false,
  "song_index_profile": "safe"
}
```

## Mode Meanings
- `format`: `auto | bms | osu`
- `key_mode`: `none | auto | 4k | 5k | 6k | 7k | 8k | 9k | 10k | 16k`
- `gauge`: `normal | hard | ex_hard | easy`
- `random`: `off | mirror | fr | sr`
- `random_seed`: fixed seed for FR/SR, forced key-mode conversion, and LN Mix selection (`0` is also fixed)
- `mods`: normalized mod-token array saved by the Mod Manager
- `enable_osu_charts`: `false | true`
- `ghost_battle_enabled`: `false | true`
  - defaults to `false`
  - `true`: auto-load the selected chart's best compatible replay for ghost comparison
  - `false`: keep normal gameplay in the single-field layout
- `autoplay_enabled`: automatically handles hittable notes and marks the result as `ASSIST`
- `practice_no_fail_enabled`: prevents gauge-based early failure and keeps playing to the chart end
- `one_miss_fail_enabled`: `Sudden Death (1 MISS)`, which fails immediately on the first osu!mania OD8 object `MISS`
  - native `BAD` timing alone and empty-key `POOR` do not trigger it
  - mutually exclusive with Practice No-Fail in Mode Settings
- `song_index_profile`: `safe | fast`
  - `safe`: the default that prioritizes lowering large-library RAM high-water usage
  - `fast`: the optional choice that aims for faster reindexing in 32GB+ environments

## Lane-Transform / Random Rules
- **Mirror**: deterministically reverses the final lanes after key-mode conversion
  - 10K/16K mirror each player half independently instead of swapping the two player fields
  - Mirror itself ignores `random_seed`, but an earlier forced key-mode conversion may use it
- **FR (Full Random)**: replaces the entire lane set with a random **permutation**
- **SR (Super Random)**: random placement per note
  - choose candidate lanes so that there is no overlap on the same lane, including simultaneous timing
  - **Long notes keep the head / tail on the same lane**
  - if no candidate lane is available, keep the original lane and log a warning

## Note-Structure Mods
- **Full LN**: converts eligible taps into standard holds ending just before the next note in the same lane
- **LN Mix 10%-90%**: preserves existing holds, excludes heads overlapping an existing same-lane span, and uses `random_seed` to deterministically select the requested share of taps that can form a hold of at least 50ms while leaving 50ms before the next same-lane note
- **Full Tap**: removes every hold tail and converts all holds into taps
- These options share the `Note Structure` category, so only one is active; the same chart and seed reproduce the same LN Mix result.

## Key-Mode Handling
- `none` keeps the chart's lane count and base pattern layout as-is
- `auto` is kept as a legacy alias and currently behaves the same as `none`
- key-mode conversion runs before Mirror / FR / SR
- `4k..16k` match the key count through N2NC-based lane remapping

## Gauge Rules
- All gauges start at `100%` and fail immediately at `0%`.
- `ex_hard` is a challenge gauge with lower recovery and heavier `BAD` / `POOR` loss than Hard.
- Clear status is separated as `EX-HARD CLEAR`, `HARD CLEAR`, `CLEAR`, and `EASY CLEAR`.
- `Sudden Death (1 MISS)` is not a gauge type; it is a separate failure rule that forces the current gauge to zero and ends the run on the first osu!mania OD8 object `MISS`.

## Implementation Location
- Mode parsing: `src/gameplay/ModeSettings.*`, `src/app/ModeResolver.*`
- Mode application: `src/gameplay/ModeApplier.*`
- Mod registry and note-structure transforms: `src/app/ModeManager.*`
