# TenRiff 文档地图

Language: [한국어](README.md) | [English](README.en.md) | [简体中文](README.zh-CN.md)

如果你已经先读过根目录的 [`README.zh-CN.md`](../README.zh-CN.md)，那么这份文档就是下一步的详细文档索引。这里同时包含设计文档与当前状态文档，所以想快速建立上下文时，最有效的阅读顺序如下。

## 推荐阅读顺序
1. [`docs/current-state.zh-CN.md`](current-state.zh-CN.md)
   - 当前产品状态、核心子系统、已验证命令和仍待手动验证的项目
2. [`docs/baseline-1.0.0.zh-CN.md`](baseline-1.0.0.zh-CN.md)
   - 规定当前工作应从哪里继续叠加的 `1.0.0` 基准文档
3. [`docs/gameplay-guide.zh-CN.md`](gameplay-guide.zh-CN.md)
   - 面向真实游玩的启动方式、选歌、操作、HUD、判定、结果说明
4. [`docs/config.zh-CN.md`](config.zh-CN.md)
   - 实际配置、profile 与 keymap 结构
5. [`docs/localization.zh-CN.md`](localization.zh-CN.md)
   - 当前英语/韩语 UI 结构，以及以后新增更多语言时需要修改的文件边界
6. [`docs/menu.zh-CN.md`](menu.zh-CN.md)
   - 菜单/状态机/选歌流程
7. [`docs/core-loop.zh-CN.md`](core-loop.zh-CN.md)
   - 播放循环与数据流
8. [`docs/roadmap.zh-CN.md`](roadmap.zh-CN.md)
   - 中长期工作方向
9. [`docs/developer-extension-guide.zh-CN.md`](developer-extension-guide.zh-CN.md)
   - 新增 mode/mod 的开发者指南，说明应该改哪些 C++ 文件

## 哪些文档算作权威来源
- [`docs/current-state.zh-CN.md`](current-state.zh-CN.md)
  - 当前实现状态的总结文档
- [`docs/baseline-1.0.0.zh-CN.md`](baseline-1.0.0.zh-CN.md)
  - 后续工作必须保持的 `1.0.0` 基准文档
- [`docs/config.zh-CN.md`](config.zh-CN.md)
  - 以实际的 `config/config.json`、`profiles/<name>/config.json`、`keymap.json` 为准

## 历史/设计文档
- [`docs/menu.zh-CN.md`](menu.zh-CN.md)
  - 菜单/状态机/低延迟方向性的设计文档
- [`docs/core-loop.zh-CN.md`](core-loop.zh-CN.md)
  - 播放循环的初期设计与数据流说明
- [`docs/localization.zh-CN.md`](localization.zh-CN.md)
  - 当前 UI 本地化结构与未来新增语言时的参考文档
- [`docs/latency.zh-CN.md`](latency.zh-CN.md)、[`docs/modes.zh-CN.md`](modes.zh-CN.md)、[`docs/gap-analysis.zh-CN.md`](gap-analysis.zh-CN.md)、[`docs/roadmap.zh-CN.md`](roadmap.zh-CN.md)
  - 按功能拆分的设计、分析与中长期方向文档
- [`docs/developer-extension-guide.zh-CN.md`](developer-extension-guide.zh-CN.md)
  - 面向开发者的 mode/mod 扩展指南

## 实用规则
- 在确认当前行为时，优先看 [`docs/current-state.zh-CN.md`](current-state.zh-CN.md)。
- 在确认应当在哪个基准上继续叠加工作时，要同时看 [`docs/baseline-1.0.0.zh-CN.md`](baseline-1.0.0.zh-CN.md)。
- 由于较旧的设计文档和当前代码可能不同，若出现冲突，请优先按当前代码、[`docs/current-state.zh-CN.md`](current-state.zh-CN.md)、[`docs/config.zh-CN.md`](config.zh-CN.md) 的顺序解释。
