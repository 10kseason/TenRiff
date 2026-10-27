> **1.9.9.3:** Includes accelerated controls, LR2CRS courses, BMS/KPS overlay, clear cards, grade colors and EX rainbow fixes. General fixed baseline: **1.9.6**. Older release and local-candidate entries below are historical; this scope supersedes them. [Release](release-1.9.9.3.en.md).

> Historical implementation below. Live input difficulty and HUD computation are superseded by this follow-up: [Fixed BMS estimate, KPS worker and gauge colors](bms-overlay-20261027.en.md).

# Key overlay and larger logo · local follow-up

Fresh position defaults to X 0%, Y 25%, outside the default Classic/Studio lanes, gauge and score panels.

This unpublished candidate continues 1.9.9.1 plus the local tuning controls and Session Mix work. The general pinned baseline remains 1.9.6; prior releases are preserved.

Enable **Settings → Skin → HUD → Key Overlay Plugin (4/5/7/8/10K)**, or search for `overlay`. Default OFF. **Key Overlay X/Y** move the complete panel inside the canvas in 1% steps from 0–100%. Enablement and position persist in global settings, profiles and portable skin presets. Skin preview uses synthetic key trails marked Preview, with no fabricated estimate.

Only runtime **4/5/7/8/10K** displays the plugin. Other modes hide it without deleting the settings. Current key bindings, lane colors and scratch presentation order are used. It shows key presses/holds, recent 1-second NPS and an 8-second input-pattern estimate using the v2.13 core/calibration from [key-note-overlay-cpp](https://github.com/10kseason/key-note-overlay-cpp): 4K Reform, 5K AERY, 7K Jinjin, 8K Regular and 10K Revive. Original rank names/units remain. A retained estimate has `*`; arrows indicate values outside the calibration range. These are input-load estimates, not whole-chart ratings, skill measurements or pass predictions.

Only accepted live logical lane transitions are observed. A secondary-key takeover on an already held lane stays one visual/estimated hold. Pause, countdown, focus loss and completion clear history; autoplay, replay and ghost inputs are excluded. Judgement, scoring, audio and replay contracts remain unchanged. A bounded single-producer/consumer queue keeps allocation and analysis outside the audio/judgement callback; the HUD estimates at most 10 times per second. Lost/stale edges clear the sample window instead of leaving stuck holds.

Default native home/library/settings/result marks and wordmarks are larger. Original image files and authored layout slots remain. This is an in-game plugin, with no separate process, global hook, startup service or network. Upstream tray/report/osu! memory features are out of scope. MIT license and per-file provenance are bundled in `third_party/key_note_overlay/`. See the candidate's `evidence/VERIFICATION.ko.md`; physical input, OBS capture and extended play require separate verification.
