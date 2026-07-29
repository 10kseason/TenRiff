# Low-Latency Implementation Plan

TenRiff は input-to-judgement-to-sound の end-to-end 遅延を最小化することを重視します。このノートは、"Raw Input -> SPSC queue -> Audio thread judgement" という方針を保ったまま、後続作業でどこを締めるべきかをまとめたものです。

## Input Pipeline and Timestamping
- BMS timeline は load 時に sample positions へ正規化し、judgement、keysound、mixer が同じ決定的 clock で動くようにする。
- per-profile `input_offset_ms` を追加し、device / driver 遅延を微調整できるようにする。
- in-game loopback calibrator で key press の beep と mic loopback から推奨 offset を提案する。
- HUD に現在の PG / GR / GD / BD window を表示し、rate 変化時の体感差を見やすくする。
- raw event hygiene、multi-device ingest、stateful input tracking を維持し、duplicate edge を圧縮しつつ実際の Press/Release 遷移は保持する。

## Audio and Threading
- `--audio-backend=wasapi|asio`、`--audio-backend=alsa|jack` のような backend 切替を用意する。
- adaptive buffer step-up（128 -> 192 -> 256）で xrun 時に自動的に余裕を増やす。
- AudioThread を P-core に寄せやすいよう thread affinity を可視化する。
- audio callback は playback buffer sample domain で扱い、`buffer_start_samples` / `buffer_end_samples` 基準で keysound を配置する。
- `ClockSync` は outlier rejection、EMA、reset hook、monotonic clamp で強化する。
- judgement window は sample domain に変換して比較する。
- AudioThread 内では allocation / file I/O / lock を禁止する。

## Rendering and Frame Pacing
- VSYNC OFF、triple buffering 無効、pre-rendered frames 低設定をガイドする。
- NVIDIA / AMD / Intel の frame queue 設定を文書化し、in-game 設定とも対応させる。
- menu / result でも gameplay と同じ monotonic clock と input -> audio 分離を守る。
- render は immutable snapshot を消費するだけで、authoritative state を変更しない。

## UI / UX for Latency Awareness
- latency HUD で input -> audio delta histogram、queue depth、xruns を表示する。
- audio callback budget を `callback_time_ms / buffer_length_ms` で可視化する。
- late input counter を追跡し、`press_sample < buffer_start` になった入力を見える化する。
- 遅延が閾値を超えたら、ASIO / JACK、RawInput / evdev grab、VSYNC OFF などの対策を示す toast を出す。
- NKRO test UI では多キー同時押し確認と ghosting 検出を行う。

## QA and Tooling
- performance log export を CSV で残せるようにする。
- launcher で power plan、CPU boost、USB polling rate を確認する preflight を用意する。
- 権限不足で RT scheduling や evdev grab が失敗した場合の remediation を表示する。
- `--burnin` で長時間自動プレイし、input queue、xruns、latency 回帰を早期発見する。
- replay は sample-time input として保存し、決定的再シミュレーションに使う。
