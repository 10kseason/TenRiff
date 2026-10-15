# TenRiff Gameplay Guide

**2026-10-15 · 1.9.1:** The user-authorized 1.9.1 includes the earlier volume/input/outro fixes, BMS TOTAL/RANDOM compatibility, Result colors and the 10 mark. A Home language button cycles EN → KO → JA → ZH and saves the choice. New BMS gauge/branch records remain local until server verification supports them. The fixed baseline stays 1.9.0; earlier releases, installations and profiles are preserved. Dated local notes below retain their historical no-version-bump/unpublished status; this 1.9.1 scope supersedes that status. [Changes and compatibility](feedback-1.9.1.en.md) · [Release verification gate](release-1.9.1-gate.md).

**2026-10-15 · BMS TOTAL / RANDOM / visual follow-up — unreleased, local build and CTest regression passed:** This local follow-up builds on 1.9.0.2. Result levels use the song lobby’s red level text and key-mode symbol colors, and the logo adopts the supplied 10-shaped mark. New ordinary BMS plays use LR2-based recovery/damage with #TOTAL and note count; authored #RANDOM branches receive a seed independent of lane randomization. Historical replays retain their original behavior. The earlier unified-volume changes remain included. Published 1.9.0.2, the fixed 1.9.0 baseline and the version number are unchanged. [Formulas, branching, compatibility and verification scope](bms-total-random-followup.en.md).

**1.9.1 BMS volume:** This guide describes the BMS performance volume shared by accompaniment and keysounds. [Settings and compatibility](config.en.md#audio).

1.9.1 shows difficulty-table loading status and changes debounce UI steps to 1ms, with a 20ms maximum and 0ms default. The old automatic 5→8ms rewrite is retired, preserving saved 5ms and 8ms values. The Korean skin label becomes `LN 끝 줄임`; appearance and judgement behavior are unchanged. In ordinary single-player with Listen to End enabled, `Esc` skips the outro directly to results without opening Pause once the final note and LN tail have been processed.

In 1.9.0.1, assign Hi-Speed -10/+10 in Profile (defaults F5/F6, range 0.50–50.00). Press Xbox/XInput digital buttons or LT/RT in Keymap to bind lanes, or in Profile to bind restart and Hi-Speed controls. Use the D-pad to navigate menus, A to select and B to go back; an otherwise unassigned Start pauses gameplay. LT/RT count as pressed above 30/255. Analog stick axes and vibration are outside this release. [Changes and compatibility](feedback-20261015.en.md).

The 1.9.0 release selects Normal / Hard / EX under `Options > Key Mode Settings > Judge Window`. Easy is removed from new selections but retained for historical replays. The session-only choice is not profile-persisted and resets to Normal on restart. See [EX timing and compatibility](judge-ex-1.9.0.ko.md) (Korean).
- New Normal/Hard plays retain `tenriff-native-score-v2-ruleset-5`; EX uses `tenriff-native-score-v2-ruleset-6`. Historical R1–R5 timing, Easy and LN policies remain unchanged; custom timing is unofficial.
- `Judge EX` uses EASY PG/GR/GD=`14/44.571429/78.857143ms` (80% of Hard), retaining BMS RANK scaling. BAD is fixed at 250ms; the separate automatic-miss deadline remains 340ms and the score factor remains Hard's 1.10.

This document is a user guide focused on how a first-time TenRiff player actually chooses a song, plays it, and checks the result.

For the detailed config structure, see [`docs/config.en.md`](config.en.md); for the current implementation scope, see [`docs/current-state.en.md`](current-state.en.md).

## 1. First Launch

On Windows, one of the following two methods is usually used:

```powershell
.\launch_win.bat
```

or

```powershell
.\TenRiff.exe --profile default
```

On first launch, the default profile is created automatically.

## 2. Basic Flow

The standard TenRiff play flow is:

1. Enter the Title screen
2. Choose a song in Song Select
3. Adjust Mode / Audio / Graphics / Skins / Keymap if needed
4. Start the song
5. Play after the `3 / 2 / 1` countdown
6. Check the Result screen
7. Return to Song Select with `Enter` or `Esc`

## 3. Basic Menu Controls

### Title

The Home language button cycles **EN → KO → JA → ZH → EN**. It saves through the existing `ui.language` setting and persists across restarts. Strings without a translation can fall back to English.
- `Up` / `Down`: move through the menu
- `Enter`: select
- `Esc`: quit

### Song Select

The 1.8.9 lobby separates large BEST numbers from maximum score, COMBO units and detail rows. Note-count labels and values are white, and a white outer frame groups the list with the art/BEST/filter cards. Enabled Start uses the same sky blue as Continue on results. S+ color and font are unchanged.
- `Up` / `Down`: move between songs
- `PageUp` / `PageDown`: fast movement
- Mouse wheel: move between songs
- Left click: select song
- Double click: start song
- `Enter`: start the currently selected song
- Click the center `MY BEST` card to open the chart's best Result; use its replay button or F1 to watch the replay.
- Top `RECORDS` tab: browse the current chart's local records and open a selected result.
- `Left / Right`: move navigation focus. `Tab` enters quick settings, where Up/Down selects and Left/Right adjusts.
- `Esc`: return to the previous screen
- `-` / `+`: adjust the next-play Rate immediately
- `F5`: reindex the song library; centered stage / percent / ETA and a progress bar appear while it runs
- Difficulty-table card (beside Song Library in the default native layout): click the name/URL to edit, File for local JSON, and Reset for native LV. Enter applies the URL; Esc cancels. The difficulty-table row in Sort / Filter remains available.
- A loading status is shown while the difficulty table is being loaded.

### Screens commonly used from Song Select
- `Mode`
  - Adjust Ghost Battle, Autoplay, Practice, Sudden Death, Key Mode, Gauge, Random, Mods, Rate, and Hi-Speed
- `Audio`
  - Adjust Master, BMS Performance and TenRiff Menu Music volumes, plus the BMS keysound mode
- `Graphics`
  - Adjust VSync, Refresh Hz, Performance HUD, BGA visibility, and the external ONNX BGA Upscaler
  - Turning `BGA` off disables gameplay image/video backgrounds and their decoder/upscaler work; Song Select previews remain visible
  - Selecting a model is separate from enabling the upscaler and accepting its high-spec warning. Experimental `Low-Power DirectX` only requests DirectXMinPower and does not explicitly select or verify an NPU
- `Skins`
  - Switch native / TenRiff `skin.json` / LR2 skins, adjust Visual Latency, use native lower digital-piano keys with held depth and impact glitches, import one LR2 folder or batch-import an independent non-IIDX `LR2files/Theme` root (IIDX-dependent themes are skipped), port aspect-preserved lower Gear frames that enlarge with field size and clip below the judgement line, and adjust fixed-divider note gaps/size, Black Playfield, judgement-line position, LN body width, and lane colors
- `Keymap`
  - Change key bindings and run the NKRO test

The table picker also offers F1 Aery 5K, F2 Aery 7K, F3 Revive 10K and F4 Native LV. Native LV applies immediately to display, sorting, grouping and numeric filters. See [library management](library-management.md).

## 4. Recommended Starter Settings

These settings are a good starting point:

- `Mode > Gauge`: `normal`
- `Mode > Sudden Death`: enable only when you want a first-OD8-MISS challenge
- `Mode > Rate`: `1.0x`
- `Mode > Hi-Speed`: start with the default value
- `Graphics > Display`: use `Borderless` when using Discord voice overlay
- `Graphics > Performance HUD`: turn it on only when needed
- `Skins > Visual Latency`: start at the default `0ms`
- `Audio > Keysound Mode`: `follow` is recommended for BMS

If the notes look too slow or too fast, adjust only `Hi-Speed` first. If the judgement feels correct but the visuals look late or early, adjust `Visual Latency`.

### Discord voice overlay

Enable the overlay and Voice widget under Discord's `User Settings > Game Overlay`, then run TenRiff in `Graphics > Display > Borderless` or `Windowed`. The current Discord Game Overlay is not displayed in DXGI exclusive fullscreen. To avoid covering gameplay information, pin the Voice widget at bottom-left and leave TenRiff's `Performance HUD` off. If Discord does not detect TenRiff automatically, add the running `TenRiff.exe` under `Registered Games`.

See Discord's [official Game Overlay guide](https://support.discord.com/hc/en-us/articles/217659737-Game-Overlay-101) for the client-side settings.

## 5. Default Key Layout

The default keymap is selected automatically based on the chart's key count.

- `4K`: `D F L ;`
- `5K`: `D F K L ;`
- `6K`: `S D F J K L`
- `7K`: `W E R M I O P`
- `8K`: `W E R V M I O P`
- `9K`: `A S D F Space H J K L`
- `10K`: `Q W E R V M I O P [`
- `12K`: `Q W E R F V M I O P [ ]`
- `14K`: `Q W E R T F V M I O P [ ] \`
- `16K`: `Q W E R A S D F U I O P J K L ;`

If this is not the layout you want, you can change it in `Options > Keymap`.

## 6. Things to Know Before Starting Play

### Chart format
- Chart indexing and play are limited to the BMS family (`.bms/.bme/.bml/.pms`); `.osu` files are unsupported.

### Loading
- Right after song start, chart-loading progress may be shown.
- Pressing `Esc` during ordinary loading cancels the start and returns to Song Select. Session Mix ignores Esc during loading and the initial countdown.

### Countdown
- After loading finishes, the `3 / 2 / 1` countdown appears first.
- Input during the countdown does not count toward score judgement.

## 7. In-Game Controls

- Chart key input: based on the current keymap
- `Esc`: pause ordinary single-player immediately. Continue or Esc resumes after a 3/2/1 countdown while audio, chart time and scoring remain frozen. Esc during the resume countdown returns to the pause menu.
- With Listen to End (`play_to_end=true`) enabled, ordinary single-player `Esc` skips the outro directly to results after the final note and LN tail have been processed. Pause does not open; this also applies to autoplay and replay.
- Session Mix ignores Esc during play; its between-stage result exits remain available. Multiplayer retains its Esc abort behavior.
- `F3`: decrease Hi-Speed
- `F4`: increase Hi-Speed
- `F5` (default): Hi-Speed -10; reassign in Profile
- `F6` (default): Hi-Speed +10; reassign in Profile
- `F7` / `F8`: decrease / increase visual latency
- `F11`: open / close in-game chat (also F11 in menus)
- Left/right `LShift` / `RShift` can be assigned as play keys. The shortcuts above do not require Shift.
- `F9`: save a screenshot of the current screen

Pause Continue, Restart, Exit and setting −/+ buttons also support mouse clicks. Text or blank space in adjustable settings rows only selects the row without changing its value.

Pacemaker shows the live gap from the accuracy or score target. Normal gauge failure, clear and scoring rules remain active; missing the target alone does not fail a clear. Best records and ranking remain eligible when all other conditions are met.


Hi-Speed changes only the visual scroll speed; it does not change the judgement timing itself.
Rate changes playback tempo and chart scheduling, but it does not change visual scroll speed at the same Hi-Speed.
Reference BPM is the tempo with the longest accumulated running time; repeated sections add together and STOP waits are excluded. This fixes the Hi-Speed reference while explicit `#SCROLL`, stops and reverse motion still apply. See [reference BPM](reference-bpm.md).

## 8. How to Read the HUD

**1.8.9 Studio Deck:** Use `Options > Skins` to choose Studio Deck/Classic and Studio Riff Map visibility for Native/LR2 solo play. Studio places song information and Rate/HS/BPM/Scroll/Line/Latency at the upper right, score/accuracy/judgement counts at the lower left, and a horizontal 64-bin density map at the lower right. The map progresses left to right with a current-position marker and elapsed/total time. The gauge rail and marker are larger.

Cards scale as groups or move to the wider opposite side when space outside the field is narrow. If score and map share a side, score moves above the map; very narrow maps omit time labels. When an extreme field position/width and Performance HUD leave no upper-side space, only the upper information card is hidden. Hiding the map leaves other card positions unchanged. LR2 note/key/gear assets and Classic layouts for ghost, multiplayer and other image skins remain intact. [Runtime checks and limits](release-1.8.9-gate.md) (Korean).

The in-game HUD usually shows:

- Title / artist
- BPM
- Current `Rate`
- Current `Hi-Speed`
- Gauge value and current gauge type
- Native TenRiff Score
- Combo
- Recent judgement (`PG / GR / GD / BD / PR`)
- Timing deviation in milliseconds

Off-center hits show a signed timing label below the judgement: `FAST -12 ms` for early input and `SLOW +18 ms` for late input. Hits that round to `0 ms` omit the timing label.
`Skin Settings > FAST/SLOW Indicator` toggles both this label and the timing-history marker. Judgement grades remain visible when it is off.

If you enable `Graphics > Performance HUD`, you can also see the frame graph, average FPS, low FPS, and gameplay timing debug information.

### Judgement/combo feedback and volume

- Only P-GREAT uses yellow text, rainbow sparkles and a pop. GREAT stays cyan; GOOD is gray, and lower judgement labels remain stationary.
- FAST/SLOW shows only the applicable direction and is omitted for P-GREAT or offsets rounded to 0ms.
- `Options > Skins` saves independent judgement X/Y and combo X/Y positions.
- `Options > Audio > Normalize Audio` applies RMS leveling to the gameplay mix and defaults to OFF; menu music and song previews are unaffected.

## 9. Judgement and Gauge

The 1.9.0 release selects Normal / Hard / EX under `Options > Key Mode Settings > Judge Window`. Easy is removed from new selections but retained for historical replays. The session-only choice is not profile-persisted and resets to Normal on restart. See [EX timing and compatibility](judge-ex-1.9.0.ko.md) (Korean).

### Judgement
The current default judgement labels use the following abbreviations:

- `PG`: Perfect Great
- `GR`: Great
- `GD`: Good
- `BD`: Bad
- `PR`: Poor / Miss

Base PG is **±21/18/15/8ms** for BMS RANK EASY/NORMAL/HARD/VERYHARD. EASY keeps GR/GD/BAD at ±65/115/210ms; other ranks scale these proportionally. `Judge Hard/EX` are separate mods applied after RANK. See [timing windows (Korean)](judgement-windows.md) for the full tables and inclusive boundaries.

Empty POOR applies only on the early side of the next note. Input behind a consumed or already-passed note never adds a rear empty POOR.

`No LN Release` disables charge-LN tail release judgement and has an exact `1.00x` score multiplier.

OD8 conversion statistics remain inside JSON for Sudden Death and legacy replay compatibility, but are not shown on Gameplay or Result screens.

Casual Score has a 10,000 maximum and uses `PG 6 / GR 3 / GD 1 / PR 0 / FAIL 0` judgement weights. An LN is one object whose head and tail each carry 0.5 weight, while Detail Score keeps its separate calculation.

Standard Accuracy is the judgement-weighted average of `PG / GR / GD / BD / PR = 100 / 80 / 50 / 20 / 0%`. It is separate from Detailed Accuracy, which reflects timing within each judgement band. Standard Accuracy has no extra timing-spread deduction or 99.5% cap.

**Rank from 1.9.0 onward:** surviving completions use standard accuracy thresholds of `99% S++ / 98% S+ / 95% S / 93% A+ / 90% A / 85% B / 80% C`, with each threshold inclusive. Every surviving completion below 80%, including below 75%, receives D; failure receives F. Thresholds use accuracy before display rounding.

Explicit aborts (`ABORTED` or `aborted`) show `--`, autoplay shows `AUTO`, and missing records show `--`. Legacy failed records without enough information to distinguish death from abort retain F. Results, history and BEST recalculate the displayed grade from statistics without rewriting raw records or changing score calculation or numeric standings. The fixed 1.8.7 baseline is unchanged. [Grade display and compatibility (Korean)](grade-display.ko.md).

### Gauge

**1.9.1 BMS behavior:** New ordinary BMS plays use `TOTAL / N` recovery for Normal/Easy, and fixed recovery plus TOTAL/note-count damage correction for Hard/EX. `N` counts valid current note heads, with each LN counted once. Hard judgement damage is multiplied by 0.6 below 32%. TenRiff retains its 100% start, zero failure, gauge shifts, judgement classification, 0.5 LN head/tail weights and authored mine damage. Courses retain their course gauge; peer battle retains v1. Existing `gauge.delta` values remain for historical replay compatibility and do not tune the new BMS v2 calculation.

Gauge Shift is always active. `EX / Hard / Normal / Easy` select the starting tier, stored as `ex_hard / hard / normal / easy`. The selected tier and every lower tier start at 100% and accumulate judgements independently. When the current tier reaches zero, the next surviving tier takes over; the highest surviving tier at the end is the final result. Gauge failure occurs when all eligible tiers fail.

Grade/Session Mix uses a separate LR2-reference gauge, carried between songs, with indirect misses and failure below 2% HP. Normal long notes apply gauge once on completion/release. Ordinary `gauge.delta` settings do not apply to courses. See [LR2 gauge comparison and limits](lr2-gauge-audit.ko.md).

Legacy `shift` means an EX start. Practice retains separate completion rules. Pacemaker follows normal gauge and clear rules.

`Sudden Death (1 MISS)` is not a gauge type: it forces the gauge to zero and ends the run on the first OD8-converted object `MISS`. Native `BAD` timing alone and empty-key `POOR` do not count, and it cannot be enabled together with Practice No-Fail.

## 10. Result Screen

The 1.8.9 single-result screen switches between `SUMMARY / LANES / NOTE TYPES / TIMELINE` buttons. It distinguishes a green analysis frame, blue tabs and cyan Continue; Replay/Retry center their icon and text as one group. The left groups share an outer frame with inner dividers, and MAX COMBO numbers and units are separate. Larger text changes presentation, preserving score and analysis definitions. See the [release record](release-1.8.9-gate.md) for screen/control validation and limits.

After play ends, the Result screen shows:

- Clear / Game Over status
- Rank
- Native TenRiff Score
- Whether pause was used during the recorded play
- Accuracy
- Max Combo
- PG / GR / GD / BD / PR totals
- Average timing deviation and variance
- Final gauge and gauge history
- Saved replay / result file name

The return keys are:

- `R / Left`: restart the same chart immediately
- `F1`: play the saved replay when one is available
- `Enter`: return to Song Select
- `Esc`: return to Song Select

## 11. Common Adjustments

### The notes feel too dense or hard to read
- Increase `Hi-Speed`.
- If needed, also adjust `Skins > Note & Field Size / Note Height / Judge Line`.

### The judgement feels right, but the visuals look late or early
- Adjust `Skins > Visual Latency`.
- Positive values draw the notes earlier.

### BMS song audio or keysounds are too loud or too quiet

- Adjust `Audio > BMS Performance Volume` to change accompaniment and keysounds together. `Master Volume` also affects the final level.
- There is no separate control to make keysounds louder or quieter relative to BGM.
- The late-input path that could stay silent under `follow` was fixed in 1.2.6: judgement time stays unchanged while only the audible start is pinned to the current writable buffer.
- If keysounds are missing, check `Keysound Mode`: `follow` plays them on note hits, `autoplay` plays them at chart times, and Off silences them. This mode does not change their volume relative to BGM.
- Also check that both Master and BMS Performance volumes are above zero and that the chart's `#WAV` asset paths are valid.

### The input feels unstable or key conflicts are suspected
- Use `Options > Keymap > NKRO Test` and press multiple keys at once to check.
- If needed, change the key layout so the hand positions do not overlap.

## 12. Recommended Adaptation Order

When you start, the easiest order is:

1. Choose an easy song in `Song Select`
2. In `Mode`, confirm `Gauge=Normal` and `Rate=1.0x`
3. After playing, adjust only `Hi-Speed` first
4. If it still feels off, adjust `Skins > Visual Latency`
5. If your hand position feels uncomfortable, change the `Keymap`
6. Finally, use `Skins` to adjust note size and judgement-line position

## 13. Related Documents

- Current implementation state: [`docs/current-state.en.md`](current-state.en.md)
- Config structure: [`docs/config.en.md`](config.en.md)
- Menu structure: [`docs/menu.en.md`](menu.en.md)
- Play loop / engine structure: [`docs/core-loop.en.md`](core-loop.en.md)

**Authored branches:** `#RANDOM N` selects an integer from 1 through N and applies the matching `#IF` section’s notes, assets and timing commands. Alternative sections are not layered together. Fresh attempts use a new branch seed even when lane randomization is off; a repeated draw can still select the same branch. Replays retain the original choices. New records currently remain local.

[Formulas, branching, compatibility and verification scope](bms-total-random-followup.en.md).
