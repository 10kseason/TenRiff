> **1.9.9.3:** Includes accelerated controls, LR2CRS courses, BMS/KPS overlay, clear cards, grade colors and EX rainbow fixes. General fixed baseline: **1.9.6**. Older release and local-candidate entries below are historical; this scope supersedes them. [Release](release-1.9.9.3.en.md).

> **Superseded color policy:** [Rank/shift colors and EX rainbow correction](rank-colors-20261027.en.md).

# Fixed BMS estimate, KPS worker and gauge colors

Local follow-up to 1.9.9.1, including the previous controls, Session Mix, enlarged logo and clear cards. Fixed baseline 1.9.6 and existing releases/installations/profiles remain intact.

Enable **Skin → Judgement / HUD → Key Overlay Plugin**. Default Off; saved toggle and X/Y positions remain. Only 4/5/7/8/10K display it; no separate installation is needed. A large **BMS chart estimate** is computed once during loading and stays fixed throughout play, independently of input, judgement and autoplay. Analysis uses the whole resolved BMS at the selected mode/rate/layout, before practice trimming/countdown; different new-play settings may change it.

This replaces live input difficulty. The unchanged v2.13 core scans 8-second windows with 4-second warmup context every 0.5 seconds and selects the peak. Tap/LN timing is read, not media or keyboard activity. Units remain 4K Reform, 5K AERY, 7K Jinjin, 8K Regular and 10K Revive. Arrows indicate extrapolation. It estimates chart-section load, not an official level, player skill or pass probability. Sparse/invalid/overlapping charts or limits (250,000 notes, one hour, core event capacity) show **Not rated**, never a truncated-chart rating. Skin preview remains synthetic with **Preview**.

**KPS** counts accepted logical lane presses in the last second, excluding repeats, held-lane secondary takeover, autoplay, replay and ghost. Pause/focus loss/countdown clear keys and show KPS `--` while the chart estimate stays fixed. The input path only copies events to a bounded SPSC queue; no allocation, mutex or worker wake. A below-normal-priority worker updates keys/KPS about every 8ms, with a nonblocking HUD snapshot and 100ms stale cutoff. Overflow clears optional visualization without waiting in gameplay. The worker stops with the session. Actual hardware latency/FPS improvements have not been measured.

Classic/Studio/player/ghost default gauges match clear cards: **EZ #00ED57, NM #FFF000, HD #FF3838, EX rainbow** anchored to the full track. Explicit imported skin colors take precedence. Gauge math, judgement, score and replay contracts are unchanged.

Existing grades use unrounded ordinary accuracy: S+++/S++/S+ ≥99/98/97% gold **#FFD66B**; S ≥95% cyan **#70ECFF**; A+/A ≥93/90% lavender **#9DB3FF**; B ≥85% green **#78E3A9**; C ≥80% orange **#FFAD70**; D ≥70% and failed F pink-red **#FF7386**; surviving below70% E blue-gray **#B6C0D0**. Autoplay is `AUTO`, no record/abort `--`. This ladder is unchanged. MIT provenance stays in `third_party/key_note_overlay/`. Automated checks do not prove physical input, OBS or long-session behavior.
