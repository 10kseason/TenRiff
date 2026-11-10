# LR2 clear lamp import

Imports clear lamps from an LR2 score database into a TenRiff profile and shows them in Song Select. Display only: an imported lamp is never a TenRiff result, a BEST record or an upload candidate.

## Usage

### Import in game

Close LR2, then open **Options → Profile → Import LR2 Records** and select the score `.db` file. The active profile receives the lamps. Registered song folders and the current folder are checked for EX; basic lamps are imported even without matching songs.

Import runs in the background. Select the same button again to cancel. Completion refreshes Song Select immediately without restarting. Reimport replaces the previous LR2 lamp list. Cancellation, read/write failure or closing the game keeps the existing list. TenRiff records, BEST and online eligibility are unchanged.

### Command line

```
tenriff-lr2-import.exe <LR2 score .db> <profile folder> [songs folder ...]
```

- The LR2 database is opened read-only.
- Output is `<profile folder>\lr2_import.json`, read whenever the game reloads records. Delete the file to remove the imported lamps. Running the tool again rewrites it.

## Display rules

| LR2 | TenRiff lamp |
|---|---|
| EASY | EZ CLEAR |
| NORMAL (GROOVE) | NM CLEAR |
| HARD, FULL COMBO | HD CLEAR |
| plays that pass the check below | EX CLEAR |

- A TenRiff record with the same or a better clear is shown instead.
- When an imported lamp is shown, the selected song's lamp text is prefixed with `LR2 `; an LR2 full combo adds `(FC)`.
- Scores, ranks and accuracy are not imported.

## EX check

When song folders are given, the LR2 `ghost` (per-note judgement order of the best-score play) is replayed through TenRiff's EX-HARD gauge. Each LR2 judgement is replaced by **the worst TenRiff Normal-mode judgement possible inside that LR2 timing window**. A play that still survives is recorded as EX.

- LR2 windows (ms, RANK VERY HARD/HARD/NORMAL/EASY): PGREAT 8/15/18/21, GREAT 27/32/40/60, GOOD 40/60/100/120, BAD 200. PGREAT and BAD agree between lr2oraja's LR2 judge table and Lunatic Vibes; GREAT/GOOD differ by a few ms and the wider value is used.
- Example: at RANK NORMAL an LR2 GOOD (up to 100 ms) can exceed TenRiff's GOOD edge (98.57 ms), so it counts as BAD. An LR2 BAD beyond TenRiff's BAD window counts as an empty poor plus a miss.
- The gauge is the same `GaugeManager` used in play (BMS TOTAL based EX-HARD).

Not evaluated (LR2 lamp only): chart not found in the song folders, charts with long notes, mines or `#RANDOM`, missing or out-of-range `#RANK`, note count different from the LR2 record, and ghosts that are missing, incomplete or inconsistent with the stored counts.

## Limits

- The ghost format was reverse-fitted from one real LR2 database (judgement counts match in 5,274 of 5,285 rows), not taken from a specification.
- The ghost belongs to the best **score** play, which may differ from the play that earned the LR2 lamp.
- The worst-case rule rejects plays that would in fact have survived.
- LR2 and TenRiff gauges differ, so equally named lamps are not equally hard.
