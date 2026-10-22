# TenRiff 1.9.9

This follow-up adds KeyWeaver 1.3.0 NK2 preset E and 16K leaderboard submission to 1.9.8.5. The general fixed baseline remains 1.9.6; prior releases, installations and profiles are preserved.

Select **NK2** and **E - Source LN** in Game Mode Settings. Targets remain the existing **4/5/6/7/8/9/10/12/14/16K** menu modes. Fresh settings default the NK2 preset to E while explicit saved Native/Transform/Remaster selections and the default Krrcream algorithm remain unchanged. Expansion uses E phrase search and extra taps/holds; every generated hold fits wholly inside one source hold. Reduction places or drops notes without additions or retiming. F's boost is disabled.

Source sample times, hold lengths, keysounds and release semantics remain intact; generated notes do not duplicate keysounds. Independent safety and exact sample-occupancy checks reject unsafe output with a warning and retain the native chart. The engine uses millisecond resolution and runtime base BPM. Historical NK2 presets keep their original isolated engine so old replays retain their meaning.

E uses `source_ln_e_v1` in the existing replay preset field and requires the **1.9.9 verifier**. This also includes 1.9.8.5 secondary-key takeover replay validation. NK2 E OFF/Mirror ghosts accept differing unused play seeds; random, note-add, LN-mix and BMS branch checks remain. Timing, grades, R9/R10, replay format and peer v7 remain unchanged.

Both client and web server accept **16K**, with a 16K web filter. Previous web 4–14K support, including historical 11/13K reads, remains; 15/17K are rejected. E is identified separately as `nk2:source_ln_e_v1`. New submissions still require standard Online R9/R10 and retain Guest/custom/LNMix v2 exclusions and the `client_submitted` trust boundary. There is no database migration, board-ID change or rewriting of old records. Skin format is unchanged and the public Skin Editor remains v11.

See [release gates](release-1.9.9-gate.md) for verification requirements and limits. Synthetic/local API checks do not establish physical-input, long-session, other-PC or real-account production-upload behavior. The [1.9.9 release page](https://github.com/10kseason/TenRiff/releases/tag/1.9.9) is authoritative for publication and downloads.
