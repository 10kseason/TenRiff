# TenRiff 1.9.9.9-final4

Adds `tenriff-lr2-import`, a display-only import of clear lamps from an LR2 score database. EX is shown only for plays whose LR2 judgement order survives the EX-HARD gauge under worst-case timing. Adds a preventive measure for the lock shared by the audio callback and the HUD thread. The fixed baseline moves to **1.9.9.9-final4**.

## Use

Extract the release ZIP into a new folder and run `launch_win.bat`.

### LR2 clear lamp import

### Import in game

Close LR2, then open **Options → Profile → Import LR2 Records** and select the score `.db` file. The active profile receives the lamps. Registered song folders and the current folder are checked for EX; basic lamps are imported even without matching songs.

Import runs in the background. Select the same button again to cancel. Completion refreshes Song Select immediately without restarting. Reimport replaces the previous LR2 lamp list. Cancellation, read/write failure or closing the game keeps the existing list. TenRiff records, BEST and online eligibility are unchanged.

### Command line

```
tenriff-lr2-import.exe <LR2 score .db> <profile folder> [songs folder ...]
```

The LR2 database is opened read-only and the only output is `<profile folder>\lr2_import.json`; delete it to remove the imported lamps. An imported lamp is shown in Song Select only when it outranks the TenRiff record, and the selected song's lamp text is prefixed with `LR2 `. It is never a result, a BEST record or an upload candidate, and scores are not imported. No user's LR2 database ships with the release. Rules and limits: [LR2 clear lamp import](lr2-clear-import.en.md).

### Audio callback lock

The HUD thread raises its priority only while it holds the lock shared with the audio callback (`TENRIFF_HUD_LOCK_CEILING=0` disables it). The audio callback no longer queries the window manager or writes to the console. Measurements showed no audio stall before the change either, so this is preventive: no audible or latency improvement is claimed. Running with `TENRIFF_RT_PROBE=1` prints lock wait/hold times to standard error when a song ends.

## Compatibility

Judgement, score and gauge calculations, input, the replay format and R1–R14, the peer protocol and leaderboard identities are the same as final3. Saved profiles and settings load unchanged. The web leaderboard (v28) and skin editor (v14) are unchanged.

## Fixed baseline

From this release the general source baseline is **1.9.9.9-final4**. [Baseline](baseline-1.9.9.9-final4.en.md).

Physical input/listening, long sessions, other PCs, lock measurements under WASAPI exclusive or ASIO, and other users' LR2 databases remain separate checks. [Release gates](release-1.9.9.9-final4-gate.md).
