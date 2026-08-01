# TenRiff Development Roadmap (staged)

この roadmap は、スコープ肥大化を避けながらゲームループを拡張していくための推奨高位順序をまとめたものです。各段階でまず方向を固定し、その上に追加機能を積み重ねます。

## Current Baseline
- Windows GUI / runtime が主なサポート経路です。
- プロジェクト版ラインは `1.2.93 stable`。公開 package は upscaler model を同梱しない。互換性と権利を確認済みの ONNX を選択しても path を保存するだけで、BGA Upscaler は user が有効化して high-spec warning を確認するまで off のまま。自動 benchmark gate はない。
- 現行 menu / runtime は BMS family（`.bms/.bme/.bml/.pms`）専用で、native/LR2 skin をサポート。
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

## 1.5) BMS-only chart support の強化
- 以前の multi-format 方針は現行 release line では superseded。loading、indexing、replay、result、difficulty-table は BMS family に集中する。
- archive や別 format の import path を再導入せず、real-pack の encoding、keysound、BGA、long note、lane layout 互換性を強化する。
- optional integration は user-supplied、明示的に有効化するまで disabled、失敗時は native behavior へ安全に復帰できる形を維持する。

## 2) Key remap と 8K / 10K modes
- ✅ 「リマップ UI フロー」仕様どおりの key remapping UI（NKRO test を含む）。
- ここまで来ると、プロジェクトは十分実用的な個人練習ツールになる。

## 3) Lane transform / Random mode
- ✅ **Mirror**、**Full Random (FR)**、**Super Random (SR)** は実装済み。**AR** は挙動を定義するまで後回し。

## 4) Launcher を付ける
- folder check、初回 config 生成、error code 整理を扱う。
- これが終わると、ゲームはローカル PC 上で自己完結する。
