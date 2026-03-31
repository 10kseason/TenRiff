# TenRiff 本地化指南（当前）

Language: [한국어](localization.md) | [English](localization.en.md) | 简体中文 | [日本語](localization.ja.md)

这份文档整理 TenRiff 当前的 UI 本地化结构，并给以后新增更多语言时提供一个可直接查阅的参考，避免再次从零寻找相关文件和边界。

## 当前模型
- 当前官方 UI 语言为 `en` 和 `ko`。
- 配置键为 `ui.language`，默认值是 `en`。
- 非法语言 token 在加载配置时会被规范化为 `en`。
- 在 Graphics Settings 中切换语言会立即反映到菜单 UI，并在保存后持久化。
- 当前本地化覆盖范围主要是菜单、设置、帮助、选歌、结果画面与 gameplay HUD 文本。
- 歌曲标题、作者名以及部分保存到 replay/result 中的状态文本属于数据本身，不做翻译，只做显示安全处理。

## 主要边界
1. 配置保存/加载与语言 token 规范化
   - `src/config/Config.h`
   - `src/config/Config.cpp`
2. 应用层字符串选择辅助函数
   - `src/app/MenuApp.h`
   - `src/app/MenuApp.cpp`
3. 将语言状态传入渲染快照
   - `src/app/MenuAppTail.inl`
   - `src/render/MenuWindow.h`
4. 应用层菜单字符串生成
   - `src/app/MenuAppDeviceSettings.cpp`
   - `src/app/MenuAppSettings.cpp`
   - `src/app/MenuAppKeymap.cpp`
   - `src/app/MenuAppSkin.cpp`
   - `src/app/MenuAppSongSelectRender.cpp`
   - `src/app/MenuAppTail.inl`
5. 渲染器侧硬编码 UI 文本
   - `src/render/MenuWindow_draw.inl`
   - `src/render/MenuWindow_draw_title_body.inl`
   - `src/render/MenuWindow_draw_generic_body.inl`
   - `src/render/MenuWindow_draw_songselect_body.inl`
   - `src/render/MenuWindow_draw_result_body.inl`
   - `src/render/MenuWindow_draw_gameplay_body.inl`

## 当前约定
- 在应用逻辑里直接生成文本时，使用 `ui_text("English", "한국어")`。
- 对于反复出现的 token 型标签，使用专用标签辅助函数。
  - 例如：`ui_on_off`、`ui_language_label`、`ui_gauge_label`、`ui_random_label`
- 对于只在渲染器里使用的固定文本，跟随 `MenuWindow::draw(...)` 中的 `loc(...)` 与 `wloc(...)`。
- 不要翻译用户数据或谱面元数据，只通过 `sanitize_ui_text(...)` 保证安全显示。
- 持久化配置值要和显示标签分离。
  - 例如：保存值是 `hard`，显示时再映射为 `Hard` 或 `하드`

## 新增或修改 UI 文本时
1. 如果文本只存在于应用状态、设置或帮助逻辑里，就在 `MenuApp*` 文件中通过 `ui_text(...)` 添加。
2. 如果文本只存在于渲染代码里，就在 `MenuWindow_draw*.inl` 中通过 `loc(...)` 或 `wloc(...)` 添加。
3. 如果它属于重复值，如 `on/off`、gauge、random、display mode，不要到处复制成对文本，而是扩展标签辅助函数。
4. 不要本地化持久化配置值本身，只新增把保存 token 映射为显示标签的函数。
5. 不要翻译谱面元数据、文件名或 replay/result 路径。

## 新增语言的推荐流程
- 当前结构是围绕 `英语/韩语` 的 pair 形式。进入第三种语言后，与其继续堆叠 pair helper，不如同时把结构做通用化。

1. 在配置规范化逻辑中加入新语言 token。
2. 保持 `ui.language` 的保存/加载与 migration 行为稳定，同时接受新 token。
3. 把 `render.ui_korean` 这类 bool 传递改成语言 enum 或语言 token。
4. 把 pair helper 提升为真正的多语言查找模块。
   - 建议位置：`src/ui/Localization.h`、`src/ui/Localization.cpp`
5. 把重复标签迁移到这个基于 token 的 API 中。
6. 全面检查 `MenuApp*` 和 `MenuWindow_draw*` 文件中剩余的硬编码文本。
7. 同步更新文档和测试。

## 推荐的未来 token 结构
- 持久化值继续保持为短而稳定的 token。
  - 例如：`en`、`ko`、`hard`、`normal`、`easy`
- 显示字符串从 token 表中解析。
- 同一个 token 的多语言文本放在同一个位置集中管理。
- 尽量让各个画面文件请求 token，而不是直接持有字面文案。

示例：

```cpp
enum class UiTextId {
    Back,
    Save,
    Language,
    GaugeHard,
};

std::string localized_text(Language lang, UiTextId id);
```

## 最低回归检查清单
- `config save and load preserve ui language setting`
- `config load normalizes invalid ui language to english`
- 确认 Graphics Settings 里的 Language 行能立即刷新 UI
- 确认 Help overlay、Song Select、Result、Gameplay loading/countdown 不会混用语言
- 确认 keymap 保存/失败提示、result 重开/回放提示、song browser 提示会一起切换
- 按 `docs/ui-audit-checklist.md` 做 `720p`、`1080p`、`Performance HUD on/off` 手动检查

## 已知限制
- 当前实现还不是通用 i18n 系统，而是 `en/ko` 分支式结构。
- 一些已保存的 result/replay 元数据状态字符串仍可能保持英文。
- 一些 fallback placeholder 字符串也可能仍是英文。
- 如果要正式扩展到第三种语言，应该先把 bool 传递和 pair helper 做通用化。
