# TenRiff 1.9.9.8 hot fix 1 — 2026-10-30

オーディオ出力を利用できずソロプレイの開始に失敗すると、選曲画面の下部に黄色の案内を表示します。**オプション > オーディオ > サウンド出力デバイス**で接続済みのデバイスを選択し、再試行してください。ASIOの場合はインストール済みのドライバーを選択します。セッションの初期化が成功すると案内は消えます。

[Details](audio-device-guide-20261030.ja.md) · [Validation](release-1.9.9.8-hotfix1-gate.md)

The old failure text was only drawn inside the difficulty-table URL editor. The fix uses the visible song-selection footer. The saved output choice is retained; there is no silent output switching. Audio processing, input, judgement, scoring, R13/R14 and replay/peer contracts remain unchanged. This does not change the multiplayer loading-error screen. No website or database deployment. Fixed baseline: **1.9.6**.

[Release and downloads](https://github.com/10kseason/TenRiff/releases/tag/1.9.9.8-hotfix1). Extract into a new folder and run launch_win.bat. Preserve existing installations and profiles.
