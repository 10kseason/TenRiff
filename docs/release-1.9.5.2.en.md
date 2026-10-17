# TenRiff1.9.5.2 — Note and field size down to10%

Review also fixes BMS editor LN bodies that could outgrow narrow heads. Bodies stay within80% of the displayed head; previous body coordinates at50–140% remain unchanged. The editor retains its8px minimum head for readability, so very narrow lanes do not scale strictly proportionally. Gameplay field scaling is separate.

The native preview tool now accepts the new runtime-transition range and forwards editor width/key count. It records actual values and note regions for10%/LN pixel checks; editor input dispatch and pixel validation are reported separately.

Includes GOMazk’s contribution (PR #102). Note & Field Size expands from50–140% to10–140%. The100% default, saved values and per-key-mode settings are preserved; menus, gameplay, ghost and preview use the same lower limit. Judgement, score, input and audio calculations and the1.9.5.1 350ms FAST/SLOW/file-picker behavior remain unchanged.

Contribution: [GOMazk / PR #102](https://github.com/10kseason/TenRiff/pull/102), commit `29bc6341588390b2299d335a248617d2cde6d0ef`. The original contribution remains in Git history.

The fixed baseline remains1.9.0. No replay/schema migration, website/database deployment or bundled BGA restoration weights. Existing releases, installations and profiles are preserved.

Verification scope: [release gate](release-1.9.5.2-gate.md). Publication and downloads: [1.9.5.2](https://github.com/10kseason/TenRiff/releases/tag/1.9.5.2). Physical long play, other PCs and arbitrary imported skins remain unverified.
