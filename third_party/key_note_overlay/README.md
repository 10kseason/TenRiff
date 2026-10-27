# Key Note Overlay core

Source: https://github.com/10kseason/key-note-overlay-cpp (README v2.13, retrieved 2026-10-27).
All ten source files and LICENSE are byte-identical to the Git blobs in provenance.json.
Only the estimator/strain/rank core and calibration tables are used. No external keyboard hook or process integration is included.

TenRiff adapter: src/app/KeyNoteOverlay.{h,cpp}; renderer: src/render/MenuWindow_draw_key_overlay.inl.
The adapter accepts TenRiff logical lane transitions into a bounded queue. A separate below-normal-priority worker maintains keys and rolling one-second KPS; HUD copies never wait. Difficulty is computed once from the full parsed BMS during loading (the peak of 8-second windows evaluated every 0.5 seconds), then displayed unchanged during that play. Neither the audio callback nor the playing HUD evaluates difficulty. Display remains limited to 4/5/7/8/10K and defaults OFF.

Tests: bms_parser_tests --filter="key overlay" covers the worker, bounded event loss, short taps, holds, resets, configuration, accepted score/replay parity and synthetic preview boundaries. --filter="BMS overlay" covers every supported profile, full-chart peak/lead-in behavior, LN, invalid/cancelled input and real BMS parsing through session HUD pause/autoplay. The upstream core and calibration tables remain unchanged; the adapter does not turn the estimate into an official chart level or player-skill result.
