# TenRiff Developer Extension Guide
Language: [한국어](developer-extension-guide.md) | [English](developer-extension-guide.en.md) | [简体中文](developer-extension-guide.zh-CN.md) | 日本語

このガイドは、新しい `mode/mod` を追加するとき、あるいは既存の mode pipeline を拡張するときに、どこを変更すべきかを説明します。プレイヤー向けではなく、runtime、UI、migration、replay/result、test 層を横断する保守マップです。

## Responsibility Map

- `src/gameplay/ModeSettings.h` / `src/gameplay/ModeSettings.cpp`
  - `ChartFormatMode`、`KeyMode`、`GaugeMode`、`RandomMode` などの基本 enum と、string parser / formatter。
- `src/app/ModeResolver.h` / `src/app/ModeResolver.cpp`
  - `config::ModeConfig` を runtime の `gameplay::ModeSettings` に変換し、不正 token の warning を集める。
- `src/app/ModeManager.h` / `src/app/ModeManager.cpp`
  - mod registry、category、score multiplier、judge-window scaling、chart transformation を管理。
- `src/gameplay/ModeApplier.h` / `src/gameplay/ModeApplier.cpp`
  - 実際の `GameplayChart` に key-mode conversion と random-style transformation を適用。
- `src/app/menu/settings/ModeSettingsController.h/.cpp`, `src/app/MenuAppSettings.cpp`, `src/app/MenuAppSettingsUtils.h`
  - `Mode Settings` / `Mod Manager` の typed row/state/mutation と application-boundary effect / label helper を分離して担当。
- `src/app/menu/MenuScreenDescriptor.h/.cpp`, `src/app/MenuAppTail.inl`
  - 固定 screen title/skin/routing metadata と動的 help/render assembly の境界。
- `src/config/Config.h` / `src/config/Config.cpp`
  - `config/config.json` と profile 単位設定の load/save schema。
- `src/app/RuntimeConfigMigration.cpp`
  - legacy defaults や旧 token を現在 model へ移行。
- `src/app/PersistedRuntimeConfig.cpp`
  - session-only mode を永続 config から除去。
- `src/gameplay/Replay.cpp`, `src/gameplay/Replay.h`, `src/app/MenuRecordUtils.cpp`, `src/app/GameSession.cpp`, `src/app/MenuAppTail.inl`
  - replay/result の save/load と result 画面表示。

## Add A New Mode

1. `src/gameplay/ModeSettings.h` で enum または token 定義を追加する。
2. `src/gameplay/ModeSettings.cpp` で `to_string(...)` と `parse_...(...)` を更新する。
3. config file に現れうる token なら `src/app/ModeResolver.cpp` で扱い、不正値 warning を出す。
4. mode が chart structure を変えるなら `src/app/ModeManager.cpp` または `src/gameplay/ModeApplier.cpp` に実際の変換を実装する。
5. menu から編集させるなら stable `ModeSettingId` と typed controller/view row を追加し、`MenuAppSettings.cpp` には返された保存/reindex の境界 effect だけを接続する。
6. persistence や migration が絡むなら `src/config/Config.cpp`、`src/app/RuntimeConfigMigration.cpp`、`src/app/PersistedRuntimeConfig.cpp` を更新する。
7. unit/smoke coverage を追加し、user-visible behavior になったら docs も同期する。

## Add A New Key Mode

- `src/gameplay/ModeSettings.h` / `src/gameplay/ModeSettings.cpp`
  - 新しい `KeyMode` enum 値を追加し、`parse_key_mode(...)` / `to_string(...)` を同期する。
- `src/gameplay/ModeApplier.cpp`
  - `target_lane_count(...)` と lane-conversion logic を更新し、新 key count を正しく扱う。
  - note timing と hold metadata が remap 後も残ることを確認する。
- `src/app/ModeManager.cpp`
  - `target_lane_count(...)` や `key_mode_for_lane_count(...)` などの helper を更新する。
- `src/app/MenuAppSettingsUtils.h`
  - `normalize_runtime_key_mode(...)`、`cycle_runtime_key_mode(...)`、key-mode label helper を確認する。
- `src/app/MenuAppSettings.cpp`
  - `Key Mode` 行の label と left/right cycling を更新する。
- `src/app/MenuApp.cpp`
  - keymap editor、現在の chart lane count、runtime lane-binding 経路が新 mode に追従するようにする。
- `src/app/GameSession.cpp`
  - 選択された lane count が replay/result metadata に反映されるようにする。
- `src/app/PersistedRuntimeConfig.cpp`
  - 本当に session-only でない限り、誤って persistence から消されないことを確認する。

## Add A New Gauge Mode

- `src/gameplay/ModeSettings.h` / `src/gameplay/ModeSettings.cpp`
  - `GaugeMode` を拡張し、string conversion をそろえる。
- `src/app/MenuApp.cpp`
  - `gauge_type_from_mode_string(...)` や表示 label helper を更新する。
- `src/app/MenuAppSettings.cpp`
  - `Gauge` 行の cycling 順序を調整する。
- `src/app/ModeManager.cpp`
  - `scale_judge_windows(...)` が意図した gauge policy に一致することを確認する。
- `src/app/RuntimeConfigMigration.cpp`
  - default 値が変わるなら、旧 shipped default と完全一致する profile だけを移行する。

## Add A New Random Mode

- `src/gameplay/ModeSettings.h` / `src/gameplay/ModeSettings.cpp`
  - 新しい random token と parser を追加する。
- `src/gameplay/ModeApplier.cpp`
  - `apply_full_random(...)` や `apply_super_random(...)` と同様の transformation branch を追加する。
  - random が key-mode conversion の前後どちらで走るかを明示する。
- `src/app/ModeResolver.cpp`
  - 不正 token の warning と fallback behavior を追加する。
- `src/app/MenuAppSettings.cpp`
  - `Random` 行の label と cycling を更新する。

fixed seed で結果が決定的であることが必須なので、新 random mode 追加時は `tests/unit/test_mode_applier.cpp` に determinism test を追加してください。

## Add A New Mod

- `src/app/ModeManager.cpp`
  - 新しい `ModeModDescriptor` entry を追加する。
  - `category_token`、`category_label`、`score_multiplier` をセットで決める。
  - chart structure を変える mod なら transformation helper を追加し、`manage_modes(...)` から呼ぶ。
- `src/app/ModeManager.h`
  - app 全体へ露出すべき helper を宣言する。
- `src/app/MenuAppSettings.cpp`
  - `populate_mode_mods_render_data(...)` は registry 駆動なので、多くの mod は自動表示される。必要なら help copy だけ加える。
- `src/app/PersistedRuntimeConfig.cpp`
  - session-only mod なら保存前に除去する。
- `src/app/MenuAppTail.inl`
  - result 画面の `Mods:` summary、multiplier text が正しく表示されるか確認する。

score に影響する mod なら、`rate_score_multiplier(...)`、`mod_score_multiplier(...)`、`final_score_multiplier(...)` を必ず再確認してください。

## Config, Migration, And Save Policy

- `src/config/Config.cpp`
  - Load: JSON token を読み、default を保つ。
  - Save: 正規化済み token 値を書き戻す。
- `src/app/RuntimeConfigMigration.cpp`
  - 旧 shipped default と完全一致する config だけを upgrade する。
  - custom user config を壊さないよう、matching rule は狭く保つ。
- `src/app/PersistedRuntimeConfig.cpp`
  - session-only mode だけを除去する。

## Replay, Result, And Records Impact

- `src/gameplay/Replay.cpp` / `src/gameplay/Replay.h`
  - `mode`、`raw_score`、`final_score`、`rate_multiplier`、`score_multiplier` などの replay/result JSON field を一貫させる。
- `src/app/GameSession.cpp`
  - session 終了時に最終 `ModeManager` 出力を適用する。
- `src/app/MenuRecordUtils.cpp`
  - 保存済み result / replay を `Records` view と result detail panel 用に parse する。
- `src/app/MenuAppTail.inl`
  - result 画面の文言、score multiplier、mod summary、replay/result path label を描画する。

## Tests And Docs Sync

- `tests/unit/test_mode_applier.cpp`
  - key-mode conversion、random determinism、hold metadata 保持
- `tests/unit/test_mode_manager.cpp`
  - mod registry behavior、category conflict、judge scaling、score multiplier
- `tests/unit/test_config.cpp`
  - save/load round-trip、case normalization、default migration
- `tests/unit/test_replay_export.cpp`
  - replay/result JSON field と restore behavior
- `tests/smoke/bms_mode_smoke.cpp`
  - 実 BMS 譜面での key mode、random、mods、既知 lane-remap ケース

docs は `docs/current-state.md`、`docs/config.md`、`docs/README.md`、その後 feature-specific docs の順で考えると漏れにくいです。

## Common Mistakes

- `ModeSettings` に token を足したが `ModeResolver` と menu UI を忘れる。
- registry に mod を足したが `score_multiplier` や category metadata が不完全。
- `PersistedRuntimeConfig.cpp` で session-only mod を除去し忘れる。
- 移行対象でない config まで一括 migration してしまう。
- replay/result field を変えたのに `MenuRecordUtils.cpp` と `tests/unit/test_replay_export.cpp` を更新しない。
- `none`、`auto`、大文字小文字 variant を本来 alias のはずなのに別意味として扱ってしまう。
- new key mode を追加したのに keymap、replay、result の lane-count 経路を忘れる。

## Verification Checklist

- `ModeSettings` enum/parse/to_string の round-trip が通る。
- `ModeResolver` が invalid token に warning を出し、安全な default へ fallback する。
- `ModeManager` が正しい category と multiplier で new mod を登録する。
- `ModeApplier` が new key mode と random behavior を決定的に適用する。
- `MenuApp` が必要な場合に `Mode Settings` または `Mod Manager` に新しい行を出す。
- `Config` save/load と `RuntimeConfigMigration` が既存 user config を壊さない。
- replay/result JSON に new mode 情報が含まれる。
- `tests/unit` と `tests/smoke` が通る。
- user-visible な挙動なら `docs/current-state.md` と `docs/config.md` を更新する。
