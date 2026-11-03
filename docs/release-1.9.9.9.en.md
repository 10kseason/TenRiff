# TenRiff 1.9.9.9 — Song Select, record feedback and Result video background

2026-11-03

- Use the supplied `continue_result_bga_ko_1080p60.mp4` as the default Result backdrop: silent 1920×1080, 60 fps, 60-second loop. Preserve the restored Result layout, metrics and typography. Low-opacity compositing keeps the authored background text behind the results. Respect custom backgrounds and reduced motion; release the decoder on leaving/minimizing.
- Song Select uses ten rows and existing EZ/NM/HD/EX CLEAR badges in place of row scores, with empty unplayed slots. Folder counts and selected MY BEST remain. Add BPM/keys/level/note count/judgement difficulty and real NPS MIN/AVG/MAX bars, plus the 12-second 1080p30 Song Select loop. Hide detailed accuracy only in the lobby.
- NPS counts original-chart TAP/LN heads in one-second bins including silence; AVG is arithmetic mean. Only the graph is reduced to at most 96 peak bars. A cancellable worker publishes idle completion even with audio preview disabled. Deterministic random branches can differ from actual play.
- Select main/clear/fail BGM and button/options/slider/start/drop/import-completion sounds in Audio (MP3/OGG/WAV/FLAC). Fix the audio file dialog apartment conflict. Options uses Main Menu 3; Clear uses former Main Menu 5; omit Main Menu 4.
- A score increase of at least one point displays NEW RECORD with a 1.2-second score effect. Keep clear-first official BEST storage; clear-only improvements are not score records. Include rank-up/FULL COMBO effects and continuous cumulative gauge-history display across automatic gauge drops.
- The optional movable HUD compares score with a compatible BEST replay at the same chart time. S/S+/S++/S+++ rows show accuracy differences (%p) against 95/97/98/99%. Missing compatible BEST stays unavailable; default OFF. Fix exclusion of valid paused local BEST and updates when only comparison data changes.
- Include folder counts/left-right navigation, the 10 emblem, hidden backend text, long record titles and 30-frame/one-second transitions. Result retains the layout restored before the HTML redesign.

[Release/downloads](https://github.com/10kseason/TenRiff/releases/tag/1.9.9.9) · [Validation](release-1.9.9.9-gate.md)

Extract into a new folder and run `launch_win.bat`. Keep existing installations, profiles and records. Existing scoring/judgement and R13/R14 replay/online contracts remain compatible. No website or database deployment. Fixed baseline: **1.9.6**. Physical input/listening, arbitrary skins/charts, long play and other PCs remain unverified.
