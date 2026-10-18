# Windowed drops and skin settings follow-up

The 2026-10-18 follow-up is included in 1.9.6. Earlier local candidates, published 1.9.5.3, installations and original profiles are preserved. See the [release notes](release-1.9.6.en.md) for current publication status.

- Retain file-drop acceptance when switching to windowed mode. Existing BMS file/folder handling and owned file-dialog lifetimes are unchanged.
- Show supported key modes as one horizontal tab row above Skin Settings, with a search box at the end. Search filters settings visible for the current source. Per-mode persistence is unchanged.
- Click a tab or use Ctrl+Tab / Ctrl+Shift+Tab to change modes. Ctrl+F focuses search; Tab / Shift+Tab cycles between tabs, search and the settings list. Search matches labels, values and categories in the current language.
- Hide Gear Boundary, Judgement X, Combo X, FAST/SLOW X and indicator X controls without clearing stored values.
- Show Create Skin, Open Skin Folder and Reload only for TenRiff; show Imported Skin selection only for TenRiff or LR2.
- Preserve Original-mode scratch-position visibility and the hidden 100% divider-width policy.
- Reduce the default album-art judgement badge from 288×36 to 192×28 design pixels. Explicit skin-authored rectangles still take precedence.
- When combo text overlaps optional judgement counts, move it to the nearest side that fits inside the field. It can now cross above the indicator instead of always being pushed down. Classic/Studio and player/ghost share this rule; stored positions are unchanged.

The [fixed judgement policy](judgement-baseline-20261018.en.md), R1–R8 replay compatibility and peer protocol 7 remain. With compatible Sites support, 1.9.6 permits new standard Online R9/R10 submissions and preserves old boards as read-only. The general source baseline remains 1.9.0.

Final verification is recorded in `evidence/VERIFICATION.ko.md` under the 1.9.6 release root. Earlier candidate `evidence-ui-20261018/VERIFICATION.ko.md` remains historical evidence. Automated HWND messages and renderer captures are distinct from human Explorer dragging, physical IME/input and extended play.
