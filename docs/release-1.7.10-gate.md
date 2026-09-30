# TenRiff 1.7.10 — ALL SONG loading

2026-10-01. This patch hardens the combined song library and its completion state.

## Changes and rationale

- Skip chart paths that are no longer regular files, including deleted subfolders and paths replaced by directories. Continue processing later charts and sources; preserve per-folder caches instead of forcing a rescan.
- Report cached-chart merge progress every 64 entries and check cancellation after progress callbacks. Cancellation keeps existing caches and does not publish a partial aggregate.
- Publish worker completion state under the same result lock as the final result. Previously, a consumer could receive the result while the worker still reported loading and publish a stale loading snapshot.
- Missing source roots already allowed later sources to load. Regression coverage now checks a missing root at the beginning, middle and end for cold scans, cached loads and forced refreshes.

## Verification scope

- MSVC x64 Release build and CTest: 3/3; 859 internal cases, no integration skips.
- Deleted chart/subfolder and directory replacement regressions; cancellation and cache-byte preservation checks.
- 5,000 repeated result-publication checks; the completion-state regression failed before the fix.
- The actual MenuApp path converts aggregate results into visible songs and song-card render data. Representative existing cache copies produced 2,958 unique and visible charts, including with a missing source inserted in the middle.
- The runnable client and public source are packaged separately. Extracted archives are checked for CRC, file hashes, clean defaults, personal paths, credentials and excluded runtime data. The extracted source is built and tested independently before publication.

The reported **empty list after loading** was not reproduced with the inspected caches. The two confirmed defects are fixed, but this does not establish that the original native-window symptom is resolved. Song-card checks inspect render data; native pixels, mouse interaction and long gameplay sessions remain unverified in this patch.

## Public package boundary

The release contains the client ZIP, source ZIP and SHA-256 checksums. Profiles, account connections, local cache copies, song libraries, diagnostic harnesses, logs, replay/result exports, personal images, private paths, credentials and private checkpoints are excluded. The declared NK3 models and third-party runtime dependencies remain part of the public baseline. See [source privacy](open-source-privacy.ko.md) and `SOURCE_PACKAGE_SCOPE.txt`.

Extract the client into a new folder and run `launch_win.bat`. Preserve the prior installation while checking the new version with your library.
