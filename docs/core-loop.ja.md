# Core Play Loop (Initial Implementation)

この文書は、現在実装されている **core play loop** の構造とデータフローをまとめたものです。

## Core Flow
1. **InputThread** が RawInput または polling 入力を集めて `SPSCQueue` に渡す
2. **AudioThread** の callback 内で `ClockSync` が入力 timestamp を sample time へ変換する
3. **GameplayEngine** が input / timeline data を消費して judgement、gauge、statistics を更新する
4. オーディオバッファは現状 **silence (fill 0)** を基本とし、keysound / music mixing は後段で結び付く

## Main Components
- `config/Config.*`
  - **SimpleJson** parser 経由で `config.json` をロード
  - `audio/input/judge/speed/gauge/ui/offsets` セクションを適用
- `config/Keymap.*`
  - `keymap.json` をロードし、default keymap を構築
  - `KeycodeMap` で key string を keycode に変換
- `gameplay/GameplayChart.*`
  - BMS timeline を **sample-time-based note events** に変換
  - `rate` 適用時は `t' = t / rate` で schedule をスケーリング
- `gameplay/GameplayEngine.*`
  - judgement windows (`PG / GR / GD / BD`) と 30ms mask を適用
  - 同一 lane の古い note がすでに BD で、直後の note が明確に GD 以上なら、古い note を miss として記録し、現在の press を次の note に割り当てて BAD chain を防ぐ
  - POOR event に lane mask を適用
  - **Hold rule**: 早離しは BAD
  - **Hold tail rule**: BMS `#LNMODE 2` charge note のみ通常 judgement window を使って release timing を評価（head/tail 50:50）
  - 通常 BMS long note は最後まで保持されれば終端で auto 処理され、tail-release timing judgement は使わない
  - result statistics（combo、judgement counts、average / standard deviation）を集計
- `app/GameSession.*`
  - CLI options -> config application -> chart loading -> input / audio thread startup
  - audio callback 内で input queue 消費 + judgement 更新
  - polling backend は `input.polling_hz`（`1000..8000 Hz`）で keyboard state をサンプリング
  - judgement / miss / hold 更新は audio buffer ごと 1 回ではなく、別の `input.judgement_hz` cadence（`1000..8000 Hz`）で callback 内 sub-step される

## Initial Judgement Policy To Note
- **Hold tail judgement** は BMS `#LNMODE 2` charge note にのみ適用され、早離しは BAD

## Planned Future Connections
- Menu state machine (Title / SongSelect / Gameplay / Result)
- SongIndexerThread + cache
- Key remap UI + NKRO test screen
- Result screen + replay / result JSON saving
- Launcher expansion and log / environment diagnostics
