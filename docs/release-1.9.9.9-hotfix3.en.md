# TenRiff 1.9.9.9-hotfix3

- **NK2 E+** supports 7K, 8K and 10K. In Options → Mode, select the target key count, NK2 and **E+ - Full-field + taps**. Returning to a supported key mode restores the preset. Added taps at retained long-note tails use the original sample times.
- **Safe indexing** shares one chart read between parsing and file hashing, and reuses the calculated timeline for NPS and difficulty. For 12,000 synthetic charts, median initial indexing fell from 10.898 s to 8.501 s, and median cache reopening fell from 3.414 s to 3.042 s.
- **Add Songs Folder** sits below the Library tab. The Windows folder picker opens directly from Song Select. Title and artist sort labels use A-Z and Z-A.
- **TenRiff Leaderboard** handles online lookup and submission. Choose Online with F10, then paste the connection information from the official site or import the connection file. In-game lookup supports R13/R14 and separate E+ v1/v2 boards. Multiplayer supports LAN discovery and direct peer connections.

Extract the Windows ZIP into a new folder and run `launch_win.bat`. Keep existing installations and profiles separately. The fixed baseline is **1.9.6**.

[TenRiff Leaderboard](https://tenriff-leaderboard.lastestarcorp.chatgpt.site) · [Connection guide](sites-leaderboard.md) · [Validation scope](release-1.9.9.9-hotfix3-gate.md)
