# TenRiff Development Roadmap (staged)

この roadmap は、スコープ肥大化を避けながらゲームループを拡張していくための推奨高位順序をまとめたものです。各段階でまず方向を固定し、その上に追加機能を積み重ねます。

## Current Baseline
- Windows GUI / runtime が主なサポート経路です。
- プロジェクト版ラインは `1.1.2` で、現在の公開 `final stable` バージョンとして命名されています。
- 既定では BMS-first の menu / runtime が有効で、4K-10K `.osu` は config / menu toggle の背後にある任意機能です。
- 現在出荷されている挙動は [`docs/current-state.ja.md`](current-state.ja.md) を先に見てください。この roadmap は方向と残作業を示す文書です。

## 0) 骨格と master clock を固める
- すべての時間敏感処理で **AudioThread を master clock** として扱う。
- **InputThread** は RawInput / evdev から event に timestamp を付け、SPSC queue に積む。
- 譜面 timeline は **sample positions (int64)** へ正規化し、audio thread が決定的 timestamp を消費できるようにする。
- **Render** は snapshot を描くだけに限定し、judgement / score は audio 側で確定させる。

## 0.5) 低遅延ループを強化する
- 1 つの backend（WASAPI / ALSA）を end-to-end で仕上げ、device padding を露出させ、`buffer_start_samples` を明示計算して mixer が playback-buffer domain で動くようにする。
- ClockSync は外れ値除去、sliding-window EMA 更新、device 変更 / underrun reset hook、monotonic clamp で強化する。
- AudioThread では入力を取り出して sample time に変換し、late / normal / future に分岐して keysound を配置する。
- 判定 window を sample で表現し、callback budget / late inputs / xruns の HUD カウンタを出し、rate 可変時は window を適切にスケーリングする。
- ✅ replay を sample-time input trace（lane / state / sample）として保存し、決定的再現のための JSON export を書き出す。

## 1) 1 曲を end-to-end で遊べるようにする
- menu からも audio backend を silent callback 付きで動かし、gameplay 前から master clock を安定させる。
- ✅ UI state machine（console）として **Title -> Song Select -> Play -> Result** を InputThread / SPSC とともに立ち上げた。
- ✅ Windows D3D11 menu UI（text + background + focus styling）を追加した。
- ✅ async SongIndexerThread と cached index（mtime / hash）を追加し、Song Select がスキャン中も応答するようにした。
- 最小 BMS loader（essential channel のみ）-> note scheduling -> judgement -> result screen を通す。
- preview audio は audio engine で予約し、Song Select 中に keysound を preload する。

## 1.5) BMS と osu! beatmap の両対応
- BMS と並行して osu! beatmap（mania）loader を追加し、正規化後イベントモデルを共有する。
- scheduling / judgement 経路は共通のままにし、chart-format 差分は load 時点に閉じ込める。
- Song Select に format selection を出し、replay / result 画面にも元 format を表示する。

## 2) Key remap と 8K / 10K modes
- ✅ 「リマップ UI フロー」仕様どおりの key remapping UI（NKRO test を含む）。
- ここまで来ると、プロジェクトは十分実用的な個人練習ツールになる。

## 3) Random mode はまず 2 種
- ✅ **Full Random (FR)** と **Super Random (SR)** を先に入れ、**AR** は後回し。

## 4) Launcher を付ける
- folder check、初回 config 生成、error code 整理を扱う。
- これが終わると、ゲームはローカル PC 上で自己完結する。
