# TenRiff 1.9.9.9-hotfix3 validation

- Build Windows x64 Release and run CTest, the native menu smoke suite and offline skin editor tests. Repeat the build and tests from the standalone source ZIP. Run the existing sanitizer and NK3 CPU CI jobs on the PR and merged commit.
- Cover NK2 E+ 7K/8K/10K, preset restoration, equal-key remix, exact LN-tail timing, source fallback, historical v1 playback and R13/R14 replay verification. New 7K layouts and layouts that use repaired tail accents use E+ v2. Compatible 8K/10K layouts keep their v1 identity. The existing Sites v24 contract keeps the two versions on separate boards.
- Check Safe indexing fingerprints, timeline-derived metadata and cache parity. The performance figures in the release notes come from a synthetic 12,000-chart library. Storage and chart mix affect actual results.
- Open the Windows folder picker from Song Select and the configured shortcut before visiting Options. Check cancellation and restored menu input. Check the enlarged button's hit region alongside the Library controls.
- Check the official Sites origin, public R13/R14 metadata lookup, and offline payload acceptance for both E+ board identities. The production check reads public records. Authenticated submissions and other PCs require separate verification.
- Verify source ZIP entries against the release commit and both ZIPs for CRC, file hashes, defaults and licenses. Read back all three GitHub assets before and after publication. Keep existing release assets and tags intact.

Replay format and R13/R14 scoring rules follow the existing contract. The fixed baseline is 1.9.6. Release validation covers synthetic charts and the build environment. Windows Defender behavior varies with the host and detection definitions.
