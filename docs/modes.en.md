# Mode System (Format / Key / Gauge / Random)

This document summarizes the currently implemented mode system and random rules (SR / FR).

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
  "song_index_profile": "safe"
}
```

## Mode Meanings
- `format`: `auto | bms | osu`
- `key_mode`: `none | auto | 4k | 5k | 6k | 7k | 8k | 9k | 10k | 16k`
- `gauge`: `normal | hard | easy`
- `random`: `off | fr | sr`
- `random_seed`: random fixed seed (`0` is also treated as a fixed value)
- `song_index_profile`: `safe | fast`
  - `safe`: the default that prioritizes lowering large-library RAM high-water usage
  - `fast`: the optional choice that aims for faster reindexing in 32GB+ environments

## Random Rules
- **FR (Full Random)**: replaces the entire lane set with a random **permutation**
- **SR (Super Random)**: random placement per note
  - choose candidate lanes so that there is no overlap on the same lane, including simultaneous timing
  - **Long notes keep the head / tail on the same lane**
  - if no candidate lane is available, keep the original lane and log a warning

## Key-Mode Handling
- `none` keeps the chart's lane count and base pattern layout as-is
- `auto` is kept as a legacy alias and currently behaves the same as `none`
- `4k..16k` match the key count through N2NC-based lane remapping

## Implementation Location
- Mode parsing: `src/gameplay/ModeSettings.*`, `src/app/ModeResolver.*`
- Mode application: `src/gameplay/ModeApplier.*`
