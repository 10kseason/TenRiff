# TenRiff 1.9.9.9-hotfix2 — NK2 E+ preset for 8K and 10K

In Options > Mode, select **8K or 10K**, the **NK2** conversion algorithm, then **E+ - Full-field + taps** after E. Changing to another key mode resets this preset to E. E+ also remixes charts already using the target key count.

Import KeyWeaver 1.4.0 E+ as an isolated engine. It uses the full lane field and adds taps only at retained original note heads. Preserve original sample times, LN semantics and keysounds; generated notes stay silent. Removed original keysounds use the existing autoplay-cue path. Failed safety validation retains the source pattern. Existing E, Native/Transform/Remaster and defaults remain unchanged.

The stable identity `fullfield_taps_eplus_v1` separates replay, BEST, ghost and online-submission conversion modes without changing R13/R14 rules or file formats. Do not play E+ replays in older clients that lack this preset. Conversion quality and physical play feel are not guaranteed for arbitrary charts.

Includes hotfix1 OGG security and Common BGA changes. Fixed baseline remains **1.9.6**. Extract the Windows ZIP into a new folder and run `launch_win.bat`; preserve existing installations, profiles and records. This client release does not redeploy the website or database.

[Engine provenance](../nk2eplus/README.md) · [Validation](release-1.9.9.9-hotfix2-gate.md)
