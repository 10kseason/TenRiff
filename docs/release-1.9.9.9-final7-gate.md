# Final7 release verification

The release workflow checks the Windows Release build, the independently extracted source ZIP, CTest, the native application smoke suite and the offline skin editor. Publication checks cover the PR and merged commit, all outgoing files, ZIP CRCs, SHA-256 sums and downloaded GitHub assets.

Validation covers the display time of automatic and press-retired misses in the engine and through the session HUD snapshot, with scoring samples, gauge history and legacy ordering held unchanged. Everything else is carried over from final6.

The live play screen, physical input, listening, extended play and arbitrary skins or codecs require hands-on verification.
