# Menu Architecture Refactor Plan

Status: implementation complete (Phases 0-6, 2026-08-28). Automated gates pass; the manual D3D11 GUI smoke remains a release-candidate check.

## Objective

Make menu navigation and settings screens easier to extend and test without changing visible behavior. The work is incremental: every slice must build, pass focused tests, and preserve the existing input-thread -> app-state -> immutable render snapshot -> render-thread flow.

The first delivery covered the Options family and Audio Settings. The follow-on delivery expanded the same controller/effect pattern to every settings family and consolidated repeated screen metadata without changing the Song Select, Multiplayer, Gameplay, or Result ownership boundaries.

## Delivery record

- The pre-change Release baseline built successfully and passed 638 unit tests.
- The no-Git safety archive is `build/refactor-backups/menu-refactor-preflight-20260828.zip`: 169 entries, SHA-256 `B0698B274479ED3770274338161AF395CCD9C3452FB01C927C100790782CE276`.
- `MenuNavigator` now owns the active screen and history. The old active-screen field and both shared return-screen fields have no remaining references.
- Options and Audio Settings now use stable typed identifiers. Audio keyboard and pointer adjustments share one range/snap implementation, and its view builder produces value-only rows.
- The Phase 0-4 Release unit run passed 657 tests. After Phases 5-6, the final Release unit run passed 706 tests, the registered Release CTest suite passed 3/3, and the Release client built successfully.
- The focused MSVC AddressSanitizer CTest target `bms_parser_tests_asan` passed 1/1 in `RelWithDebInfo` after the completed Phase 6 source was rebuilt.
- Input, Calibration, Graphics/ONNX, Keymap/NKRO, Skin, and Mode/Mod Manager now have screen-local controllers with stable typed row identifiers and focused regression tests. Filesystem dialogs, thread restarts, persistence, skin preview rendering, and song reindexing remain explicit `MenuApp` boundary effects.
- `MenuScreenDescriptor` is the exhaustive source for stable titles, skin background keys/fallbacks, root snapshot routing, generic builder routing, Options-family music selection, and input-footer policy. Dynamic Quick Setup and Song Select titles remain in their owning view logic.
- Independent transition, Audio, and Options reviews found two boundary inconsistencies. The implementation now gives pointer no-op adjustments consistent visual acknowledgement and routes the Multiplayer Space shortcut through the same active-match/Ready checks as the visible Options action.
- Automated checks do not replace the remaining manual D3D11 GUI smoke for keyboard, click/drag, music/background selection, persistence, and one-level Back behavior.

## Baseline and problem shape

The 2026-08-28 source snapshot has these indicators:

- `MenuApp.cpp`: 154,211 bytes; `MenuAppTail.inl`: 166,921 bytes; `MenuApp.h`: 37,250 bytes.
- 20 values in the private `MenuApp::Screen` enum.
- 73 direct assignments to `screen_`, 19 assignments to the two shared return-screen fields, and 9 `switch (screen_)` dispatch sites.
- 172 numeric `settings_cursor_` conditions across app implementation files.
- Input mutation, navigation, persistence/restart effects, localization, and render DTO construction are coupled through `MenuApp` state.

These counts are diagnostics, not line-count targets. The success criterion is clearer ownership and focused tests.

## Invariants

- No user-visible behavior, layout, key binding, persisted config format, or render-thread contract changes during extraction.
- `render::MenuRenderData` remains value-only. It must not contain callbacks or references to mutable app state.
- Filesystem, audio/input/render restart, and config persistence remain application-boundary effects owned by `MenuApp` adapters.
- Controllers receive normalized menu actions and return explicit effect requests; they do not call threads or platform APIs.
- Top-level progression and nested navigation are distinct operations. History must never survive a root reset accidentally.
- Each phase is independently releasable. Do not combine the work with unrelated cleanup.

## Target structure

```text
src/app/menu/
  README.md
  MenuScreen.h
  MenuScreenDescriptor.h/.cpp
  MenuNavigator.h/.cpp
  OptionsHubController.h/.cpp
  MenuAction.h
  settings/
    SettingsRowModel.h
    AudioSettingsController.h/.cpp
    AudioSettingsView.h/.cpp
    InputSettingsController.h/.cpp
    InputSettingsView.h/.cpp
    CalibrationSettingsController.h/.cpp
    CalibrationSettingsView.h/.cpp
    GraphicsSettingsController.h/.cpp
    GraphicsSettingsView.h/.cpp
    KeymapSettingsController.h/.cpp
    KeymapSettingsView.h/.cpp
    SkinSettingsController.h/.cpp
    ModeSettingsController.h/.cpp
tests/unit/
  test_menu_navigator.cpp
  test_menu_screen_descriptor.cpp
  test_audio_settings_controller.cpp
  test_*_settings_controller.cpp
  test_options_hub_controller.cpp
```

`MenuNavigator` is the sole owner of the active screen and navigation history:

- `reset(screen)`: enter a new root flow and clear history.
- `push(screen)`: enter a nested screen and remember the current screen.
- `replace(screen)`: replace the current step while preserving deliberate history.
- `back()`: pop exactly one nested screen; report failure at a root instead of inventing a destination.

`SettingsRowModel` uses stable per-screen row identifiers and explicit row kinds (`Action`, `Toggle`, `Choice`, `Numeric`, `Slider`). Numeric rows carry finite minimum, maximum, and step values. Pointer and keyboard input both become the same typed adjustment intent. Controllers return effect flags such as `RenderChanged`, `PersistConfig`, and `RestartAudio`; `MenuApp` performs those effects after state mutation.

Localization stays outside mutation logic. `AudioSettingsView` builds a value-only `GenericMenuData` from controller state, runtime config, and the current language choice.

## Execution phases

### Phase 0 - Safety baseline and characterization (complete)

1. Record the current Release test/build result and create a timestamped archive of files touched by the first slice because this source package currently has no Git metadata.
2. Add navigation sequence fixtures for:
   - Title -> Options -> Audio -> Options -> Title
   - Song Select -> Options -> Audio -> Options -> Song Select
   - Multiplayer -> Options -> Audio -> Options -> Multiplayer
   - Options -> Keymap -> confirmation/test -> Keymap/Options
3. Add Audio Settings behavior fixtures covering every row, save-on-back, one audio restart request, slider clamping/snapping, and keyboard/pointer parity.

Exit gate: characterization cases are explicit and the existing 638-test baseline still passes.

### Phase 1 - Establish explicit navigation (complete)

1. Move `Screen` to `MenuScreen.h` without changing enum values or screen-kind mapping.
2. Add `MenuNavigator` and its pure unit tests.
3. Replace direct `screen_` ownership with the navigator. Initially map existing transitions to `reset`, `push`, or `replace` according to the documented flow.
4. Migrate Options entry and child-screen back behavior to `push`/`back`.
5. Remove `submenu_return_screen_` and `options_return_screen_` only after all callers use navigation history.

Exit gate: both return-screen fields have no references; the three Options origin flows pass; menu music, background selection, help, repeat input, pointer dispatch, and snapshot publishing all read the navigator's current screen.

### Phase 2 - Normalize settings actions and row identity (complete)

1. Introduce `MenuAction` for move, activate, adjust, set-ratio, and back intents.
2. Give Audio Settings rows a stable `AudioSettingId`; stop using raw indexes as behavior identifiers.
3. Keep numeric row constraints in one typed specification consumed by both keyboard and pointer paths.
4. Keep renderer hit data serializable by carrying the stable row identifier as data, never a callback.

Exit gate: Audio Settings contains no `settings_cursor_ == <number>` behavior branches, and equivalent keyboard/pointer inputs produce identical config values.

### Phase 3 - Extract Audio Settings as the proving screen (complete)

1. Move cursor/dirty state and config mutation into `AudioSettingsController`.
2. Move render-data construction into `AudioSettingsView`.
3. Let the `MenuApp` adapter translate raw input to `MenuAction`, execute returned effects, and publish a snapshot.
4. Verify that leaving the screen persists once and restarts audio once only when audio settings changed.

Exit gate: Audio Settings behavior and appearance are unchanged; controller tests cover normal, boundary, no-op, and back cases; `MenuAppSettings.cpp` no longer owns Audio row mutation/render logic.

### Phase 4 - Extract the Options hub (complete)

1. Move grid cursor movement and selected destination mapping into `OptionsHubController`.
2. Route every Options card through `MenuNavigator::push`.
3. Keep screen-entry preparation (skin discovery, keymap preparation, first-run setup) in explicit `MenuApp` boundary functions.

Exit gate: adding an Options card requires one typed identifier/destination entry plus its view text, not coordinated edits to return flags and multiple row-index switches.

### Phase 5 - Expand one settings family at a time (complete)

Use the proven pattern in this order:

1. Input Settings and Calibration.
2. Graphics Settings and its ONNX confirmation step.
3. Keymap, confirmation, and NKRO test.
4. Skin Settings.
5. Mode Settings and Mod Manager.

Complete focused tests and the normal Release build after each family. Graphics live-apply, input restart, key capture, skin import/preview, and song reindex effects stay at the application boundary.

Delivery note: Skin Settings keeps its specialized preview/render assembly in `MenuAppSkin.cpp`; the extracted controller owns selection, edit state, dirty state, and pure configuration mutation. Mode Settings follows the same split, with persistence and reindex effects executed by the `MenuApp` adapter.

### Phase 6 - Consolidate snapshot routing (complete)

1. Move screen metadata such as title, help text, skin background key, and generic-view builder selection into explicit screen descriptors.
2. Keep a small, exhaustive root dispatcher; avoid hidden registration, reflection, or runtime-discovered screens.
3. Split large render builders only where a screen family now has a clear owner.
4. Update `src/app/menu/README.md`, `docs/menu*.md`, `docs/current-state*.md`, and `docs/developer-extension-guide*.md` with the final entry points and extension procedure.

Exit gate: a fresh contributor can find a screen's state, actions, effects, view builder, and tests from the nearby README without tracing `MenuAppTail.inl` end to end.

Delivery note: static screen metadata moved to the exhaustive `MenuScreenDescriptor` table. Help content stays in `populate_help_overlay` because it depends on live screen state and belongs with its screen-specific presentation; it no longer needs to duplicate titles, background keys, or root/generic routing metadata.

## Verification for every phase

Run in risk-proportionate order:

1. New focused unit tests.
2. `bms_parser_tests.exe` and the registered Release CTest suite.
3. Release client build.
4. Static searches for removed flags, raw row-index branches in extracted screens, and callbacks in render DTOs.
5. Manual GUI smoke using a disposable profile:
   - open Options from Title, Song Select, and Multiplayer;
   - enter/leave each migrated child and confirm one-level Back behavior;
   - compare keyboard, click, and drag adjustments;
   - relaunch and confirm persistence;
   - check help text, menu music, skin backgrounds, and mouse hit targets.

Run ASan after Phase 3 and again after Phase 6. A passing unit suite does not replace the GUI smoke for D3D11 input/render behavior.

## Stop and rollback rules

- Stop a phase if it requires behavior changes outside its declared screen family.
- Do not remove legacy state until the replacement has focused tests and all reads have migrated.
- If a regression cannot be isolated within the current phase, restore that phase's touched-file archive instead of layering compatibility flags onto both designs.
- Do not proceed to the next family with a failing focused test, CTest failure, Release build failure, or unresolved GUI navigation regression.

## Definition of done

The full refactor delivery is complete after Phases 0-6: navigation history is explicit, every settings family uses typed screen-local controller state, boundary side effects remain visible in `MenuApp`, repeated static screen routing lives in one exhaustive descriptor table, and the nearby README identifies entry points and tests. Automated tests do not claim to cover the manual D3D11 GUI smoke boundary listed above.
