# TenRiff 1.9.9.9-hotfix4 validation

- Build Windows x64 Release and run CTest, native menu smoke and offline editor tests. Build and test the standalone source ZIP, and verify PR/main sanitizer and NK3 CPU CI.
- Exercise Safe/Fast scans, compact and large caches, unknown nested JSON fields, escaped strings, oversized historical entries, late malformed entries, worker exceptions and ordered results. Check ALL SONG overlapping sources, missing charts and cancellation.
- Compare repeated cached loads and menu folder opens on synthetic fixtures. Retain metadata parity and measure available CPU threads. Actual storage and chart mix affect timings.
- Check successful-hit animation and PG/GR/GOOD head visibility, failed-note fall-through, 3 burst sizes, 6 procedural styles, 5 palettes, PNG/JPEG selection and Type 2 solid bands.
- Verify both ZIPs for CRC, file hashes, dependency licenses, defaults and source equivalence to the release commit. Freshly review outgoing files and notes. Read back all three release assets after upload and publication, and compare prior release/tag/asset snapshots.

The existing R13/R14 contracts and fixed baseline 1.9.6 apply. Validation uses synthetic charts and the build PC. Physical play feel, long sessions and other PCs require separate testing.
