# Native menu presentation

The September 2026 local UI revision keeps the existing game flow and scoring while
making native Home, Song Select, single-player Result and shared settings easier to scan. It is the 1.7.0 UI release;
result-analysis expansion remains future work.

## Song Select difficulty-table control

The follow-up adds a difficulty-table card beside Search and Sort / Filter in the
center action strip. The main area shows the table name and opens a URL editor
over Song Select; File selects a local JSON, and Reset returns to native LV.
The library count remains in the library header. The existing Filters row remains
available and shares `MenuApp::handle_difficulty_table_input` with the new control.

`SongDifficultyTableAction` identifies EditUrl / LocalFile / Reset / Apply / Cancel.
`MenuWindow_draw_difficulty_table_editor.inl` owns the modal presentation and hit
regions. MenuApp blocks underlying clicks and key-repeat navigation while it is
open. Invalid imports leave the editor open with an error and retain the current
table; Enter applies and Escape cancels. Existing import/cache/index behavior is
reused. Friendly table names are loaded once per selected path/source refresh in
`MenuAppSongSelectRender.cpp`, outside the renderer, and reused in Filters.

Preview: `menu_visual_preview.exe --small` shows the new card, and `--table-editor`
starts with its URL editor open. Preview clicks exercise renderer targets only;
they never fetch the example URL or open a real file dialog.

Validation for this follow-up: Windows x64 Release build succeeded. The existing
suite passed 703 tests with 10 unrelated Windows/NK3 integration cases skipped.
Computer Use confirmed the card at 960x540 and a large window requested as 1080p,
URL editor open/cancel, reset to Native LV, and the File click target. Actual
remote import and file-picker persistence were not exercised with user data.

## Ownership and data flow

- `src/render/MenuWindow_draw.inl` selects the native treatment through
  `modern_title_screen` / `modern_library_screen` / `modern_settings_screen`, applies the native palette, and draws flat
  surfaces. Gameplay, multiplayer results and imported lobby skins retain
  their existing scene and skin paths.
- `src/render/MenuWindow_draw_title_native.inl` owns the native Home layout:
  profile header, quiet wordmark, wrapped guide, large primary action and three
  secondary actions. `MenuApp::populate_title_render_data` supplies localized
  button descriptions and retains the existing empty-library action. The native
  path has no horizon beam, spectrum animation or layered neon borders. Imported
  title skins keep `MenuWindow_draw_title_body.inl` and their original layout slots.
- `src/render/MenuWindow_draw_songselect_body.inl` owns selection emphasis,
  navigation width, compact visible-item count, and vertically centered actions.
  Native empty-list guidance wraps within the list. Jacket fallback artwork and
  the Selected badge require an actually selected row; an empty library shows
  Select a chart instead of a colored pseudo-jacket.
- `src/render/MenuWindow_draw_result_single_body.inl` owns song information,
  analysis panels, statistics and result actions. The native center panel is in
  `MenuWindow_draw_result_summary.inl`; the imported-skin prism remains separate.
- `src/render/MenuWindow.cpp` creates the cached score and metric text formats.
  No font resources are created per frame.
- `src/render/MenuWindow_draw_generic_body.inl` owns the native settings header,
  readable-width list, selection markers, aligned stepper buttons, and the skin
  preview. Without a skin preview, help occupies the right column; with a preview,
  help occupies the area below the left list. Light color swatches use dark labels.
- `MenuWindow_draw_generic_help.inl` measures and caches wrapped notes, including
  every footer note. Fixed-height pages align to whole text lines. Arrow controls
  emit `GenericHelpPage` with an absolute page index, consumed inside `MenuWindow`.
  This renderer-only state does not change the selected setting or persist config.
  Text/width/font changes rebuild the cached layout and reset to the first page.
  Home reuses it with compact paragraph spacing; long backend guidance remains
  readable through the same pager instead of being clipped into a narrow rail.
- `MenuApp` still supplies `MenuRenderData`. Score computation, replay evidence,
  result readiness and hit-target identifiers are unchanged. The same rectangles
  are used for drawing and hit registration. Temporary brush and text-alignment
  changes must be restored before other widgets draw.

The timing bars remain a Gaussian estimate from mean and standard deviation, not
an observed per-hit histogram. Their caption now explicitly says estimate and the
spread readout says standard deviation. Actual timing-distribution analysis is
future work.

## Verification

```powershell
cmake -S . -B build-ui-modern -G "Visual Studio 17 2022" -A x64 -DTENRIFF_ENABLE_TESTS=ON
cmake --build build-ui-modern --config Release --target tenriff menu_visual_preview bms_parser_tests nk3_onnx_smoke gameplay_judgement_benchmark --parallel 4
ctest --test-dir build-ui-modern -C Release --output-on-failure
```

`menu_visual_preview` is an explicit, excluded-from-default-build developer target
using the actual D3D11 menu renderer with synthetic data. It does not initialize
audio, accounts or profiles and does not submit or save records. Run from the
build's `Release` directory so the existing runtime DLLs are available:

```powershell
.\menu_visual_preview.exe
.\menu_visual_preview.exe --result
.\menu_visual_preview.exe --result --failed --1080p
.\menu_visual_preview.exe --empty
.\menu_visual_preview.exe --sources
.\menu_visual_preview.exe --records --english --performance
.\menu_visual_preview.exe --settings
.\menu_visual_preview.exe --skin-settings --1080p
.\menu_visual_preview.exe --title --english
.\menu_visual_preview.exe --title --empty --focus-options --performance
```

Default size is 1280x720. `--1080p` requests 1920x1080, `--reveal` exercises the
staged result entrance, and `--performance` enables the performance overlay.
Clicks print their renderer target kind and index; they do not run MenuApp actions.
Guide pagination is handled locally by the real renderer and works in the preview.
Close the preview with its window close button; it also exits after ten minutes.

Initial validation: Release build and CTest passed (3/3). The user supplied 1920x1080
screenshots of real song selection and a failed result, confirming the native
treatment renders. Those images exposed a clipped Session Mix tab and top-aligned
button labels; the follow-up widens native navigation and centers control labels.
Final shared-UI build: `build-ui-modern` Release succeeded and CTest passed 3/3.
Computer Use verified the actual renderer with synthetic fixtures:

- 1280x720 settings: readable rows, separate wrapped help, next/previous page
  changes with the same selected row.
- Large window requested with `--1080p` (Windows fits the decorated window to the
  desktop): skin preview, dark labels on light swatches, help-page advance, and
  a settings-button hit delivered as `SettingsRow`, index 0.
- 1280x720 Result: centered Continue/Replay/Retry labels and readable score/metrics.
- 1280x720 settings with performance overlay: help relocates below the overlay;
  list and footer remain unobscured.

These are renderer/interaction-target checks. Actual settings persistence, imported
skins, multiplayer, and the complete `ui-audit-checklist.md` matrix were not manually
retested. The earlier build path remained in use and produced LNK1104 when relinking;
the final binary was therefore built separately without replacing that running file.

Home follow-up: the user opted to check the screen directly and supplied a
1920x1080 screenshot of the new native Home with Play selected. The supplied Song
Select screenshot also confirms the widened Session Mix tab and centered controls.
Its empty state exposed a clipped help sentence and a Selected badge without a
selected chart; the follow-up wraps that sentence and removes the misleading badge
and pseudo-jacket. That final empty-state pixel change has not been manually
rechecked. No Computer Use was performed after the user chose direct verification.
