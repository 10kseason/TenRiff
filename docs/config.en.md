# TenRiff Config Schema (current)

This document summarizes the configuration structure that is actually active today, based on `config/config.json`, `profiles/<name>/config.json`, and `profiles/<name>/keymap.json`.

## Load Order
1. code defaults
2. global config: `config/config.json`
3. profile config: `profiles/<name>/config.json`
4. CLI
5. menu/runtime save

If a profile does not exist, it is created automatically on first launch.

## `config.json`

### `audio`
- `rate` (int)
  - default sample rate
- `frames` (int)
  - buffer frames
- `periods` (int)
  - number of periods
- `exclusive` (bool)
  - whether to try WASAPI exclusive mode
- `use_mmcss` (bool)
- `affinity` (int)
  - `-1` means default
- `preset` (string)
  - `basic | high`
- `bms_keysound_policy` (string)
  - `follow | autoplay | ignore`
- `background_sound_enabled` (bool)
  - turns menu BGM and chart background audio on or off
- `volume` (double)
  - master volume
- `bgm_volume` (double)
- `keysound_volume` (double)

### `input`
- `backend` (string)
  - `polling | rawinput`
  - default is `rawinput` on the current `1.1.3` release line
  - the stored value is preserved and no longer force-normalized to `polling` on save
  - in the `1.1.3` preview, gameplay keeps RawInput and a bound-key polling shadow together inside the same `InputThread` when `rawinput=true`
  - initialization/startup failure or an unexpected RawInput message-pump exit switches the same producer thread to Polling without resetting its queue or pressed state
- `rawinput` (bool)
  - convenience boolean persisted alongside `backend`
  - on the current `1.1.3` release line it is saved and restored as-is
  - when `true`, menu/gameplay prefer RawInput, and gameplay continuously shadows note/control keys with polling to survive runtime delivery loss
- `use_qpc` (bool)
- `grab` (bool)
  - currently a Linux-preview-oriented setting
- `queue_size` (int)
- `polling_hz` (int)
  - `1000 | 2000 | 4000 | 8000`
  - how often the polling backend samples keyboard state
  - default is `1000` (`1ms`)
- `judgement_hz` (int)
  - `1000 | 2000 | 4000 | 8000`
  - compatibility field kept in the input config
  - the current `1.1.3` runtime no longer drives a separate audio-thread judgement sub-step loop from this value
  - default is `4000` (`0.25ms`)
- `debounce_ms` (double)
  - input state-tracking setting. The current runtime does not drop real Press/Release transitions through debounce; it only removes duplicate same-state events from pressed-state tracking
  - clamped to the `0..25` range
  - default value is `8ms`

### `judge`
- `pg`, `gr`, `gd`, `bd` (double, ms)
- default `gd` is `75ms`
- default `bd` is `340ms`
- `indirect_miss` (double, ms)
  - the indirect-miss threshold used when no input arrives at all and a note is auto-missed
  - in the current runtime this is always folded into the same value as `bd`, regardless of what is stored
- `hold_grace` (double, ms)
  - the dedicated window used to treat long-note tail release as `PG`
  - default value is `80ms`
- `hold_break` (double, ms)
  - the final window that still allows long-note tail release to be judged up to `GR`
  - outside this range it becomes `BD`
  - internally always kept at or above `hold_grace`
  - default value is `200ms`
- `mask` (double, ms)

### `speed`
- `rate` (double)
- `hispeed` (double)
- `target_scroll_bps` (double)

### `gauge`
- There is no automatic gauge shift. The selected gauge type stays fixed until the song ends or fails.
- EX-Hard, Hard, Normal, and Easy all start at `100%` and fail immediately at `0%`.
- `delta`
  - `ex_hard`, `hard`, `normal`, `easy`
  - each contains `PG`, `GR`, `GD`, `BD`, `PR`

### `graphics`
- `display_mode` (string)
  - `borderless | windowed | fullscreen`
  - `windowed` is a fixed-size window with a title bar and can be moved
- `resolution` (string)
  - `native | 720p | 1080p | qhd`
- `vsync` (bool)
- `refresh_hz` (int)
  - clamped to the `60..1050` range
  - default value is `300`
  - serves as a direct FPS cap only when `vsync=false`
  - when `vsync=false`, menu rendering uses an effective cap of `300`, while gameplay render pacing is safety-clamped to `min(configured target, max(300, monitor_hz * 2))`
  - when `vsync=true`, the present refresh follows the active monitor Hz and render pacing targets `monitor_hz * 2` (`1050` clamp)
- `performance_overlay` (bool)

### `mode`
- `format` (string)
  - used together with chart filtering by default
  - `bms | osu | auto`
  - `auto` effectively means `All`
- `key_mode` (string)
  - `none | auto | 4k | 5k | 6k | 7k | 8k | 9k | 10k | 16k`
  - `none` means using the chart's original key count as-is
- `gauge` (string)
  - `normal | hard | ex_hard | easy`
- `random` (string)
  - `off | fr | sr`
- `random_seed` (int)
- `enable_osu_charts` (bool)
- `ghost_battle_enabled` (bool)
  - when `true`, TenRiff auto-loads the selected chart's best compatible replay for ghost comparison
  - when `false`, normal gameplay stays single-field
- `autoplay_enabled` (bool)
  - QA assist mode
  - when `true`, playable note input is handled automatically and the result is tagged with `ASSIST`
  - intended to stay out of the default ghost / replay comparison flow
- `practice_no_fail_enabled` (bool)
  - QA assist mode
  - when `true`, gauge-based early failure is disabled while judgement and result export still run to chart end
  - the result is tagged with `ASSIST`
- `song_index_profile` (string)
  - `safe | fast`
  - `safe` is the default that prioritizes lower RAM high-water usage on large libraries
  - `fast` is the optional choice that aims for faster rescans with a higher worker/batch budget in 32GB+ environments

### `ui`
- `language` (string)
  - `en | ko`
  - invalid values are normalized to `en` on load
  - wired to the Language row in Graphics Settings
- `result_tail_ms` (double)
- `require_enter_to_exit` (bool)
- `active_song_source` (string)
  - the last song root that was opened
- `recent_song_sources` (array of string)
  - recent external/internal song source list

### `skin`
- `source` (string)
  - `native | osu | lr2`
- `osu_skin_name` (string)
  - imported osu!mania skin name
- `lr2_skin_name` (string)
  - imported LR2 playskin name
- `lr2_resolution_mode` (string)
  - `auto | sd | hd | fhd`
  - LR2 playskin resolution override token
  - `auto` resolves the SD/HD/FHD family based on the LR2 playskin `#DST_NOTE` layout coordinates instead of asset file names
- `note_shape` (string)
  - `rect | circle`
- `note_border_enabled` (bool)
- `judgement_line_position` (double)
  - vertical position ratio of the gameplay judgement line
  - clamped to the `0.55..0.86` range
  - default value is `0.82`
- `combo_position` (double)
  - vertical position ratio of the combo display inside the gameplay field
  - clamped to the `0.10..0.78` range
  - default value is `0.24`
- `lane_width_scales` (object)
  - per-key-mode arrays for individual lane-width scales
  - each mode value is a number array with one entry per lane
  - each value is clamped to the `0.50..1.75` range
- `note_width_scale` (double)
  - width scale for note heads / tails
  - clamped to the `0.50..1.40` range
- `lane_spacing_scales` (object)
  - per-key-mode arrays for blank spacing between lanes
  - each mode value is a number array with `(lane_count - 1)` entries
  - each value is clamped to the `0.00..2.00` range
- `note_height_scale` (double)
  - height scale for note heads / tails
  - clamped to the `0.50..4.00` range
- `lane_divider_width_scale` (double)
  - shared scale for the white lane separator lines
  - clamped to the `0.00..2.00` range
  - applied uniformly across all key modes
  - native skin multiplies the default `1px` divider by this value, and osu/lr2 skins also multiply any imported divider widths by it
- `lane_center_gap_scale` (double)
  - center-gap scale between the left and right halves of the 16K field
  - clamped to the `0.00..2.00` range
  - currently applied only to the `16k` layout
- `hold_body_width_scale` (double)
  - width scale for long-note bodies
  - clamped to the `0.50..1.20` range
  - actual render calculation uses `max(4.0f, note_width * 0.5f * scale)`
- `note_width_scales` (object)
  - per-key-mode `note_width_scale` overrides
- `note_height_scales` (object)
  - per-key-mode `note_height_scale` overrides
- `lane_divider_width_scales` (object)
  - legacy compatibility field
  - the current runtime uses only the shared `lane_divider_width_scale`
- `lane_center_gap_scales` (object)
  - per-key-mode `lane_center_gap_scale` overrides
- `lane_colors` (object)
  - per-key-mode lane color palettes
  - current default / persisted modes are `4k..10k` and `16k`
  - each mode value is an array of strings with one entry per lane
  - supported tokens:
    `ice`, `azure`, `gold`, `mint`, `rose`, `violet`, `orange`, `teal`

### `offsets`
- `input` (double)
- `visual` (double)
  - clamped to the `-500..500` range

## `keymap.json`

### Shape
- `layout` (string)
- `bindings`
  - legacy 10K compatibility
- `modes`
  - `4k`, `5k`, `6k`, `7k`, `8k`, `9k`, `10k`
  - lane id -> key token under each mode

### Notes
- Old single-layout keymaps are migrated into the 10K map at runtime.
- Runtime selects the relevant mode binding based on the final chart lane count.
- Key rebinding is saved immediately to `keymap.json` after a successful capture; there is no separate final save step.
- When opening keymap editing from Song Select, the editor should default to the selected chart's lane count first, then fall back to `mode.key_mode`, then `10k`.

## Runtime Migration Notes
- Stale profiles are automatically corrected for some values.
- In particular, BMS-first defaults, osu key-mode mismatch, and keysound policy values are migration targets.
- If the config file does not exist, the app starts with defaults and immediately saves the profile.
