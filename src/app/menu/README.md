# Menu subsystem

This directory owns the typed navigation and extracted screen-controller layer used by `MenuApp`.
It deliberately does not own platform input, config persistence, thread restarts, or mutable render
state. Those effects stay at the `MenuApp` application boundary.

## Entry points

- `MenuScreen.h` defines the stable screen identifiers shared by navigation and rendering.
- `MenuScreenDescriptor.h/.cpp` is the exhaustive table for stable titles, skin background keys and
  fallbacks, snapshot/generic-view routing, Options-family music selection, and input-footer policy.
- `MenuNavigator.h/.cpp` is the sole owner of the active screen and nested Back history.
- `OptionsHubController.h/.cpp` owns the typed 4-by-2 Options cursor and card destinations.
- `MenuAction.h` defines normalized actions and the effect flags returned by controllers.
- `settings/*SettingsController.h/.cpp` owns typed selection, edit/dirty state, and pure config
  mutation for Audio, Input, Calibration, Graphics/ONNX, Keymap/NKRO, Skin, and Mode/Mods.
- The Audio, Input, Calibration, Graphics, and Keymap view builders produce localized value-only rows.
  Skin keeps specialized preview assembly in `MenuAppSkin.cpp`; Mode rendering remains a thin adapter
  in `MenuAppSettings.cpp`.
- `MenuApp.cpp` translates raw keyboard and pointer input, applies navigation, and prepares screens.
- `MenuAppDeviceSettings.cpp`, `MenuAppKeymap.cpp`, `MenuAppSettings.cpp`, and `MenuAppSkin.cpp`
  execute explicit platform/persistence/restart/file-dialog/reindex effects and copy view models into
  `render::MenuRenderData`.

## Data and control flow

1. The input thread delivers a key or pointer event to `MenuApp`.
2. `MenuApp` normalizes that event to a `MenuAction` and, for pointer input, a stable row identifier.
3. The screen controller mutates only its local state and the supplied runtime config, then returns
   `MenuEffectFlags`.
4. `MenuApp` performs requested boundary effects such as config persistence, audio restart, or one
   navigation pop.
5. The view builder produces value-only rows. `MenuApp` copies them into the immutable render
   snapshot consumed by the render thread.

There are no callbacks or references to mutable app state in either settings view models or render
DTOs.

## Navigation rules

- Use `reset(screen)` for a new root flow. It clears all old history.
- Use `push(screen)` for a nested menu or confirmation that Back must leave one level at a time.
- Use `replace(screen)` only when the current step changes without adding a Back destination, such as
  Gameplay progressing to its Result.
- Use `back()` for nested exits. A false return means the caller is already at a root and must choose
  an explicit safe fallback.

Title, Song Select, and Multiplayer can each push Options. Options cards then push their child screen,
so leaving Audio returns to Options first and leaving Options returns to the actual origin.

## Extending a settings screen

1. Add a stable screen-local row enum. Preserve existing numeric values used by renderer hit data;
   append instead of reordering where compatibility matters.
2. Put bounds and step sizes in one typed row specification used by both keyboard and pointer paths.
3. Keep config mutation and local cursor/dirty state in a pure controller that returns effect flags.
4. Keep localization and value-only row construction in a view builder.
5. Keep persistence, filesystem access, platform APIs, and thread restarts in `MenuApp`.
6. Add focused tests for every row, boundaries, no-ops, Back effects, and keyboard/pointer parity.
7. For a new screen, add its static metadata and root/generic routing to `MenuScreenDescriptor.cpp`;
   keep dynamic title/help content with the screen's view code.
8. Add the source and test files to `CMakeLists.txt`, then run the verification commands below.

See `docs/menu-refactor-plan.md` for the completed phase gates, decisions, and rollback rules.

## Verification

From the repository root on a configured Windows build machine:

```powershell
cmake --build build --config Release --target bms_parser_tests tenriff --parallel
build\Release\bms_parser_tests.exe
ctest --test-dir build -C Release --output-on-failure
```

After an extracted controller slice, run the focused sanitizer target as well:

```powershell
cmake -S . -B build-asan -A x64 -DTENRIFF_ENABLE_ASAN=ON -DTENRIFF_ENABLE_TESTS=ON
cmake --build build-asan --config RelWithDebInfo --target bms_parser_tests --parallel
ctest --test-dir build-asan -C RelWithDebInfo -R "^bms_parser_tests_asan$" --output-on-failure
```

Also search extracted screens for legacy numeric behavior branches and perform a GUI smoke of keyboard,
click, drag, persistence, Back order, menu music, and background selection. Unit tests do not prove the
D3D11 pointer/render path.

## Known boundaries

Session Mix and the Options card grid still have substantial screen-specific render assembly in
`MenuAppTail.inl`; they do not share settings row mutation state. Skin preview rendering intentionally
stays at the application boundary because it combines filesystem-derived assets and live preview
geometry. `KeymapConfirm` remains a thin compatibility screen; keep its navigation contract covered
until a product flow either connects or removes it. Manual D3D11 keyboard/pointer/render smoke remains
required before publishing a release candidate.
