# Settings UI and per-play seed follow-up

2026-10-19 · [1.9.6](baseline-1.9.6.en.md)

- Keymap adds Common Keys and tabs for its existing ten layouts. Common Keys edits instant restart and eleven function actions, sharing Profile storage, conflict validation and immediate saving. Tab moves between tabs and bindings; Left/Right selects a tab.

- The fourteen skin tabs increase from 52 to 60 design pixels high and text from 18 to 24. Search width changes from 340 to 260 to retain one row. Mode colors reuse the Song Select palette; 7+1 uses the eight-key color.

- Skin labels color FAST blue and SLOW red, preserving contours, fitting, localization and preview. Percentage size, position, opacity and brightness controls use 1% increments. Pixel/millisecond units and stored fractional values are not migrated.

- Key Mode Settings becomes Game Mode Settings; the default Judge Window label is Normal in every language. Timing values, R9/R10, scores and pinned grades remain unchanged.

- Every new solo play and Retry generates a fresh seed in OFF, Mirror, Random, scratch-fixed Random, S-Random and R-Random. Manual seed editing becomes a read-only automatic label. BMS RANDOM uses an independent seed. Different seeds can still produce the same layout by chance.

- Replay playback/verification uses recorded seeds. Shared-chart ghosts retain seed matching when the seed affects layout; seed-independent Original OFF/Mirror does not reject a ghost merely for a new seed. Peer random-OFF shared rules and peer v7 remain unchanged.

- These changes are included in 1.9.7. Preserve prior releases, installations and profiles; the general source baseline remains 1.9.6. Physical input/listening, long sessions and other PCs require separate validation.

- Increase the reference gain for TenRiff scene music (title, menus and results) from 10% to 12.5%, reducing attenuation by about 1.94 dB. Stored master/menu sliders and song preview/gameplay gains remain unchanged.
