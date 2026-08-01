# TenRiff Gameplay Guide

This document is a user guide focused on how a first-time TenRiff player actually chooses a song, plays it, and checks the result.

For the detailed config structure, see [`docs/config.en.md`](config.en.md); for the current implementation scope, see [`docs/current-state.en.md`](current-state.en.md).

## 1. First Launch

On Windows, one of the following two methods is usually used:

```powershell
.\launch_win.bat
```

or

```powershell
.\build-dist\Release\TenRiff.exe --songs .\songs --profile default
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
- `Up` / `Down`: move through the menu
- `Enter`: select
- `Esc`: quit

### Song Select
- `Up` / `Down`: move between songs
- `PageUp` / `PageDown`: fast movement
- Mouse wheel: move between songs
- Left click: select song
- Double click: start song
- `Enter`: start the currently selected song
- `Left` / `Right`: switch focus on the left-side menu
- `Esc`: return to the previous screen
- `-` / `+`: adjust the next-play Rate immediately
- `F5`: reindex the song library; centered stage / percent / ETA and a progress bar appear while it runs
- `Browse > Difficulty Table`: copy a BMSTable HTML/header link and press `Enter` to import, use `Right` for local JSON, or `Left` to clear

### Screens commonly used from Song Select
- `Mode`
  - Adjust Ghost Battle, Autoplay, Practice, Sudden Death, Key Mode, Gauge, Random, Mods, Rate, and Hi-Speed
- `Audio`
  - Adjust Master / BGM / Keysound volume and the BMS keysound policy
- `Graphics`
  - Adjust VSync, Refresh Hz, Performance HUD, Display Offset, BGA visibility, and the external ONNX BGA Upscaler
  - Turning `BGA` off disables gameplay image/video backgrounds and their decoder/upscaler work; Song Select previews remain visible
  - Selecting a model is separate from enabling the upscaler and accepting its high-spec warning. Experimental `Low-Power DirectX` only requests DirectXMinPower and does not explicitly select or verify an NPU
- `Skins`
  - Switch native/LR2 skins, import an LR2 folder, and adjust fixed-divider note gaps/size, Black Playfield, judgement-line position, LN body width, and lane colors
- `Keymap`
  - Change key bindings and run the NKRO test

## 4. Recommended Starter Settings

These settings are a good starting point:

- `Mode > Gauge`: `normal`
- `Mode > Sudden Death`: enable only when you want a first-OD8-MISS challenge
- `Mode > Rate`: `1.0x`
- `Mode > Hi-Speed`: start with the default value
- `Graphics > Display`: use `Borderless` when using Discord voice overlay
- `Graphics > Performance HUD`: turn it on only when needed
- `Graphics > Display Offset`: start at the default `0ms`
- `Audio > Keysound Mode`: `follow` is recommended for BMS

If the notes look too slow or too fast, adjust only `Hi-Speed` first. If the judgement feels correct but the visuals look late or early, adjust `Display Offset`.

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
- `16K`: `Q W E R A S D F U I O P J K L ;`

If this is not the layout you want, you can change it in `Options > Keymap`.

## 6. Things to Know Before Starting Play

### Chart format
- BMS-family charts (`.bms/.bme/.bml/.pms`) are the default; enabling `OSU Charts` in Mode Settings also indexes and plays 4K-10K osu!mania `.osu`.

### Loading
- Right after song start, chart-loading progress may be shown.
- Pressing `Esc` during loading cancels the start and returns to Song Select.

### Countdown
- After loading finishes, the `3 / 2 / 1` countdown appears first.
- Input during the countdown does not count toward score judgement.

## 7. In-Game Controls

- Chart key input: based on the current keymap
- `Esc`: open the pause menu (Continue / Restart / Exit) in single-player; abort play in multiplayer
- F3: decrease Hi-Speed
- `F4`: increase Hi-Speed
- `F5`: decrease Hi-Speed significantly
- `F6`: increase Hi-Speed significantly
- `F9`: save a screenshot of the current screen

Hi-Speed changes only the visual scroll speed; it does not change the judgement timing itself.
Rate changes playback tempo and chart scheduling, but it does not change visual scroll speed at the same Hi-Speed.

## 8. How to Read the HUD

The in-game HUD usually shows:

- Title / artist
- BPM
- Current `Rate`
- Current `Hi-Speed`
- Gauge value and current gauge type
- Native TenRiff Score
- Auxiliary `OSU OD8` score converted from real input timing with osu!mania stable OD8 / ScoreV1
- Combo
- Recent judgement (`PG / GR / GD / BD / PR`)
- Timing deviation in milliseconds

Off-center hits show a signed timing label below the judgement: `FAST -12 ms` for early input and `SLOW +18 ms` for late input. Hits that round to `0 ms` omit the timing label.

If you enable `Graphics > Performance HUD`, you can also see the frame graph, average FPS, low FPS, and gameplay timing debug information.

## 9. Judgement and Gauge

### Judgement
The current default judgement labels use the following abbreviations:

- `PG`: Perfect Great
- `GR`: Great
- `GD`: Good
- `BD`: Bad
- `PR`: Poor / Miss

The `OSU OD8` value is an auxiliary comparison score capped at 1,000,000; it does not change TenRiff's native score, rank, or clear result.

Native Score is 90,000 judgement points plus 10,000 cumulative-combo points. An all-`PG` full combo is exactly 100,000, and an LN is one object whose head and tail each carry 0.5 weight.

Accuracy starts from `PG / GR / GD / BD = 100 / 80 / 50 / 20%` and removes up to another 0.5 percentage points according to timing inside each judgement band. Even an all-`PG` run cannot exceed 99.5% when its PG timing span is wider than 8ms.

Rank uses `<75 F`, `75 B`, `80.5 A`, `86.5 A+`, `90 S`, `95.5 S+`, `98 AA`, `99 SS`, and `99.75 SSS` boundaries.

### Gauge
The five selectable gauge types are:

- `ex_hard`
- `hard`
- `normal`
- `easy`
- `shift`

Fixed gauges (`ex_hard / hard / normal / easy`) start at `100%` and never change type during play.

- `ex_hard`: lower recovery and heavier `BAD` / `POOR` damage than Hard; immediate Game Over at `0%`
- `hard`: immediate Game Over at `0%`
- `normal`: immediate Game Over at `0%`
- `easy`: immediate Game Over at `0%`
- `shift`: simulates EX-Hard / Hard / Normal / Easy independently from 100%; when the current tier dies, it selects the next tier that has survived the same judgement history, and the highest surviving tier is final

Gauge transitions occur only when `shift` is explicitly selected.
`Sudden Death (1 MISS)` is not a gauge type: it forces the gauge to zero and ends the run on the first OD8-converted object `MISS`. Native `BAD` timing alone and empty-key `POOR` do not count, and it cannot be enabled together with Practice No-Fail.

## 10. Result Screen

After play ends, the Result screen shows:

- Clear / Game Over status
- Rank
- Native TenRiff Score
- Auxiliary `OSU OD8` score and converted judgement totals
- Accuracy
- Max Combo
- PG / GR / GD / BD / PR totals
- Average timing deviation and variance
- Final gauge and gauge history
- Saved replay / result file name

The return keys are:

- `Left`: restart the same chart immediately
- `F1`: play the saved replay when one is available
- `Enter`: return to Song Select
- `Esc`: return to Song Select

## 11. Common Adjustments

### The notes feel too dense or hard to read
- Increase `Hi-Speed`.
- If needed, also adjust `Skins > Note Width / Note Height / Judge Line`.

### The judgement feels right, but the visuals look late or early
- Adjust `Graphics > Display Offset`.
- Positive values draw the notes earlier.

### BMS keysounds are too loud or too quiet
- Adjust `Audio > Keysound Volume`.
- This can be tuned separately from BGM.
- The late-input path that could stay silent under `follow` was fixed in 1.2.6: judgement time stays unchanged while only the audible start is pinned to the current writable buffer.
- If it is still silent, verify `Keysound Mode=follow`, a non-zero volume, and the chart's `#WAV` asset paths.

### The input feels unstable or key conflicts are suspected
- Use `Options > Keymap > NKRO Test` and press multiple keys at once to check.
- If needed, change the key layout so the hand positions do not overlap.

## 12. Recommended Adaptation Order

When you start, the easiest order is:

1. Choose an easy song in `Song Select`
2. In `Mode`, confirm `Gauge=Normal` and `Rate=1.0x`
3. After playing, adjust only `Hi-Speed` first
4. If it still feels off, adjust `Display Offset`
5. If your hand position feels uncomfortable, change the `Keymap`
6. Finally, use `Skins` to adjust note size and judgement-line position

## 13. Related Documents

- Current implementation state: [`docs/current-state.en.md`](current-state.en.md)
- Config structure: [`docs/config.en.md`](config.en.md)
- Menu structure: [`docs/menu.en.md`](menu.en.md)
- Play loop / engine structure: [`docs/core-loop.en.md`](core-loop.en.md)
