# TenRiff 1.8.2 — option colors and timing feedback

2026-10-03. This patch keeps the black menu surfaces from 1.8.1 and updates visual feedback.

## Changes and rationale

- Ten Options icons use distinct saturated colors at full opacity. Selected borders inherit the icon color; corners increase from 7px to 12px and borders from 1px/1.35px to 1.5px/2.25px. Native skin slots remain editable. Studio PNGs share the same defaults; imported images keep their pixels.
- The gameplay and ghost timing bars now share the FAST/SLOW text visibility gate. PG, centered timing, expired feedback and disabled feedback hide the bar even when timing history remains. Scoring and timing history are unchanged.
- The offline and web editor preview the actual Options card layout and shared vector geometry. Colors and border controls show their effects immediately. Native menu previews use the black client palette, and the gameplay preview follows the same FAST/SLOW visibility rule.

See [colors and skin controls](options-icon-colors.ko.md).

## Validation

The local visual fix passed Release CTest 3/3 (915 cases, no integration skips), 40 icon color checks, four selected-border checks and 40 option hit regions across native, Studio PNG, Japanese 1366px and explicit-color fixtures. Seven gameplay fixtures cover PG, FAST, SLOW, expired/disabled feedback and ghost PG/FAST. Pre-fix captures reproduced the unwanted timing bar.

The release workflow rebuilds the versioned client, runs the editor regression suite, independently builds and tests extracted source, checks PR/main AddressSanitizer and OpenVINO CI, verifies package privacy and hashes, matches final source to merged Git blobs, and downloads uploaded assets. Final outcomes are recorded in the release notes and external evidence rather than assumed here.

## Limits

Browser previews approximate the native renderer. Physical keyboard rollover, real OS input dispatch, multi-PC play and long gameplay sessions remain unverified. Packages exclude accounts, connection secrets, user profiles, song libraries, results and diagnostic evidence. Previous releases and installed clients are preserved.
