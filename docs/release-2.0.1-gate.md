# TenRiff 2.0.1 verification gate

Scope: fix Options readability on the 2.0.0 source baseline. Keep the final4 compatibility reference.

- Default Options/settings galaxy RGB brightness: 0.98 to 0.392 (40% of 2.0.0); source image and alpha retained. Fallback rendering also dims the background.
- Eight cards: 85% width and height about their original centers; text, icons and matching click regions scale together.
- Restore the existing localized description as one non-wrapping line below the selected card grid.
- Local MSVC Release build; CTest 4/4, 1593 cases, zero integration skips; native smoke 774/774.
- Twelve actual renderer captures: all eight selected categories in Korean, English/Japanese/Simplified Chinese at large text size and 960×540, plus Korean at 2560×1440. Geometry checks 96/96. A background-only crop measures 39.94% of the previous capture brightness; card bounds measure 85% on both axes.
- An initial concurrent executable post-build asset copy failed. Serial executable builds resolve the copy race; final binaries are checked before packaging.
- No gameplay, judgement, scoring, replay, protocol, model-weight, configuration-default or custom-background changes.

Publication checks include outgoing file/privacy review, exact Git/source-ZIP and binary/archive matching, checksums, CI, and public asset read-back. Final publication evidence is recorded after completion; historical release gates do not substitute for these checks.

The preview could not acquire OS foreground, so geometry resolution is verified but physical/native mouse dispatch is not claimed. Physical input, listening, other PCs, arbitrary skins/codecs and long play remain unverified. No production account/score/database write or website deployment is included.
