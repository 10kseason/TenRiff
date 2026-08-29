# TenRiff 开发扩展指南
Language: [한국어](developer-extension-guide.md) | [English](developer-extension-guide.en.md) | [简体中文](developer-extension-guide.zh-CN.md) | [日本語](developer-extension-guide.ja.md)

本文说明在 TenRiff 中新增 `mode/mod`，或者扩展现有模式链时，应该修改哪些代码位置。它是面向维护和扩展的代码地图，不是面向玩家的说明书。

## 责任分工

- `src/gameplay/ModeSettings.h` / `src/gameplay/ModeSettings.cpp`
  - `ChartFormatMode`、`KeyMode`、`GaugeMode`、`RandomMode` 等基础枚举，以及字符串解析/输出函数。
- `src/app/ModeResolver.h` / `src/app/ModeResolver.cpp`
  - 把 `config::ModeConfig` 转成运行时使用的 `gameplay::ModeSettings`，并收集无效 token 的警告。
- `src/app/ModeManager.h` / `src/app/ModeManager.cpp`
  - 负责 mod 注册表、分类、分数倍率、判定窗口缩放和谱面结构变换。
- `src/gameplay/ModeApplier.h` / `src/gameplay/ModeApplier.cpp`
  - 把键位模式转换和随机类变换真正应用到 `GameplayChart` 上。
- `src/app/menu/settings/ModeSettingsController.h/.cpp`、`src/app/MenuAppSettings.cpp`、`src/app/MenuAppSettingsUtils.h`
  - 分离负责 `Mode Settings` / `Mod Manager` 的类型化行、状态、修改逻辑，以及应用边界效果和标签 helper。
- `src/app/menu/MenuScreenDescriptor.h/.cpp`、`src/app/MenuAppTail.inl`
  - 固定 screen title/skin/routing metadata 与动态帮助/渲染组装之间的边界。
- `src/config/Config.h` / `src/config/Config.cpp`
  - `config/config.json` 和每个 profile 配置的读写结构。
- `src/app/RuntimeConfigMigration.cpp`
  - 把旧默认值和旧 token 迁移到当前运行时模型。
- `src/app/PersistedRuntimeConfig.cpp`
  - 在保存运行时配置前移除仅在本局有效的模式。
- `src/gameplay/Replay.cpp`、`src/gameplay/Replay.h`、`src/app/MenuRecordUtils.cpp`、`src/app/GameSession.cpp`、`src/app/MenuAppTail.inl`
  - 回放/结果的保存、加载，以及结果界面展示。

## 新增一个 Mode

新增模式时，一般按下面顺序处理：

1. 先在 `src/gameplay/ModeSettings.h` 里增加枚举或 token 定义。
2. 在 `src/gameplay/ModeSettings.cpp` 里同步更新 `to_string(...)` 和 `parse_...(...)`。
3. 如果这个 token 会出现在配置文件里，就在 `src/app/ModeResolver.cpp` 里做解析和无效值警告。
4. 如果这个 mode 会改变谱面结构，就在 `src/app/ModeManager.cpp` 或 `src/gameplay/ModeApplier.cpp` 里加入实际变换逻辑。
5. 如果需要让用户在菜单里操作，就添加稳定的 `ModeSettingId` 和 typed controller/view row；`MenuAppSettings.cpp` 只连接 controller 返回的保存/reindex 等边界效果。
6. 如果涉及保存或迁移，就同时检查 `src/config/Config.cpp`、`src/app/RuntimeConfigMigration.cpp`、`src/app/PersistedRuntimeConfig.cpp`。
7. 最后补上单元测试/烟雾测试，并在行为已经对用户可见时同步文档。

## 新增一个 Key Mode

Key mode 不只是一个 token，它会影响输入映射、谱面变换、回放元数据和菜单编辑。

- `src/gameplay/ModeSettings.h` / `src/gameplay/ModeSettings.cpp`
  - 增加新的 `KeyMode` 枚举值，并同步 `parse_key_mode(...)` / `to_string(...)`。
- `src/gameplay/ModeApplier.cpp`
  - 更新 `target_lane_count(...)` 和实际 lane 变换逻辑，确保新 lane 数可以正确处理。
  - 检查音符时序和 hold 元数据在 remap 后是否还能保留。
- `src/app/ModeManager.cpp`
  - 更新 `target_lane_count(...)`、`key_mode_for_lane_count(...)` 之类的推断逻辑。
- `src/app/MenuAppSettingsUtils.h`
  - 检查 `normalize_runtime_key_mode(...)`、`cycle_runtime_key_mode(...)` 和 key-mode 标签帮助函数。
- `src/app/MenuAppSettings.cpp`
  - 更新 `Key Mode` 行的显示和左右切换行为。
- `src/app/MenuApp.cpp`
  - 确认 keymap 编辑器、当前谱面 lane 数、运行时 lane binding 路径都会跟随新模式。
- `src/app/GameSession.cpp`
  - 确认最终选中的 lane count 会正确写入回放/结果元数据。
- `src/app/PersistedRuntimeConfig.cpp`
  - 确认它不会被错误地从持久化配置里剔除，除非它真的是 session-only。

最常见的错误是只改了一个别名路径，却忘了 `none`、`auto`、大小写版本，或者菜单中的标签。所有层都应该表达同一个模式含义。

## 新增一个 Gauge Mode

Gauge 的变更不只影响 `ModeSettings`，还会牵涉标签、迁移和结果展示。

- `src/gameplay/ModeSettings.h` / `src/gameplay/ModeSettings.cpp`
  - 扩展 `GaugeMode` 并保持字符串转换一致。
- `src/app/MenuApp.cpp`
  - 更新 `gauge_type_from_mode_string(...)` 以及界面上显示的 gauge 标签。
- `src/app/MenuAppSettings.cpp`
  - 调整 `Gauge` 行的循环顺序。
- `src/app/ModeManager.cpp`
  - 确认 `scale_judge_windows(...)` 仍然符合目标 gauge 策略。
- `src/app/RuntimeConfigMigration.cpp`
  - 如果默认值改了，只迁移那些与旧 shipped default 完全一致的配置。

Gauge 信息会出现在结果页和回放/结果元数据里，所以如果你改了标签或序列化方式，也要同步检查 `tests/unit/test_replay_export.cpp` 和结果页路径。

## 新增一个 Random Mode

Random 模式在 `ModeSettings` 里只是一个字段，但真正的行为在 `ModeApplier`。

- `src/gameplay/ModeSettings.h` / `src/gameplay/ModeSettings.cpp`
  - 增加新的 random token 和解析支持。
- `src/gameplay/ModeApplier.cpp`
  - 增加对应的变换分支，风格类似 `apply_full_random(...)` 或 `apply_super_random(...)`。
  - 明确 random 是在 key-mode 转换之前还是之后执行。
- `src/app/ModeResolver.cpp`
  - 对无效 token 增加警告和回退逻辑。
- `src/app/MenuAppSettings.cpp`
  - 更新 `Random` 行的显示和循环逻辑。

Random 行为必须在固定 seed 下保持确定性。只要新增 random 模式，就建议在 `tests/unit/test_mode_applier.cpp` 里加确定性测试。

## 新增一个 Mod

大多数 mod 都应该放进 `ModeManager` 的注册表。关键是 token、分类、倍率、结构变换和保存策略要一起对齐。

- `src/app/ModeManager.cpp`
  - 增加新的 `ModeModDescriptor` 条目。
  - `category_token`、`category_label`、`score_multiplier` 要一起决定。
  - 如果这个 mod 会改变谱面结构，就增加对应的变换函数，并在 `manage_modes(...)` 中调用。
- `src/app/ModeManager.h`
  - 如果需要向外暴露新的辅助函数，就在这里声明。
- `src/app/MenuAppSettings.cpp`
  - `populate_mode_mods_render_data(...)` 是按注册表驱动的，所以多数新 mod 会自动显示；只有新分类需要解释时才补帮助文案。
- `src/app/PersistedRuntimeConfig.cpp`
  - 如果这是 session-only mod，就要在保存运行时配置前剔除。
- `src/app/MenuAppTail.inl`
  - 检查结果页里的 `Mods:` 总结和倍率文本是否能正确显示新 mod。

如果这个 mod 会影响分数，一定要重新检查 `rate_score_multiplier(...)`、`mod_score_multiplier(...)`、`final_score_multiplier(...)`。当前系统在最终分数上取 `rate` 倍率和 `mod` 倍率中的较小值。

## 配置、迁移与保存策略

新增设置时，要同时检查三个地方：

- `src/config/Config.cpp`
  - 读取时从 JSON 里拿到 token，并保留默认值。
  - 保存时写回已经归一化的 token。
- `src/app/RuntimeConfigMigration.cpp`
  - 只升级那些仍然和旧 shipped default 完全一致的配置。
  - 匹配条件要尽量窄，避免覆盖用户自定义配置。
- `src/app/PersistedRuntimeConfig.cpp`
  - 只从持久化副本里移除 session-only 模式。

常见错误是菜单循环已经更新了，但持久化或迁移没有更新。这样看起来能用一次，但下次启动又会回退。

## 回放、结果与 Records 影响

模式变更必须反映到保存文件和结果/记录界面中。

- `src/gameplay/Replay.cpp` / `src/gameplay/Replay.h`
  - `mode`、`raw_score`、`final_score`、`rate_multiplier`、`score_multiplier` 等 JSON 字段要保持一致。
- `src/app/GameSession.cpp`
  - 回合结束时要把 `ModeManager` 的最终结果写入回放/结果元数据。
- `src/app/MenuRecordUtils.cpp`
  - 负责解析已保存的结果和回放文件，供 `Records` 页面和结果详情面板显示。
- `src/app/MenuAppTail.inl`
  - 结果页的文案、分数倍率、mod 总结、replay/result 路径标签都在这里渲染。

如果你改了回放或结果序列化，一定要立刻更新 `tests/unit/test_replay_export.cpp`。这些文件往往还会被别的代码路径读取，所以字段名和默认值最好保持稳定，除非你明确要做迁移。

## 测试与文档同步

新模式相关改动如果跳过测试，后面很容易在 UI 或保存层炸掉。

- `tests/unit/test_mode_applier.cpp`
  - 验证 key-mode 转换、random 确定性、hold 元数据保留。
- `tests/unit/test_mode_manager.cpp`
  - 验证 mod 注册、分类冲突、判定窗口缩放、分数倍率。
- `tests/unit/test_config.cpp`
  - 验证保存/读取 round-trip、大小写归一化、默认值迁移。
- `tests/unit/test_replay_export.cpp`
  - 验证 replay/result JSON 字段和恢复行为。
- `tests/smoke/bms_mode_smoke.cpp`
  - 检查真实 BMS 谱面下 key mode、random、mod 和已知 lane-remap 组合是否正常。

文档同步通常按这个顺序想：[`docs/current-state.zh-CN.md`](current-state.zh-CN.md)、[`docs/config.zh-CN.md`](config.zh-CN.md)、[`docs/README.zh-CN.md`](README.zh-CN.md)，然后再是功能专项文档。本轮只新增开发者指南，所以如果之后需要更新代码行为描述，再单独做一次 current-state 文档更新会更稳。

如果这次工作还会刷新公开源代码包，就还要再多做一步。重新 staging `opensource-Tenriff-source/TenRiff-<version>-source` 之后，要在那个目录本身里按“没有 `tools/`、`10k-calc/`、现成 `profiles/` 这类 repo 专用辅助内容”的前提，实际验证原生 `cmake` configure/build，并至少运行一次 `bms_parser_tests`。

## 常见坑

- 只在 `ModeSettings` 里加 token，却忘了 `ModeResolver` 和菜单 UI。
- 已经把 mod 放进注册表，但 `score_multiplier` 或分类信息没补全。
- 忘了在 `PersistedRuntimeConfig.cpp` 里剔除 session-only mod。
- 把所有配置都迁移了，而不是只迁移那些和旧默认值完全一致的配置。
- 改了 replay/result 字段，却没同步 `MenuRecordUtils.cpp` 和 `tests/unit/test_replay_export.cpp`。
- 把 `none`、`auto`、大小写变体当成不同语义值，而它们本来是 alias。
- 新增 key mode 后，忘了同步 keymap、replay、result 的 lane count 路径。

## 验证清单

- `ModeSettings` 的 enum/parse/to_string 可以 round-trip。
- `ModeResolver` 对无效 token 会报警，并安全回退到默认值。
- `ModeManager` 会把新 mod 注册到正确的分类和倍率上。
- `ModeApplier` 会确定性地应用新的 key mode 和 random 规则。
- 需要时，`MenuApp` 的 `Mode Settings` 或 `Mod Manager` 会显示新行。
- `Config` 保存/读取与 `RuntimeConfigMigration` 不会破坏旧用户配置。
- Replay/result JSON 包含新的模式信息。
- `tests/unit` 与 `tests/smoke` 仍然全部通过。
- 如果行为会被用户直接看到，再在单独的文档更新里同步 [`docs/current-state.zh-CN.md`](current-state.zh-CN.md) 和 [`docs/config.zh-CN.md`](config.zh-CN.md)。
- 如果刷新了公开源代码包，还要在 `opensource-Tenriff-source/TenRiff-<version>-source` 目录里验证 standalone `cmake` configure/build 和 `bms_parser_tests` 的实际运行。
