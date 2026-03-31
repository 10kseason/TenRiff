# TenRiff Developer Extension Guide
Language: [한국어](developer-extension-guide.md) | English | [简体中文](developer-extension-guide.zh-CN.md) | [日本語](developer-extension-guide.ja.md)

This guide explains where to change the code when adding a new `mode/mod`, or when extending the existing mode pipeline. It is intentionally code-first: use it as a maintenance map for the runtime, UI, migration, replay/result, and test layers.

## Responsibility Map

- `src/gameplay/ModeSettings.h` / `src/gameplay/ModeSettings.cpp`
  - Basic enums such as `ChartFormatMode`, `KeyMode`, `GaugeMode`, and `RandomMode`, plus string parsers and formatters.
- `src/app/ModeResolver.h` / `src/app/ModeResolver.cpp`
  - Converts `config::ModeConfig` into runtime `gameplay::ModeSettings` and collects warnings for invalid tokens.
- `src/app/ModeManager.h` / `src/app/ModeManager.cpp`
  - Owns the mod registry, categories, score multipliers, judge-window scaling, and chart transformations.
- `src/gameplay/ModeApplier.h` / `src/gameplay/ModeApplier.cpp`
  - Applies key-mode conversion and random-style transformations to the actual `GameplayChart`.
- `src/app/MenuAppSettings.cpp`, `src/app/MenuAppTail.inl`, `src/app/MenuAppSettingsUtils.h`
  - Menu UI, input handling, and help copy for `Mode Settings`, `Mod Manager`, `Key Mode`, and related rows.
- `src/config/Config.h` / `src/config/Config.cpp`
  - Load/save schema for `config/config.json` and per-profile settings.
- `src/app/RuntimeConfigMigration.cpp`
  - Migrates legacy defaults and old tokens to the current runtime model.
- `src/app/PersistedRuntimeConfig.cpp`
  - Strips session-only modes before persisting runtime config.
- `src/gameplay/Replay.cpp`, `src/gameplay/Replay.h`, `src/app/MenuRecordUtils.cpp`, `src/app/GameSession.cpp`, `src/app/MenuAppTail.inl`
  - Replay/result saving, loading, and result-screen presentation.

## Add A New Mode

The usual sequence for a new mode is:

1. Add the enum or token definition in `src/gameplay/ModeSettings.h`.
2. Update `to_string(...)` and `parse_...(...)` in `src/gameplay/ModeSettings.cpp`.
3. If the token can appear in config files, handle it in `src/app/ModeResolver.cpp` and emit warnings for bad values.
4. If the mode changes chart structure, implement the actual transformation in `src/app/ModeManager.cpp` or `src/gameplay/ModeApplier.cpp`.
5. If users should edit it in the menu, add the row and input logic in `src/app/MenuAppSettings.cpp`.
6. If persistence or migration is involved, update `src/config/Config.cpp`, `src/app/RuntimeConfigMigration.cpp`, and `src/app/PersistedRuntimeConfig.cpp`.
7. Add unit/smoke coverage and sync the docs when behavior becomes user-visible.

## Add A New Key Mode

Key modes are not just tokens. They affect input mapping, chart transformation, replay metadata, and menu editing.

- `src/gameplay/ModeSettings.h` / `src/gameplay/ModeSettings.cpp`
  - Add the new `KeyMode` enum value and keep `parse_key_mode(...)` / `to_string(...)` in sync.
- `src/gameplay/ModeApplier.cpp`
  - Update `target_lane_count(...)` and the actual lane-conversion logic so the new lane count is handled correctly.
  - Verify that note timing and hold metadata survive the remap.
- `src/app/ModeManager.cpp`
  - Update helper logic such as `target_lane_count(...)` and `key_mode_for_lane_count(...)`.
- `src/app/MenuAppSettingsUtils.h`
  - Check `normalize_runtime_key_mode(...)`, `cycle_runtime_key_mode(...)`, and the key-mode label helpers.
- `src/app/MenuAppSettings.cpp`
  - Update the `Key Mode` row label and the left/right cycling behavior.
- `src/app/MenuApp.cpp`
  - Make sure the keymap editor, current chart lane count, and runtime lane-binding paths follow the new mode.
- `src/app/GameSession.cpp`
  - Ensure the selected lane count is reflected in replay/result metadata.
- `src/app/PersistedRuntimeConfig.cpp`
  - Confirm that the mode is not accidentally stripped from persistence unless it is truly session-only.

The most common mistake is changing one alias path but forgetting `none`, `auto`, uppercase/lowercase tokens, or the menu-side labels. All of those layers should describe the same mode meaning.

## Add A New Gauge Mode

Gauge changes are not isolated to `ModeSettings`; they also touch labels, migration, and result presentation.

- `src/gameplay/ModeSettings.h` / `src/gameplay/ModeSettings.cpp`
  - Extend `GaugeMode` and keep the string conversion functions aligned.
- `src/app/MenuApp.cpp`
  - Update helpers such as `gauge_type_from_mode_string(...)` and the visible gauge labels.
- `src/app/MenuAppSettings.cpp`
  - Adjust the `Gauge` row cycling order.
- `src/app/ModeManager.cpp`
  - Make sure `scale_judge_windows(...)` still matches the intended gauge policy.
- `src/app/RuntimeConfigMigration.cpp`
  - If default values changed, migrate only the exact old shipped defaults to the new ones.

Gauge values are visible on the result screen and in replay/result metadata, so update `tests/unit/test_replay_export.cpp` and the result-screen path if you change labels or serialization behavior.

## Add A New Random Mode

Random modes live in `ModeSettings`, but the actual behavior belongs to `ModeApplier`.

- `src/gameplay/ModeSettings.h` / `src/gameplay/ModeSettings.cpp`
  - Add the new random token and parser support.
- `src/gameplay/ModeApplier.cpp`
  - Add the transformation branch, similar to `apply_full_random(...)` or `apply_super_random(...)`.
  - Be explicit about whether the random step runs before or after key-mode conversion.
- `src/app/ModeResolver.cpp`
  - Add warning and fallback behavior for invalid tokens.
- `src/app/MenuAppSettings.cpp`
  - Update the `Random` row label and cycling behavior.

Random behavior must be deterministic for a fixed seed. Whenever you add a new random mode, add a determinism test in `tests/unit/test_mode_applier.cpp`.

## Add A New Mod

Most mods belong in the `ModeManager` registry. The important part is to keep token, category, multiplier, transformation, and persistence policy aligned.

- `src/app/ModeManager.cpp`
  - Add a new `ModeModDescriptor` entry.
  - Decide the `category_token`, `category_label`, and `score_multiplier` together.
  - If the mod changes chart structure, add the transformation helper and call it from `manage_modes(...)`.
- `src/app/ModeManager.h`
  - Declare any helper you need to expose to the rest of the app.
- `src/app/MenuAppSettings.cpp`
  - `populate_mode_mods_render_data(...)` is registry-driven, so most new mods appear automatically. Add help copy only if the new category needs explanation.
- `src/app/PersistedRuntimeConfig.cpp`
  - If the mod is session-only, strip it before saving runtime config.
- `src/app/MenuAppTail.inl`
  - Verify that the result screen still renders the `Mods:` summary and multiplier text correctly.

If a mod affects scoring, always re-check `rate_score_multiplier(...)`, `mod_score_multiplier(...)`, and `final_score_multiplier(...)`. The current system uses the lower of the rate multiplier and the mod multiplier for final score.

## Config, Migration, And Save Policy

When you add a setting, check three places:

- `src/config/Config.cpp`
  - Load: read the JSON token and preserve defaults.
  - Save: write back the normalized token values.
- `src/app/RuntimeConfigMigration.cpp`
  - Only upgrade configs that still exactly match an old shipped default.
  - Keep the matching rules narrow so custom user configs are not overwritten.
- `src/app/PersistedRuntimeConfig.cpp`
  - Remove session-only modes from the persisted copy only.

A common mistake is to update the menu cycling code but forget persistence or migration. In that case the option appears to work once and then reverts on the next launch.

## Replay, Result, And Records Impact

Mode changes must be reflected in saved files and in the records/result UI.

- `src/gameplay/Replay.cpp` / `src/gameplay/Replay.h`
  - Keep replay/result JSON fields such as `mode`, `raw_score`, `final_score`, `rate_multiplier`, and `score_multiplier` consistent.
- `src/app/GameSession.cpp`
  - Apply the final `ModeManager` output when the session ends.
- `src/app/MenuRecordUtils.cpp`
  - Parse saved result and replay files for the `Records` view and result detail panel.
- `src/app/MenuAppTail.inl`
  - Render the result screen copy, score multipliers, mod summary, and replay/result path labels.

If you change replay or result serialization, update `tests/unit/test_replay_export.cpp` immediately. Those files are often consumed by other code paths, so field names and defaults should stay stable unless you are intentionally migrating them.

## Tests And Docs Sync

New mode work is easy to break if you skip tests.

- `tests/unit/test_mode_applier.cpp`
  - Key-mode conversion, random determinism, and hold metadata preservation.
- `tests/unit/test_mode_manager.cpp`
  - Mod registry behavior, category conflicts, judge scaling, and score multipliers.
- `tests/unit/test_config.cpp`
  - Save/load round-trips, case normalization, and default migration.
- `tests/unit/test_replay_export.cpp`
  - Replay/result JSON fields and restoration behavior.
- `tests/smoke/bms_mode_smoke.cpp`
  - Real-chart combinations of key mode, random, and mods.
- `tests/smoke/n2nc_compare_smoke.cpp`
  - Key-mode remap behavior against known conversion cases.

For docs, think in this order: `docs/current-state.md`, `docs/config.md`, `docs/README.md`, then any feature-specific docs. This turn only adds the developer guide, so keep code-facing behavior changes and current-state docs in a later pass if they are needed.

If the work also refreshes the public source package, go one step further. After restaging `opensource-Tenriff-source/TenRiff-<version>-source`, verify raw `cmake` configure/build plus at least a `bms_parser_tests` run inside that staged folder, assuming repo-only helpers such as `tools/`, `10k-calc/`, and existing `profiles/` are absent there.

## Common Mistakes

- Adding a token in `ModeSettings` but forgetting `ModeResolver` and the menu UI.
- Adding a mod to the registry but leaving `score_multiplier` or category metadata incomplete.
- Forgetting to strip session-only mods in `PersistedRuntimeConfig.cpp`.
- Migrating all configs instead of only the exact legacy defaults that should be upgraded.
- Changing replay/result fields without updating `MenuRecordUtils.cpp` and `tests/unit/test_replay_export.cpp`.
- Treating `none`, `auto`, and case variants as different semantic values when they are meant to be aliases.
- Adding a new key mode but forgetting the keymap, replay, and result lane-count paths.

## Verification Checklist

- `ModeSettings` enum/parse/to_string round-trip works.
- `ModeResolver` warns on invalid tokens and falls back to safe defaults.
- `ModeManager` registers the new mod with the correct category and multiplier.
- `ModeApplier` applies the new key mode and random behavior deterministically.
- `MenuApp` exposes the new row in `Mode Settings` or `Mod Manager` when needed.
- `Config` save/load and `RuntimeConfigMigration` do not break old user configs.
- Replay/result JSON includes the new mode information.
- `tests/unit` and `tests/smoke` still pass.
- If behavior is user-visible, update `docs/current-state.md` and `docs/config.md` in a separate docs pass.
- If the public source package was refreshed, verify standalone `cmake` configure/build and a `bms_parser_tests` run from `opensource-Tenriff-source/TenRiff-<version>-source` itself.
