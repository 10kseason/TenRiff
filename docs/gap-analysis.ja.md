# TenRiff Manual Gap Analysis (v0.1)

- 参照文書: `개발메뉴얼(v0.1)/개발지시사항.txt`, `개발메뉴얼(v0.1)/Tenriff 런쳐 개발 지시사항.txt`,
  `개발메뉴얼(v0.1)/UI 개발지시사항.txt`, `개발메뉴얼(v0.1)/RAW 인풋과 멀티스레드 활용과 최적화에 대한 개발 지시사항 메뉴얼.txt`
- 分析日: 2025-12-23
- 対象範囲: `src/`, `docs/`, `launch_*.{bat,sh}`, `profiles/default/`（Songs folder は除外）

## Severity Criteria
- Critical: end-to-end play loop を塞ぐ、または audio master clock や determinism といった中核原則に反する gap
- High: 核心機能 / UX 要件が抜けており、実用性が大きく下がる gap
- Medium: 直ちに詰まらせはしないが、機能・検証・追跡がまだ必要な重要 gap
- Low: 品質や利便性の改善項目

## Summary (unresolved)
- Critical: 0
- High: 1
- Medium: 8
- Low: 1

## Items Already Satisfied / Partially Satisfied
- BMS parser / normalize / timeline（sample-time）基盤
- 基本 10-key channel mapping
- SpeedManager / GaugeManager の baseline spec 対応
- BMS family 専用 chart loader / indexer
- RawInput + InputThread + SPSCQueue + ClockSync scaffolding
- Input polling（1000 / 2000 / 4000 / 8000 Hz）+ RenderThread 分離
- WASAPI backend + AudioThread skeleton
- Launcher script bootstrap

## Detailed Gaps

### 1) audio-master-clock ベースの “play loop” が未完成
- Requirement: AudioThread を master clock にし、judgement / gauge / keysound を audio thread 側で処理する
- Current state: audio callback はすでに input 消費と judgement / gauge 更新を回している
- Gap: keysound / real-audio mixing が未実装
- Severity: Medium

### 2) menu state machine（Title / SongSelect / Gameplay / Result）が未完成（audio 再利用）
- Requirement: menu state machine + SongIndexerThread + cache + live audio-clock retention
- Current state: menu state machine + Windows D3D11 menu UI + SongIndexerThread + cache は実装済み
- Gap: menu-to-gameplay の audio device reuse が不完全
- Severity: Medium

### 3) input event と judgement の接続不足
- Requirement: InputThread -> SPSCQueue -> audio-thread event consumption / judgement
- Current state: audio callback がすでに input を消費し、judgement / gauge を更新
- Gap: なし（初期実装完了）
- Severity: Resolved

### 4) Rate / Hi-Speed の適用範囲が不完全
- Requirement: Rate は schedule / judgement window に影響し、Hi-Speed は視覚スクロールだけを変える
- Current state: SpeedManager はあるが、timeline / schedule / 実判定経路にまだ完全には接続されていない
- Gap: Rate 変更が実際の playback / judgement timing に反映されない
- Severity: High

### 5) Key remap + profile save / duplicate warning UI
- Requirement: `keymap.json` 保存、重複防止、NKRO test UI
- Current state: remap UI / save / duplicate handling / test screen 実装済み
- Gap: なし
- Severity: Resolved

### 6) Result screen と replay saving
- Requirement: 詳細 result screen + Enter wait + replay saving
- Current state: result screen / statistics display / Enter return + replay / result JSON saving 実装済み
- Gap: なし
- Severity: Resolved

### 7) chart input scope
- Requirement: BMS family 専用の loading / indexing
- Current state: `.bms/.bme/.bml/.pms` のみを support し、osu loader と import path は削除済み
- Gap: なし
- Severity: Resolved

### 8) judgement rules（window / mask / LN handling）未実装
- Requirement: PG / GR / GD / BD / PR window、30ms lane mask、LN retention / leave rule
- Current state: judgement / mask / LN rule は GameplayEngine に実装済み
- Gap: なし
- Severity: Resolved

### 9) config.json load と CLI 優先適用が未実装
- Requirement: CLI > config.json priority、rate / HS / gauge 適用
- Current state: global + profile config load と CLI priority は適用済み
- Gap: なし
- Severity: Resolved

### 10) Launcher 機能拡張が未完成
- Requirement: binary metadata、SDL2 / VC++ guidance、exit code 別 log tail
- Current state: 基本 folder check / default file creation のみ
- Gap: 診断 / guidance が不足
- Severity: Medium

### 11) Linux input / audio path 未実装
- Requirement: evdev + ALSA（または代替）対応
- Current state: Windows RawInput / WASAPI のみ
- Gap: Linux execution path がない
- Severity: Medium

### 12) performance / latency metrics HUD と diagnostic logging が未実装
- Requirement: latency histogram、xruns、queue depth などを表示 / 記録する
- Current state: 文書だけ存在
- Gap: instrumentation / diagnostic tool がない
- Severity: Medium

### 13) unit test coverage が不十分
- Requirement: key remap、judgement / shift cooldown、Rate window scaling などのテスト
- Current state: parser / normalize / Speed / Gauge の一部テストのみ
- Gap: 新機能向け test が不足
- Severity: Medium

### 14) 文書 / コード同期の不足
- Requirement: 最新進捗を README / docs に反映
- Current state: README は要約中心
- Gap: gap と優先順位の可視化が不足
- Severity: Low

### 15) keymap file format 拡張（layout / metadata）は反映済み
- Requirement: `keymap.json` が layout / bindings を明示的に持つ
- Current state: layout field は含まれている
- Gap: なし
- Severity: Resolved

## Decisions Needed
- どの UI framework を使うか
- Linux audio backend をどちら優先にするか（ALSA vs JACK）
- menu / gameplay の default rendering resolution と scaling policy
- Replay format は sample-time-based JSON で確定済み
