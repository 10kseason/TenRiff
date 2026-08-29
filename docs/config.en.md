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
  - controls menu, result, and song-preview music only; gameplay chart BGM remains audible
- `volume` (double)
  - master volume
- `bgm_volume` (double)
- `keysound_volume` (double)

### `input`

- `backend` (string)
  - `polling | rawinput`
  - defaults to `rawinput` on the current `1.5.1` release line
  - selectable per profile under `Options -> Input Settings -> Backend` or `Options -> Profile Setup -> Input Backend`
  - runtime fallback never rewrites the saved value to `polling`
  - a confirmed RawInput startup failure, registration-target loss, or message-window exit latches Polling across menu and subsequent gameplay sessions for the current app run
  - restarting the app or explicitly changing Backend in Input Settings retries the selected backend
- `rawinput` (bool)
  - convenience boolean persisted alongside `backend`
  - when `true`, menu/gameplay prefer RawInput
  - gameplay continuously shadows bound note/control keys with polling in the same `InputThread`
- `use_qpc` (bool)
- `grab` (bool)
  - currently a Linux-preview-oriented setting
- `queue_size` (int)
- `polling_hz` (int)
  - `1000 | 2000 | 4000 | 8000`
  - sampling cadence for the Polling backend and the gameplay polling shadow
  - default is `1000` (`1ms`)
- `judgement_hz` (int)
  - `1000 | 2000 | 4000 | 8000`
  - compatibility field kept in the input config
  - the current `1.5.1` runtime no longer drives a separate audio-thread judgement sub-step loop from this value
  - default is `4000` (`0.25ms`)
- `debounce_ms` (double)
  - real Press/Release transitions are preserved; only duplicate same-state events are removed from pressed-state tracking
  - clamped to the `0..25` range
  - default value is `8ms`
### `judge`
- `pg`, `gr`, `gd`, `bd` (double, ms)
- default `pg / gr / gd` values are `20ms / 45ms / 90ms`
- default `bd` is `210ms`
- `Judge Easy` follows its existing `1.25x` scale (`bd=262.5ms`), while `Judge Hard` uses `bd=340ms`; Hard leaves PG/GR/GD and long-note tail windows at their base values
- `indirect_miss` (double, ms)
  - the indirect-miss threshold used when no input arrives at all and a note is auto-missed
  - its timing is aligned with `bd`; under `Judge Hard`, an unplayed note is recorded as a combo-breaking indirect `POOR` / OD8 `MISS` instead of BAD
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
- visual scroll stays anchored to the chart's starting BPM; later BPM changes do not correct pixels per second, while explicit `#SCROLL`, stops, and reverse motion remain active

### `gauge`
- `normal | hard | ex_hard | easy` stay fixed until the song ends or fails.
- `shift` simulates EX-Hard, Hard, Normal, and Easy independently from 100%. When the current tier reaches 0%, it selects the next surviving tier with its already accumulated value; the highest tier still alive at the end becomes the final gauge.
- EX-Hard, Hard, Normal, and Easy all start at `100%` and fail immediately at `0%`.
- `delta`
  - `ex_hard`, `hard`, `normal`, `easy`
  - each contains `PG`, `GR`, `GD`, `BD`, `PR`

### `graphics`
- `display_mode` (string)
  - `borderless | windowed | fullscreen`
  - the default is `borderless`; it is also the recommended mode for external overlays such as Discord, OBS, and Game Bar
  - `windowed` is a fixed-size window with a title bar and can be moved
  - `fullscreen` is DXGI exclusive fullscreen, where the current Discord Game Overlay is not displayed
- `resolution` (string)
  - `native | 720p | 1080p | qhd`
- `vsync` (bool)
- `refresh_hz` (int)
  - `-1` selects `Match Display`; `0` is the profile-compatible `Unlimited` selector with an actual 1500 FPS maximum
  - legacy fixed numeric limits migrate to `-1`
  - with `vsync=false`, Unlimited paces gameplay at no more than 1500 FPS while menu rendering keeps a 300 FPS cap
  - when `vsync=true`, the present refresh follows the active monitor Hz and render pacing targets `monitor_hz * 2` (`1050` clamp)
- `performance_overlay` (bool)
  - defaults to `false`; it occupies the top-right corner and can overlap a Discord Voice widget placed there
  - in-game frame pacing measures intervals between successful DXGI `Present()` completions; HUD update cadence is not used as an FPS sample
- `bga_enabled` (bool)
  - defaults to `true`; `false` disables gameplay image/video BGA plus its decoder and upscaler work
  - Song Select background previews remain visible because they are a separate feature
- `background_upscale_mode` (string)
  - `onnx | off`; legacy `lunasr` values migrate to `onnx`
  - defaults to `off`; users switch it explicitly through `BGA Upscaler` in Graphics Settings
  - enabling it requires confirmation of a high-spec warning; no automatic performance benchmark runs
- `background_upscale_model_path` (string)
  - selected from Graphics Settings > `ONNX Model`, or set by dropping an `.onnx` file on that screen; selecting only stores the path and does not enable the upscaler
  - accepts an absolute path or a path relative to the executable/current directory; public packages include no model
  - current contract: float32 or float16 NCHW `rgb_lr [1,3,540,960]` -> `rgb_residual_x2 [1,3,1080,1920]` residual x2; INT8 QDQ models with floating external boundaries are detected and supported
  - load, contract, or inference failure keeps native scaling
  - users are responsible for model rights, quality, and performance; see `tools/onnx_upscaler/README.md`
- `background_upscale_prefer_npu` (bool)
  - defaults to `false`; the default path requests a high-performance DirectX GPU
  - experimental `Low-Power DirectX` in Graphics Settings requests `DirectXMinPower`
  - the legacy WinML path neither explicitly selects nor verifies an NPU, so this option is not evidence of NPU execution
  - failure to create the low-power session falls back to the existing high-performance DirectX path

### `mode`
The chart loader and indexer are limited to BMS-family files (`.bms/.bme/.bml/.pms`). Legacy `enable_osu_charts` and `format` values are ignored when read and are not saved again.

- `key_mode` (string)
  - `none | auto | 4k | 5k | 6k | 7k | 8k | 9k | 10k | 12k | 14k | 16k`
  - `none` means using the chart's original key count as-is
- `key_conversion_algorithm` (string)
  - `krrcream | nk2 | nk3`
  - select `Krrcream`, `KeyWeaver nK2`, or `KeyWeaver NK3 ONNX` from in-game `Mode Settings > Key Converter`
  - defaults to `krrcream`; NK3 also remasters unchanged key counts and its default `AUTO` backend prefers ncnn Vulkan
  - Krrcream only remaps source notes into target lanes
  - when expanding the key count, nK2 creates safe support notes directly in the target layout during conversion instead of pre-adding notes to the source
  - NK3 always uses P64 and the host beam safety solver, adding the generalized MLP only for non-10K sources converted to 10K; select `TENRIFF_NK3_BACKEND=AUTO|VULKAN|OPENVINO`, where `AUTO` tries ncnn Vulkan first, and use `TENRIFF_NK3_VULKAN_DEVICE=<index>` when selecting among multiple Vulkan GPUs
- `key_conversion_nk2_preset` (string)
  - `native | transform | remaster`; defaults to `native`
  - selects nK2 `Native (12%)`, `Transform (35%)` or `Remaster (65%)`; the setting row is locked for Krrcream
  - `Remaster` raises the budget while keeping the source placement, and fills LN sections with holds of the same length
  - all three are caps; how much actually lands depends on the source density and the safety windows
- `gauge` (string)
  - `normal | hard | ex_hard | easy | shift`
- `random` (string)
  - `off | mirror | fr | sr`
- `random_seed` (int)
  - fixed seed for RR/SR, forced key-mode conversion, and LN Mix selection; ordinary Random creates a fresh session seed per play and records the actual value in the replay
- `mods` (string array)
  - Note Structure accepts one of `full_long_notes`, `ln_mix_10` through `ln_mix_90`, or `full_short_notes`
  - LN Mix considers only taps that can fit a base-BPM 1/8-note hold while ending at least 50ms before the next same-lane note, then assigns the selected holds 60% long 1/8-note, 20% medium 1/16-note, and 20% short alternating 1/24- and 1/32-note lengths
  - existing holds are preserved, heads overlapping an existing same-lane span are excluded, and the same `random_seed` selects the same taps
- `ghost_battle_enabled` (bool)
  - defaults to `false`
  - when `true`, TenRiff auto-loads the selected chart's best compatible replay for ghost comparison
  - when `false`, normal gameplay stays single-field
- `autoplay_enabled` (bool)
  - non-competitive automatic play mode for QA
  - when `true`, playable note input is handled automatically and the result is saved as `AUTOPLAY`
  - never awards an official clear, best score, clear lamp, or default ghost; local result/replay history is retained
- `practice_no_fail_enabled` (bool)
  - QA assist mode
  - when `true`, gauge-based early failure is disabled while judgement and result export still run to chart end
  - the result is tagged with `ASSIST`
- `one_miss_fail_enabled` (bool)
  - when `true`, the first OD8-converted object `MISS` forces the gauge to zero and ends the run immediately
  - native `BAD` timing alone and empty-key `POOR` judgements do not trigger it
  - enabling it in Mode Settings automatically disables `practice_no_fail_enabled`
- `pacemaker_mode` (string)
  - `off | accuracy | score`; defaults to `off`
  - Accuracy/Score modes run to chart end and clear only when the selected result target is met
  - enabling Pacemaker disables Practice and Sudden Death; replay playback and multiplayer force it off
- `pacemaker_target_accuracy` (double)
  - `0..100`, default `90.0`; compares against standard result Accuracy
- `pacemaker_target_score` (int)
  - `0..10000`, default `8000`; compares against the displayed final Score after multipliers
- `song_index_profile` (string)
  - `safe | fast`
  - `safe` is the default that prioritizes lower RAM high-water usage on large libraries
  - `fast` is the optional minimal profile that skips file hashes, previews, difficulty tables, and native LV/CR
- `calculate_song_index_difficulty` (bool)
  - defaults to `false`
  - `false` keeps BMS `#PLAYLEVEL` as the menu LV and skips the CPU-heavy native LV/CR calculation
  - `true` calculates Revive LV/Circus Rating during a full `safe` index; `fast` always skips it
  - changing the setting separates cache modes and triggers a full reindex of the current song source

### `ui`
- `profile_nickname` (string)
  - editable in Quick Setup and used as the display name in saved records and multiplayer
  - controls/duplicate whitespace are sanitized and UTF-8 length is limited to 48 bytes; an empty value falls back to the profile ID
- `profile_avatar_path` (string)
  - optional local PNG/JPG path selected from Profile Setup; empty uses the built-in TenRiff fallback mark
  - the path is saved per profile and UI-sanitized to at most 2048 UTF-8 bytes
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
- `song_collection_filter` (string)
  - last selected all/favorites/named-collection filter
- `song_key_filter` (int, `0..16`)
  - last song-browser key-count filter; `0` means all keys
- `song_level_min_filter`, `song_level_max_filter` (int, `0..50`)
  - last level-range bounds; `0` disables that bound
  - key, level, and collection filters are saved immediately and restored after restart
- `difficulty_table_path` (string)
  - path to a local BMS difficulty-table header JSON selected from Browse, or to the profile cache created from a link
  - the header uses `name`, `symbol`, and a local relative `data_url`; data-array entries use `md5` or `sha256` plus `level`
  - selecting or clearing the table reindexes the current source and displays table levels for matching charts; selecting one automatically switches indexing to `safe` because hashes are required
- `difficulty_table_url` (string)
  - original http(s) BMSTable HTML page or header JSON link imported from Browse
- `online_records_server_url` (string)
  - base URL for records, ranked play, and global chat on the currently selected account server
  - the TenRiff main default is `https://121.174.18.181:27303`; a local private server may use `http://127.0.0.1:27302`
  - server failures disable only the Online tab and never block local records or play
  - standard `<meta name="bmstable" content="...">` metadata is resolved and the header/data JSON is cached under the profile `difficulty_tables` directory; local JSON selection clears this field
- `tenriff_main_server_url` (string)
  - fixed main API endpoint used by the F10 `TenRiff Main` selection
  - main TenRiff API selected by the F10 account dialog; public deployments require HTTPS
- `private_server_url` (string)
  - user-entered private API URL; remote endpoints require HTTPS and only localhost may use HTTP
- `account_server_mode` (string)
  - last selected account server: `main | private`

### `skin`
- `source` (string)
  - `native | tenriff | lr2`
- `tenriff_skin_name` (string)
  - imported TenRiff `skin.json` skin folder name
- `lr2_skin_name` (string)
  - imported LR2 playskin name
- `lr2_resolution_mode` (string)
  - `auto | sd | hd | fhd`
  - LR2 playskin resolution override token
  - `auto` resolves the SD/HD/FHD family based on the LR2 playskin `#DST_NOTE` layout coordinates instead of asset file names
- `note_shape` (string)
  - `rect | triangle | pentagon | hexagon | circle`
  - at 100%, procedural circle and polygon shapes use the same full lane width as the rect bar
- `show_hold_tail` (bool)
  - shows or hides the long-note tail cap without changing hold judgement/body continuity
- `note_border_enabled` (bool)
- `black_playfield_enabled` (bool)
  - when true, fills the complete player/ghost playfield, including lane-spacing gaps, with solid black
  - defaults to `true`; an explicit `false` in an existing profile is preserved
- `judgement_line_position` (double)
  - vertical position ratio of the gameplay judgement line
  - clamped to the `0.00..1.00` range (0% to 100%)
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
  - scales the complete centered playfield, lanes/dividers, note heads/tails, and adjacent gauges together (`0.50..1.40`)
  - the default combined edge gap between adjacent notes is `24px` at 100%
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
  - native skin multiplies the default `1px` divider by this value, and LR2 skins also multiply any imported divider widths by it
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
- `single_color` (string)
  - `off` or one of the supported color tokens
  - a color token overrides every key mode and scratch lane with that color
  - the per-lane `lane_colors` remain stored, so selecting `off` restores the previous palettes
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
  - the storage key and behavior are unchanged; the UI exposes it as `Skins > Visual Latency`
- `sound` (double, ms)
  - clamped to `-500..500` and exposed in `Audio Settings > Sound Offset` and the `Calibration Wizard`
  - positive values delay chart BGM/autoplay keysounds and negative values advance them; judgement, note/BGA timing, and hit-triggered `follow` keysounds do not move

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
- In particular, BMS defaults and keysound-policy values are migration targets; legacy osu chart/skin fields are no longer saved.
- If the config file does not exist, the app starts with defaults and immediately saves the profile.
