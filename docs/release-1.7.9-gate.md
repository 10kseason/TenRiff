# TenRiff 1.7.9 Release Gate

## Scope

Windows x64 client and public source with clearer menus, profile controls and an ALL SONG library. The existing 4K–16K Luma Keys skin and web/offline editor remain included. This release does not change gameplay judgement, audio timing or multiplayer protocol v6.

## Changes

- Profile and first-run Quick Setup offer language selection and Normal / Large / Extra Large menu text. Settings save immediately per profile.
- Profile photos appear in the editor and title screen. Reselecting a modified image at the same path refreshes its preview; decoded preview pixels do not keep the original file locked.
- Sources > ALL SONG combines registered folders, reuses available caches and scans missing caches sequentially. Overlapping chart paths are deduplicated; separate copies remain separate entries. F5 rescans every registered folder.
- Option colors, outlined panels and digital navigation tabs make screen sections easier to distinguish. TENRIFF uses subtle hologram motion beside a static TI mark; Reduced Motion stops animation.
- Chart-stat values share a text scale and baseline. Maximum combo includes the COMBO unit. Mode labels have distinct colors from 4K through 16K.
- Note height is 50–400% in the client, manifest schema and web/offline editor. Values above 200% now display correctly. Existing LR2 image aspect ratios are retained.

See [usage and implementation notes](menu-profile-library-polish.ko.md) and the [web editor](https://tenriff-skin-editor.lastestarcorp.chatgpt.site/).

## Validation scope

- Windows Release build and all three CTest entries; the unit runner contains 854 cases.
- Editor suite: 22 tests, including note-height bounds and catalog/schema agreement.
- Synthetic actual-renderer captures check the three text sizes, profile/title photos, language setup, ALL SONG, mode colors, result combo and note-height endpoints. A same-path photo change and Reduced Motion were checked separately.
- Release completion requires ZIP CRC, extracted-file hashes and privacy/inventory checks for both archives, a standalone build/test from the extracted source, and SHA-256 readback of uploaded release assets.

The synthetic checks do not establish hardware input/audio latency, large real-library scan performance, long-session FPS or multi-PC compatibility. Native file-picker clicks were not automated. Very long result titles can still shrink, and 400% preview notes can reduce contrast behind judgement text. ASIO device support remains subject to the limitations in [the ASIO guide](asio-audio.md); WASAPI is the default.

## Privacy boundary

Packages exclude personal profiles, account connection files, upload credentials, replays/results, local logs, private model checkpoints and workspace agent instructions. Documentation images use synthetic charts and a generic PLAYER profile. Public client protocol and validation code remain included; ranking-server implementation and operational secrets are excluded. No real account upload is used as a release check.
