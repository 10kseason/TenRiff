# Gameplay and multiplayer follow-up (September 7, 2026)

The user explicitly resumed the collected client work before the planned September 12 break ended.
This follow-up ships as 1.7.1. No server code or protocol changed.

## Requested behavior and ownership

| Item | Implementation / completion boundary |
| --- | --- |
| Three players but only two shown | `app/MultiplayerPresentation.h` builds standings from every `PeerSessionSnapshot::participants` entry. The local HUD/result replaces the throttled local network copy. Missing scores remain visible; ties share competition rank. |
| Multiplayer HUD and result polish | Separate render includes `MenuWindow_draw_gameplay_multiplayer.inl` and `MenuWindow_draw_result_multiplayer.inl` draw opponents and the full room respectively. Eight result rows fit without scrolling. Remote scores remain unverified peer claims. |
| Score comparison follows current scores | `PeerBattleRuntimeRules.h` retains saturated local-minus-opponent text, but maps the marker using local / (local + opponent), with zero/zero centered. A room compares against the highest-scoring opponent. Result standings use the displayed score directly; no scoring formula changed. |
| Multiplayer completion | Spectating waits until every remote participant is terminal, not just the legacy primary opponent. The existing final-result wait already waits for all remotes. |
| Ten pastel Options buttons | `OptionsHubController` uses five columns and two rows. Existing IDs stay stable; Mods and Key Test are appended and initialized through the real MenuApp routes. Each item has a fixed muted color. Help sits below the cards. |
| LEVEL clipped to EL | Song-list level labels have a wider column. Native menu single-line text uses the existing fit calculation; wrapped help retains wrapping. |
| Smooth key-beam decay | `gameplay_interpolated_activity` continues the audio-side 200 ms decay at render cadence. This supports 60/144 Hz presentation; it does not guarantee a hardware frame rate. |
| Highest judgement feedback | P GREAT is yellow with short rainbow sparkles and a pop. GREAT retains cyan, GOOD is gray; lower judgement labels stay solid without pop or sparkle. |
| FAST/SLOW | Only the current applicable direction is drawn. Neither direction is drawn for absent/PG timing feedback; historical timing marks remain. |
| Judgement / combo placement | Skin settings expose independent Judgement X/Y and Combo X/Y. X offsets use 1920x1080 base pixels; Y uses the existing normalized field position. Config load/save clamps new values and preserves older profiles' judgement anchor. The skin preview shows both labels. |
| Normalize Audio ON/OFF | `audio/MixNormalizer.h` provides a stereo-linked, silence-gated RMS leveler for the combined gameplay mix, before the existing limiter/master volume. OFF by default. It preserves channel balance, caps gain at 0.1–2.0, and does no allocation or I/O on the audio thread. It is not offline LUFS normalization. |
| Repeated sound at every LN release | **Unconfirmed report from a 1.7.0 player (possibly the reported chart pack).** The production GameSession input/voice/mixer test emits one head sound and no new sound for early/exact/late release with both LN policies. Do not claim a fix without reproducing the user's executable/actual chart and audio. |

## Data flow and edit locations

Room snapshots → `app/MultiplayerPresentation.h` → value-only `render/MultiplayerPresentation.h`
→ `MenuAppTail.inl` HUD/result snapshots → multiplayer render includes. Network objects never enter
the renderer, and the protocol's eight-player capacity remains unchanged.

Audio UI → `AudioSettingsController`/view → `audio.normalize_audio` in profile config → GameSession's
mixed stereo callback → `MixNormalizer` → existing limiter/master volume. Reset gain and energy when
the audio sample rate initializes. Menu music and song previews are unchanged.

Skin controller rows → `SkinConfig` → immutable HUD/skin preview → renderer. New fields are
`skin.judgement_position`, `skin.judgement_offset_x`, and `skin.combo_offset_x`; existing
`skin.combo_position` remains Combo Y. Presentation changes do not affect judgement or replay input.

## Verification

```powershell
cmake --build build-ui-modern --config Release --target tenriff menu_visual_preview bms_parser_tests nk3_onnx_smoke gameplay_judgement_benchmark --parallel 4
ctest --test-dir build-ui-modern -C Release --output-on-failure
```

The existing Windows build wrapper `build/ui-modern-build.ps1` normalizes duplicate Path/PATH
environment keys when needed. Preview fixtures use the real D3D11 renderer with synthetic data;
they do not connect to a room, load user accounts, play audio or save records:

```powershell
menu_visual_preview.exe --options --small
menu_visual_preview.exe --result --three-players
menu_visual_preview.exe --result --eight-players --small
menu_visual_preview.exe --gameplay --three-players --60fps
menu_visual_preview.exe --gameplay --eight-players
menu_visual_preview.exe --skin-settings
menu_visual_preview.exe --small
```

`--small` is 960x540; default is 1280x720 and `--1080p` requests 1920x1080. The gameplay fixture
cycles PG/GREAT/GOOD, FAST/SLOW, moving notes and beams. Its requested 60/144 cadence is bounded by
VSync and actual display timing; screenshots alone do not establish sustained frame rate.

Tests cover 3/8-player projection, missing data, ties, local snapshot replacement, all-opponent
completion, relative score lead and overflow, five-column navigation/routes, config persistence,
independent positions, audio normalization/stereo/block-size behavior and deterministic key-beam
decay. `test_game_session_audio.cpp` exercises the production dispatch and mixer via a narrow
friend test accessor without starting any audio/input device or record writer.

Live remote multiplayer, hardware audio, actual sustained 144 FPS and every imported skin remain
separate manual checks. The LN sound report needs the affected executable/chart to reproduce.
