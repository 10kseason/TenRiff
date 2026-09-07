# TenRiff Config Schema (current)

Reference: [Config.h](../src/config/Config.h), [Config.cpp](../src/config/Config.cpp), [default JSON](../config/config.json), and [keymaps](../src/config/Keymap.cpp). Missing values use code defaults; bounds below describe load/save normalization.

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
- `normalize_audio` (bool)
  - Stereo-linked RMS leveling of the gameplay mix before limiter/master volume; false by default. Menu music and song previews are unaffected.
- `keysound_volume` (double)

### `input`

- `backend` (string)
  - `polling | rawinput`
  - defaults to `rawinput` on the current `1.7.1` release line
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
  - the current `1.7.1` runtime no longer drives a separate audio-thread judgement sub-step loop from this value
  - default is `4000` (`0.25ms`)
- `debounce_ms` (double)
  - real Press/Release transitions are preserved; only duplicate same-state events are removed from pressed-state tracking
  - clamped to the `0..25` range
  - default value is `8ms`
### `judge`
- `pg`, `gr`, `gd`, `bd` (double, ms)
- default `pg / gr / gd` values are `20ms / 65ms / 115ms`
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

Gauge Shift is always active. `mode.gauge` selects the starting tier: `ex_hard / hard / normal / easy`, from EX down to Easy. The selected tier and every lower tier are simulated independently from 100%; a failed tier yields to the next surviving tier. Gauge failure occurs only when all eligible tiers fail. Legacy `shift` means an EX start.

- `delta`: `ex_hard`, `hard`, `normal`, `easy` → `PG`, `GR`, `GD`, `BD`, `PR`.

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
  - NK3 always uses P64 and the host beam safety solver, adding the generalized MLP only for non-10K sources converted to 10K; select `TENRIFF_NK3_BACKEND=AUTO|VULKAN|NCNN_CPU|OPENVINO`, where `AUTO` tries ncnn Vulkan first, and use `TENRIFF_NK3_VULKAN_DEVICE=<index>` when selecting among multiple Vulkan GPUs
- `key_conversion_nk2_preset` (string)
  - `native | transform | remaster`; defaults to `native`
  - selects nK2 `Native (12%)`, `Transform (35%)` or `Remaster (65%)`; the setting row is locked for Krrcream
  - `Remaster` raises the budget while keeping the source placement, and fills LN sections with holds of the same length
  - all three are caps; how much actually lands depends on the source density and the safety windows
- `gauge` (string)
  - `normal | hard | ex_hard | easy | shift`
- `random` (string)
  - `off | mirror | rr | frns | sr` (`fr` = `frns`)
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

| Field | Type, range, default | Behavior |
| --- | --- | --- |
| `profile_nickname` | string; UTF-8 ≤48 bytes | Display name; empty uses profile ID. Whitespace/control characters are normalized. |
| `profile_avatar_path` | string; UTF-8 ≤2048 bytes | Local PNG/JPG avatar path. |
| `language` | `en`, `ko`; `en` | UI language; invalid values normalize to en. |
| `result_tail_ms` | double; `500` ms | Extra result-transition delay after judgement completion; the chart audio end is also considered. |
| `require_enter_to_exit` | bool; `true` | Retained for read/write compatibility; the current Windows result-input path does not use it to auto-exit. |
| `show_cursor_in_gameplay` | bool; `true` | Show the mouse pointer during gameplay. |
| `active_song_source`, `recent_song_sources` | string / string[] | Current and recent song folders. |
| `session_mix_lr2_course_path` | string | Selected LR2 course file path. |
| `favorite_chart_keys` | string[] | Internal chart keys for favorites. |
| `collections` | object: name → string[] | Chart keys grouped by collection name. |
| `song_collection_filter` | string; `all` | All/favorites/collection filter; saved immediately. |
| `song_key_filter` | int: `0..16`; `0` | Key-count filter; zero means all. UI choices are 4K–10K, 12K, 14K and 16K. |
| `song_level_min_filter`, `song_level_max_filter` | int: `0..50`; `0` | Level bounds; zero disables the corresponding bound. |
| `difficulty_table_path` | string | Local header JSON or downloaded profile-cache header. Changes reindex; selecting a table switches indexing to safe. |
| `difficulty_table_url` | string | Original HTTP(S) BMSTable page/header URL. Resolves bmstable metadata and caches header/data; local JSON selection clears it. |
| `online_records_server_url` | string | Records/ranking/chat API URL; failures do not block local play or records. |
| `tenriff_main_server_url` | string | F10 main-server API URL; default is `kTenRiffMainApiUrl` in Config.h. |
| `private_server_url` | string | F10 private API URL; remote HTTPS, HTTP allowed only for localhost. |
| `account_server_mode` | `main`, `private`; `main` | Last account-server selection. |

Difficulty-table headers use `name`, `symbol` and a local relative `data_url`; data entries match `md5` or `sha256` plus `level`. Remote imports are stored in the profile's `difficulty_tables` cache.

### `skin`

These are profile `config.json` skin settings. For a skin package's `skin.json` contract, see [Skin format](skin-format.md).

| Field | Type, range, default | Behavior |
| --- | --- | --- |
| `source` | string: `native`, `tenriff`, `lr2` | Skin source. |
| `tenriff_skin_name`, `lr2_skin_name` | string | Imported skin folder names. |
| `scratch_position` | `left`, `right`; `left` | Changes only 7+1 scratch display order; input/judgement lanes stay unchanged. |
| `lr2_resolution_mode` | `auto`, `sd`, `hd`, `fhd`; `auto` | LR2 coordinate resolution; auto uses `#DST_NOTE` coordinates, not filenames. |
| `visual_preset` | `classic`, `neon`, `minimal`, `tenriff`; `tenriff` | Selecting a preset in the menu resets its visual option bundle. |
| `note_shape` | `rect`, `triangle`, `pentagon`, `hexagon`, `circle`; `rect` | Procedural note shape. |
| `note_image_aspect` | `stretch`, `contain`, `width`; `stretch` | Fill the rectangle / fit inside with aspect preserved / keep width and derive height from aspect. |
| `preserve_note_image_aspect_ratio` | bool; `false` | Legacy field; explicit `note_image_aspect` wins. Saved true for non-stretch modes. |
| `note_border_enabled`, `show_lane_dividers`, `show_judgement_line` | bool; `true` | Show note borders, lane dividers, and the judgement line respectively. |
| `note_divider_gap_px` | double: `0..40`; `12` px | Gap from each note edge to the divider; zero expands notes to the dividers. |
| `show_gear_boundary_line` | bool; `false` | Show the gear boundary line. |
| `show_timing_feedback` | bool; `true` | Show FAST/SLOW text and timing history; judgement grades remain independent. |
| `show_hold_tail`, `hold_tail_taper_enabled` | bool; `false` | LN tail-cap visibility and taper respectively; no judgement-rule change. |
| `judgement_line_glow_enabled` | bool; `true` | Judgement-line glow. |
| `key_pulse_brightness` | double: `0..1`; `1` | Hit Burst brightness; zero disables it. |
| `key_pulse_enabled` | bool; `true` | Legacy ON/OFF mirror; false or zero brightness disables the burst. |
| `hit_burst_style` | `prism`, `ring`, `spark`; `prism` | Built-in Hit Burst material. |
| `ui_font` | `default`, `malgun`, `bahnschrift`, `consolas`; `default` | Menu font; default is Segoe UI. Logo, rank and combo keep their dedicated fonts. |
| `key_label_position` | `bottom`, `top`, `off`; `bottom` | Lane key-label position. |
| `judgement_line_position` | double: `0..1`; `0.82` | Vertical judgement-line position ratio. |
| `gameplay_field_offset_x` | double: `-720..720`; `0` | Field X offset in 1920×1080 base pixels; additionally constrained to keep the field and ↔ handle visible. |
| `combo_position`, `judgement_position` | double: `0.10..0.78`; `0.24` | Independent combo/judgement Y anchors; missing judgement position inherits `combo_position` in older profiles. |
| `combo_offset_x`, `judgement_offset_x` | double: `-600..600`; `0` | Independent combo/judgement X offsets in 1920×1080 base pixels. |
| `lane_background_opacity` | double: `0..0.45`; `0.18` | Lane-background opacity. |
| `black_playfield_enabled` | bool; `true` | Black field including lane gaps. |
| `visual_opacity` | double: `0.20..1`; `0.96` | Shared opacity multiplier for notes, receptors and key labels. |
| `note_outline_opacity` | double: `0..1`; `0.78` | Note-outline opacity. |
| `hold_body_opacity` | double: `0.05..1`; `1` | LN-body opacity. |
| `lane_width_scales` | object: mode → number[]; `0.50..1.75` | Per-lane widths; array length equals lane count. |
| `note_width_scale` | double: `0.50..1.40`; `1` | Note & Field Size: scales field, lanes, notes and adjacent gauge around the center. |
| `lane_spacing_scales` | object: mode → number[]; `0..2` | Lane-gap array with lane_count - 1 entries. |
| `note_height_scale` | double: `0.50..4`; `1.8` | Note head/tail height scale. |
| `lane_divider_width_scale` | double: `0..2`; `1` | Shared divider-width scale for all modes, including imported LR2 dividers. |
| `lane_center_gap_scale` | double: `0..2`; `0` | Center gap between the two 16K blocks. |
| `hold_body_width_scale` | double: `0.50..1.20`; `1` | LN-body width scale. |
| `note_width_scales`, `note_height_scales`, `lane_center_gap_scales` | object: mode → number | Per-mode overrides of the corresponding scalar, with the same bounds. |
| `lane_divider_width_scales` | object: mode → number | Legacy field; runtime uses the shared `lane_divider_width_scale`. |
| `lane_colors` | object: mode → string[] | Color-token array with one entry per lane. |
| `single_color` | string; `off` | A color token overrides all lanes while preserving `lane_colors`. |

Per-mode arrays/overrides support `4k`, `5k`, `6k`, `7k`, `7+1`, `8k`, `9k`, `10k`, `12k`, `14k`, `16k`. Color tokens: `ice`, `azure`, `gold`, `mint`, `rose`, `violet`, `orange`, `teal`. `7+1` is a skin palette, not a separate keymap mode. Legacy `expand_notes_to_dividers=true` seeds a zero gap; explicit `note_divider_gap_px` takes precedence.

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
  - `4k`, `5k`, `6k`, `7k`, `8k`, `9k`, `10k`, `12k`, `14k`, `16k`
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
