> **Historical document — local candidate before 1.9.9.5:** Behavior, unpublished status and check counts below describe that candidate only. Candidate evidence is not fresh release evidence. Current scope/colors follow [1.9.9.5](release-1.9.9.5.en.md); fresh verification follows the [public release gates](release-1.9.9.5-gate.md).

# Recovery from stale render wait compensation

Transient timer lateness can increase the oversleep estimate until it excludes every subsequent timer sleep. With no new sleep samples, the estimate previously stayed high indefinitely.

The candidate reduces the estimate to 7/8 only after an estimate-blocked wait with no actual sleep, and only when at least 100ms has passed since the last measurement or recovery. It does not decay per frame or per yield. Actual timer/fallback samples retain the EMA; short waits, target FPS, deadlines, VSync and unlimited mode retain their behavior. Input, judgement, audio, replay code and settings are unchanged. Prior gold/skin/BGA changes are inherited; blue rank images remain concepts.

This addresses a reproduced scheduling state, not a proven Discord root cause or end-to-end fix. Persistently late timers still incur occasional retry costs. Per-frame decay was rejected after a sustained-lateness model exposed regressions. The 100ms interval balances recovery against retry frequency; recovery time depends on the available frame budget.

For in-game cursor stutter without streaming, compare the same scene with overlays disabled one at a time. A loaded module alone does not prove a conflict. [Discord's overlay guide](https://support.discord.com/hc/en-us/articles/217659737-Game-Overlay-101) recommends disabling the overlay for a game experiencing lag.

Local candidate only; published 1.9.9.3 and fixed baseline 1.9.6 remain unchanged.
