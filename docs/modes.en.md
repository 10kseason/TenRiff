# Mode System (Key / Gauge / Random / Mods)

This document summarizes the implemented mode system, lane-transform/random rules (Mirror / FR / SR), and note-structure mods.

## Configuration Location
- Global: the `mode` section in `config/config.json`
- Profile: the `mode` section in `profiles/<name>/config.json`

```json
"mode": {
  "key_mode": "none",
  "key_conversion_algorithm": "krrcream",
  "key_conversion_note_add_mode": "default",
  "enable_osu_charts": false,
  "gauge": "normal",
  "random": "off",
  "random_seed": 0,
  "mods": [],
  "ghost_battle_enabled": false,
  "autoplay_enabled": false,
  "practice_no_fail_enabled": false,
  "one_miss_fail_enabled": false,
  "song_index_profile": "safe",
  "calculate_song_index_difficulty": false
}
```

## Mode Meanings
- `enable_osu_charts`: defaults to `false`; Mode Settings > `OSU Charts` enables indexing and play for 4K-10K osu!mania `.osu` and refreshes the library
- `key_mode`: `none | auto | 4k | 5k | 6k | 7k | 8k | 9k | 10k | 12k | 14k | 16k`
- `key_conversion_algorithm`: `krrcream | nk2` (defaults to `krrcream`; selected in the in-game Key Converter row and used only for actual key-count conversion)
- `key_conversion_note_add_mode`: `default | add_25_plus` (defaults to `default`; only an actual playable-key-count change first requests at least 25% more source-pattern notes and then passes them through the key converter)
- `gauge`: `normal | hard | ex_hard | easy | shift`
- `random`: `off | mirror | rr | fr | sr`
- `random_seed`: fixed seed for RR/FR/SR, forced key-mode conversion, Note Add, and LN Mix selection (`0` is also fixed)
- `mods`: normalized mod-token array saved by the Mod Manager
- `ghost_battle_enabled`: `false | true`
  - defaults to `false`
  - `true`: auto-load the selected chart's best compatible replay for ghost comparison
  - `false`: keep normal gameplay in the single-field layout
- `autoplay_enabled`: automatically handles hittable notes and marks the result as `ASSIST`
- `practice_no_fail_enabled`: prevents gauge-based early failure and keeps playing to the chart end
- `one_miss_fail_enabled`: `Sudden Death (1 MISS)`, which fails immediately on the first OD8-converted object `MISS`
  - native `BAD` timing alone and empty-key `POOR` do not trigger it
  - mutually exclusive with Practice No-Fail in Mode Settings
- `song_index_profile`: `safe | fast`
  - `safe`: the default that prioritizes lowering large-library RAM high-water usage
  - `fast`: the optional choice that aims for faster reindexing in 32GB+ environments
- `calculate_song_index_difficulty`: `false | true`
  - default `false`: keep BMS `#PLAYLEVEL` and skip native LV/CR calculation
  - `true`: calculate Revive LV/Circus Rating during a full reindex

`Rate` is stored under `speed.rate`, not `mode`. It can be changed in Mode Settings or directly from Song Select with `-` / `+` while search text entry is inactive.

## Lane-Transform / Random Rules
- **DP Flip**: swaps the complete left/right player fields while preserving lane order inside each field
- **Mirror**: deterministically reverses the final lanes after key-mode conversion
  - DP layouts mirror each player field independently instead of swapping the two fields
  - Mirror itself ignores `random_seed`, but an earlier forced key-mode conversion may use it
- **RR (R-Random)**: keeps scratches fixed and rotates each playable lane group by a seeded offset; DP halves rotate independently
- **FR (Full Random)**: replaces the entire lane set with a random **permutation**
- **SR (Super Random)**: random placement per note
  - choose candidate lanes so that there is no overlap on the same lane, including simultaneous timing
  - **Long notes keep the head / tail on the same lane**
  - if no candidate lane is available, keep the original lane and log a warning

## Note-Structure Mods
- **Note Add 10%-100%**: deterministically adds silent chord notes at existing note times while avoiding scratches, hold bodies, same-lane duplicates, and excessive chord size. Runs remain in Records but cannot replace a normal best record.
- **Full LN**: converts eligible taps into standard holds ending just before the next note in the same lane
- **LN Mix 10%-90%**: preserves existing holds and excludes heads overlapping an existing same-lane span. It uses `random_seed` to select the requested share of taps that can fit a base-BPM 1/8-note hold while ending at least 50ms before the next same-lane note, then deterministically assigns every Mix level 60% long 1/8-note, 20% medium 1/16-note, and 20% short alternating 1/24- and 1/32-note lengths
- **Full Tap**: removes every hold tail and converts all holds into taps
- These options share the `Note Structure` category, so only one is active; the same chart and seed reproduce the same LN Mix result.

## Key-Mode Handling
- `none` keeps the chart's lane count and base pattern layout as-is
- `auto` is kept as a legacy alias and currently behaves the same as `none`
- `4k..10k`, `12k`, `14k`, and `16k` match the key count through N2NC-based lane remapping
- forced conversion of `5+1 SP` and `7+1 SP` remaps only the keyboard part; followed scratch keysounds move to autoplay
- forced conversion of `10+2 DP` and `14+2 DP` likewise excludes both scratches and converts the two keyboard halves independently
- `add_25_plus` first requests at least 25% safe silent chords at existing source-pattern times, then the key converter produces the final layout from all notes; a higher Note Add mod replaces that percentage instead of stacking another pass. The run remains in Records but cannot replace a normal best record.
- application order: key-mode conversion → DP Flip → Mirror/RR/FR/SR → Note Add → LN/Full Tap structure transform

## Gauge Rules
- Fixed gauges (`ex_hard / hard / normal / easy`) start at `100%`, fail immediately at `0%`, and never change type.
- `shift` independently simulates EX-Hard / Hard / Normal / Easy from 100%, selects the next tier that survived the same judgement history when the current tier dies, and finishes on the highest surviving tier.
- `ex_hard` is a challenge gauge with lower recovery and heavier `BAD` / `POOR` loss than Hard.
- Clear status distinguishes fixed-gauge results and the final Shift tier as `GAUGE SHIFT EX-HARD / HARD / NORMAL / EASY CLEAR`.
- `Sudden Death (1 MISS)` is not a gauge type; it is a separate failure rule that forces the current gauge to zero and ends the run on the first OD8-converted object `MISS`.

## Implementation Location
- Mode parsing: `src/gameplay/ModeSettings.*`, `src/app/ModeResolver.*`
- Mode application: `src/gameplay/ModeApplier.*`
- Mod registry and note-structure transforms: `src/app/ModeManager.*`
