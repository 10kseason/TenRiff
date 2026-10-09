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

- `play_to_end` (bool; default `true`): Listen to remaining chart audio after the final judgement. When false, finish normally after `ui.result_tail_ms` without waiting for the outro. Judgements, score, replay and manual post-note skip are unchanged.

- `backend` (string)
  - `wasapi | asio`; default `wasapi`. Selects gameplay and song-preview output; menu/result background music keeps its separate Windows MCI path.
- `asio_driver` (string)
  - CLSID of an installed 64-bit ASIO driver. Empty means Auto, the first driver sorted by name. Select in Audio settings; F5 refreshes the list.
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
- `title_music` (string)
  - `none | default | random_bms | last_played`; default `default`. Selects no music, the bundled theme, a random loaded BMS, or the last played chart for Title and Quick Setup. Unavailable chart audio falls back to the default theme. BMS audio includes keysounds and is prepared asynchronously, capped at five minutes and looped.
- `volume` (double)
  - master volume
- `bgm_volume` (double)
- `normalize_audio` (bool)
  - In-game RMS loudness control, default false. ON retains RMS → soft limiter → master; OFF applies linear master gain and only guards final output overflow. Menu music and song previews are unchanged.
- `keysound_volume` (double)

See [ASIO setup](asio-audio.md). ASIO holds the selected sample rate fixed and resamples chart audio. `frames` is a request negotiated to a legal driver size. Presets do not overwrite ASIO frames; `exclusive` and `periods` apply to WASAPI. ASIO failures do not fall back to WASAPI automatically.

### `input`

- `backend` (string)
  - `polling | rawinput`
  - defaults to `rawinput` on the current `1.7.2` release line
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
  - the current `1.7.2` runtime no longer drives a separate audio-thread judgement sub-step loop from this value
  - default is `4000` (`0.25ms`)
- `debounce_ms` (double)
  - real Press/Release transitions are preserved; only duplicate same-state events are removed from pressed-state tracking
  - clamped to the `0..25` range
  - default value is `8ms`
### `judge`

[Current timing windows, RANK tables and LN rules (Korean)](judgement-windows.md)

- BMS `#RANK` selects chart timing: `3/EASY` PG21ms, `2/NORMAL` PG18ms, `1/HARD` PG15ms, `0/VERYHARD` PG8ms. EASY keeps GR/GD/BAD at 65/115/210ms; the other ranks scale these by 18/21, 15/21 and 8/21. Missing/unsupported headers use EASY. Judge Easy/Hard mods apply afterward. RANK does not change `hold_grace`/`hold_break` settings or automatic-miss deadlines; tail PG/GR/GD boundaries do follow RANK.
- `pg`, `gr`, `gd`, `bd` (double, ms)
- default `pg / gr / gd` values are `21ms / 65ms / 115ms`
- default `bd` is `210ms`
- `Judge Easy` scales RANK-adjusted PG/GR/GD and hold tolerances by `1.35x`. EASY PG/GR/GD=`28.35/87.75/155.25ms`; BAD is fixed at `210ms` for every RANK; mask is unchanged
- `Judge Hard` uses EASY PG/GR/GD=`17.5/55.714286/98.571429ms`: multiply PG by `17.5/21` and GR/GD by `18/21` after RANK. BAD is fixed at `225ms` for every RANK; hold tolerances are unchanged
- `indirect_miss` (double, ms)
  - current profiles save and normalize this value to `340ms`, independently of the BAD hit window
  - default Normal/Easy/Hard automatically miss an unplayed note once it is more than `340ms` late. Normal/Easy record BAD; Hard records a combo-breaking indirect `POOR` / OD8 `MISS`
  - a late press outside BAD but before automatic timeout misses the expired note, then checks the next note; it cannot score a BAD hit outside the hit window
- New plays record `tenriff-native-score-v2-ruleset-4`. R3 retains its previous Easy/Hard windows and RANK/LN release behavior. R1/R2 retain PG20ms, no RANK and older LN releases; R1 also keeps Easy1.25x/Hard BAD340ms/automatic miss=BAD. Custom timing remains unofficial.
- `hold_grace` (double, ms; default `80ms`)
  - retained for configuration compatibility and as the lower bound of `hold_break`; not the current native tail PG/GR boundary
- `hold_break` (double, ms; default `200ms`)
  - timeout after a CN tail when it remains held, ending in BAD; `270ms` with Judge Easy, unchanged by RANK
  - internally always kept at or above `hold_grace`
- LN release immediately uses the current PG/GR/GD windows, with BAD outside GD. Standard LN completes automatically if held to the end. `No LN Release` does not ignore an early drop
- `mask` (double, ms; default `30ms`)
  - temporarily ignores additional presses on a masked lane; unchanged by RANK or Judge mods

### `speed`
- `rate` (double)
- `hispeed` (double)
- `target_scroll_bps` (double)
- Reference BPM is the tempo with the longest accumulated running time; repeated sections add together and STOP waits are excluded. This fixes the Hi-Speed reference while explicit `#SCROLL`, stops and reverse motion still apply. See [reference BPM](reference-bpm.md).

### `gauge`

Gauge Shift is always active. `mode.gauge` selects the starting tier: `ex_hard / hard / normal / easy`, from EX down to Easy. The selected tier and every lower tier are simulated independently from 100%; a failed tier yields to the next surviving tier. Gauge failure occurs only when all eligible tiers fail. Legacy `shift` means an EX start.

Grade/Session Mix uses a separate LR2-reference gauge, carried between songs, with indirect misses and failure below 2% HP. Normal long notes apply gauge once on completion/release. Ordinary `gauge.delta` settings do not apply to courses. See [LR2 gauge comparison and limits](lr2-gauge-audit.ko.md).

- `delta`: `ex_hard`, `hard`, `normal`, `easy` → `PG`, `GR`, `GD`, `BD`, `PR`.

### `graphics`
- `display_mode` (string)
  - `borderless | windowed | fullscreen`
  - the default is `borderless`; it is also the recommended mode for external overlays such as Discord, OBS, and Game Bar
  - `windowed` is a fixed-size window with a title bar and can be moved
  - `fullscreen` is DXGI exclusive fullscreen, where the current Discord Game Overlay is not displayed
- `resolution` (string)
  - `native`, legacy aliases `720p | 1080p | qhd`, or `widthxheight` (e.g. `1600x900`, `1366x768`, `1280x800`, `3440x1440`). Each axis accepts 320–8192px.
  - Graphics Settings combines common sizes with the current monitor's display modes. Press `F5` to refresh the list. Explicit custom sizes survive saving and restarting.
  - Layout preserves the 1920×1080 canvas aspect, with margins for other aspect ratios. Windowed mode scales proportionally to fit the title bar and taskbar work area.
  - Skin Settings uses a uniformly scaled view of the actual gameplay renderer, including field movement, lane/note/gear sizes, judgement/combo positions and skin fonts.
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
  - `off | accuracy | score` (default `off`)
  - Shows the live gap from the accuracy or score target. Normal gauge failure, clear and scoring rules remain active; missing the target alone does not fail a clear. Best records and ranking remain eligible when all other conditions are met
  - enabling Pacemaker disables Practice and Sudden Death; replay playback and multiplayer force it off
- `pacemaker_target_accuracy` (double)
  - 0..100, default 90.0; displays current standard Accuracy minus the target in percentage points
- `pacemaker_target_score` (int)
  - 0..10000, default 8000; displays current score minus the target pace proportional to judged note weights
- `auto_scratch_hide_lanes` (bool)
  - Default false. With auto_scratch in mods, hides actual BMS scratch columns while preserving other key widths and logical input IDs
  - `auto_scratch`: automatically plays actual BMS scratches only. Score multiplier 0%, ASSIST record, excluded from official best records and rankings
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

`last_played_chart_path` is the local path of the most recent chart whose gameplay started; empty by default. Replay and editor practice do not update it. Used by `title_music=last_played` across restarts.

| Field | Type, range, default | Behavior |
| --- | --- | --- |
| `profile_nickname` | string; UTF-8 ≤48 bytes | Display name; empty uses profile ID. Whitespace/control characters are normalized. |
| `profile_avatar_path` | string; UTF-8 ≤2048 bytes | Local PNG/JPG avatar path. |
| `language` | `en`, `ko`, `ja`; `en` | UI language; invalid values normalize to en. |
| `menu_font_size` | `normal`, `large`, `extra_large`; `normal` | Profile and first-run menu text at 100%, 115%, or 130%. Gameplay fonts remain skin-controlled. |
| `all_song_sources` | bool; `false` | Restore ALL SONG, combining registered folder caches and deduplicating identical chart paths. |
| `result_tail_ms` | double; `500` ms | Extra result-transition delay after judgement completion; the chart audio end is also considered when `audio.play_to_end=true`. |
| `require_enter_to_exit` | bool; `true` | Retained for read/write compatibility; the current Windows result-input path does not use it to auto-exit. |
| `show_cursor_in_gameplay` | bool; `true` | Show the mouse pointer during gameplay. |
| `active_song_source`, `recent_song_sources` | string / string[] | Current and recent song folders. |
| `song_sources_initialized` | bool; `false` | Explicit folder addition/removal sets this to true so an intentionally empty source list stays empty on relaunch. See [source management](library-management.md). |
| `session_mix_lr2_course_path` | string | Selected LR2 course file path. |
| `favorite_chart_keys` | string[] | Internal chart keys for favorites. |
| `collections` | object: name → string[] | Chart keys grouped by collection name. |
| `song_collection_filter` | string; `all` | All/favorites/collection filter; saved immediately. |
| `song_key_filter` | int: `0..16`; `0` | Key-count filter; zero means all. UI choices are 4K–10K, 12K, 14K and 16K. |
| `song_level_min_filter`, `song_level_max_filter` | int: `0..50`; `0` | Level bounds; zero disables the corresponding bound. |
| `difficulty_table_path` | string | Local header JSON or downloaded profile-cache header. Selecting a table switches to safe indexing; Native LV clears it and removes table metadata from a valid cache. |
| `difficulty_table_url` | string | Original HTTP(S) BMSTable page/header URL. Resolves bmstable metadata and caches header/data; local JSON selection clears it. |
| `online_records_server_url` | string | Records/ranking/chat API URL; failures do not block local play or records. |
| `tenriff_main_server_url` | string | F10 main-server API URL; default is `kTenRiffMainApiUrl` in Config.h. |
| `private_server_url` | string | F10 private API URL; remote HTTPS, HTTP allowed only for localhost. |
| `account_server_mode` | `main`, `private`; `main` | Last account-server selection. |

Difficulty-table headers use `name`, `symbol` and a local relative `data_url`; data entries match `md5` or `sha256` plus `level`. Remote imports are stored in the profile's `difficulty_tables` cache.

Native LV clears `difficulty_table_path` and `difficulty_table_url`; a valid cache removes external table metadata without a full rescan. The default is BMS `#PLAYLEVEL`; with difficulty calculation enabled, the cached calculated LV is retained.

### `skin`

Current `skin` settings and active skin assets can be exported/imported as a [portable `.trskin` preset](skin-presets.md). Audio devices, keymaps, accounts, song sources and timing calibration are excluded.

These are profile `config.json` skin settings. For a skin package's `skin.json` contract, see [Skin format](skin-format.md).

| Field | Type, range, default | Behavior |
| --- | --- | --- |
| `source` | string: `native`, `tenriff`, `lr2` | Skin source. |
| `tenriff_skin_name`, `lr2_skin_name` | string | Imported skin folder names. |
| `scratch_position` | `left`, `right`; `left` | Changes only 7+1 scratch display order; input/judgement lanes stay unchanged. |
| `lr2_resolution_mode` | `auto`, `sd`, `hd`, `fhd`; `auto` | LR2 coordinate resolution; auto uses `#DST_NOTE` coordinates, not filenames. |
| `visual_preset` | `classic`, `neon`, `minimal`, `tenriff`; `tenriff` | Selecting a preset in the menu resets its visual option bundle. |
| `note_shape` | `rect`, `circle`, `triangle`, `pentagon`, `hexagon`, `square`, `diamond`, `arrow`; `rect` | Procedural note shape. `hex` is a legacy alias for `hexagon`. |
| `note_image_aspect` | `stretch`, `contain`, `width`; `stretch` | Fill the rectangle / fit inside with aspect preserved / keep width and derive height from aspect. |
| `preserve_note_image_aspect_ratio` | bool; `false` | Legacy field; explicit `note_image_aspect` wins. Saved true for non-stretch modes. |
| `note_border_enabled`, `show_lane_dividers`, `show_judgement_line` | bool; `true` | Show note borders, lane dividers, and the judgement line respectively. |
| `note_divider_gap_px` | double: `0..40`; `12` px | Gap from each note edge to the divider; zero expands notes to the dividers. |
| `show_gear_boundary_line` | bool; `false` | Show the gear boundary line. |
| `show_timing_feedback` | bool; `true` | Show FAST/SLOW text independently of the bar. |
| `show_timing_bar` | bool; `true` | Show the timing bar; older profiles/skins without this field inherit the text switch. |
| `timing_bar_always_visible` | bool; `false` | Keep the timing scale visible (`true`) or show it for 0.75 seconds after a non-PG judgement (`false`). Text/live error markers retain the last valid non-PG error for 0.75 seconds even after PG. `show_timing_bar` remains the independent off switch. |
| `timing_feedback_override` | bool; `false` | After the user edits a switch, profile visibility choices take priority over the skin manifest. |
| `timing_text_offset_x`, `timing_bar_offset_x` | double: `-600..600`; `0` | Independent text/bar X offsets added to the existing judgement-area layout, in 1920x1080 base pixels. |
| `timing_text_offset_y`, `timing_bar_offset_y` | double: `-400..400`; `0` | Independent text/bar Y offsets in base pixels; positive moves down. |
| `show_hold_tail`, `hold_tail_taper_enabled` | bool; `false` | LN tail-cap visibility and taper respectively; no judgement-rule change. |
| `judgement_line_glow_enabled` | bool; `true` | Judgement-line glow. |
| `key_pulse_brightness` | double: `0..1`; `1` | Hit Burst brightness; zero disables it. |
| `key_pulse_enabled` | bool; `true` | Legacy ON/OFF mirror; false or zero brightness disables the burst. |
| `hit_burst_style` | `prism`, `ring`, `spark`; `prism` | Built-in Hit Burst material. |
| `hud_layout` | `classic`, `studio`; `studio` | HUD style for Native or imported LR2 solo play. LR2 note/key/gear images and geometry are preserved. Existing profiles without the key also default to `studio`; choose Classic in Options › Skins to restore the previous layout. Other image skins, ghost battles and multiplayer retain the classic layout. Unknown values use `studio`. |
| `hud_riff_map_visible` | bool; `true` | Show the note-density graph and clock to the left of the Studio Deck field. Options › Skins › Studio Riff Map saves this in profiles and skin presets. Classic HUD is unchanged. |
| `ui_font` | `default`, `malgun`, `bahnschrift`, `consolas`; `default` | Menu font; default is Segoe UI. Logo, rank and combo keep their dedicated fonts. |
| `key_label_position` | `bottom`, `top`, `off`; `bottom` | Lane key-label position. |
| `judgement_line_position` | double: `0..1`; `0.82` | Vertical judgement-line position ratio. |
| `gameplay_field_offset_x` | double: `-720..720`; `0` | Field X offset in 1920×1080 base pixels; additionally constrained to keep the field and ↔ handle visible. |
| `combo_position`, `judgement_position` | double: `0.10..0.78`; `0.24` | Independent combo/judgement Y anchors; missing judgement position inherits `combo_position` in older profiles. |
| `combo_offset_x`, `judgement_offset_x` | double: `-600..600`; `0` | Independent combo/judgement X offsets in 1920×1080 base pixels. |
| `combo_font_scale`, `judgement_font_scale` | double: `0.50..2`; `1` | Independent combo/judgement text multipliers. Skin Settings adjusts 50–200% in 5% steps or with mouse sliders; profile values also apply to imported skins. |
| `lane_background_opacity` | double: `0..0.45`; `0.18` | Lane-background opacity. |
| `black_playfield_enabled` | bool; `true` | Black field including lane gaps. |
| `visual_opacity` | double: `0.20..1`; `0.96` | Shared opacity multiplier for notes, receptors and key labels. |
| `note_outline_opacity` | double: `0..1`; `0.78` | Note-outline opacity. |
| `note_fade_in` | double: `0..1`; `0` | Top black fog depth; notes gradually appear. Zero is off. |
| `note_fade_out` | double: `0..1`; `0` | Black fog depth above the judgement line; notes gradually disappear. Zero is off. Receptors and HUD remain visible. |
| `hold_body_opacity` | double: `0..1`; `1` | LN-body opacity. |
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

No full-screen BGA dark filter is applied; configured black playfields, lane backgrounds and gear remain intact. Visual Latency is the fifth Skin Settings item.

Text readability applies across menus, options, the song library, results, help/chat/account overlays, the editor and gameplay. Light text gets a dark contour and dark text a light contour, preserving skin text colors and transparency.

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

## Settings usability and audio synchronization

New profiles default `audio.volume` to `0.7`; explicitly saved volumes remain intact. `audio.mute_when_inactive` defaults to `false` and gates output while another window is active, preserving music position and saved gain. ASIO Buffer Size is displayed in samples per channel; the internal `frames_per_buffer` API name stays compatible.

Key Settings presents horizontal Key 1… keys with separate primary and secondary bindings. Optional `keymap.json` `secondary_modes` follows the existing mode/lane structure. Either held input keeps the logical key held; × or Delete during capture clears only the secondary slot. The initial Options editor mode is 4K; opening from a chart uses its actual key count.

`skin.key_backdrop_enabled` and `skin.key_backdrop_opacity` (0–1) control held-key lane tint independently of hit-burst brightness. Skin defaults apply until the first user change, which saves `key_backdrop_override=true`. Judgement-line thickness follows note height. ALL SONG shows only table members while a difficulty table is active; Native LV restores all charts. Visuals/BGA follow the audible playback head while judgement/audio scheduling retain the write timeline. Actual speed changes play a short click.

Held-key backdrop RGB brightness uses `skin.key_backdrop_brightness` (0–2, default 1); maximum height uses `skin.key_backdrop_height` (0–1, default 1), anchored at the field bottom. Both are independent of opacity and match in gameplay, ghost play and Skin Settings previews. Hold Left/Right to repeat an enabled minus/plus setting.
