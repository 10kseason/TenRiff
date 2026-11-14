# TenRiff 2.0.0

[한국어](release-2.0.0.ko.md) | [English](release-2.0.0.en.md) | [日本語](release-2.0.0.ja.md) | [简体中文](release-2.0.0.zh-CN.md)

整合EX Score（PG×2 + GR）、账号关联托管多人游戏、星座主页/选曲/选项、已验证ALL COMBO显示，以及房间聊天、资料库与Session Mix修复。

- **计分：**EX Score=P-Great×2 + Great，Good、Bad、Poor均为0分。显示、个人最佳、Ghost、Session Mix、多人游戏和Web排名统一使用EX，不应用模式/Rate倍率。准确率、等级、血条和判定规则仍为独立指标。保留旧回放验证分数，不推测缺失判定数；相应Web记录保留但不进入EX排名。
- **在线房间：**通过F10关联现有排行榜账号后创建或加入官方房间，无需已有排行榜成绩。最多8人共享选曲、准备、预约开始、分数及聊天，房主退出时转移权限。每台电脑自行准备相同谱面，不上传谱面、音频或完整曲库。
- **界面：**整合星座主页、选曲和选项及静态银河背景。主页标语严格为**Tenriff BMS Rhythm Game Client**；仅移除主视觉下方副标题，保留操作指南。选项保留8个分类的4×2卡片和原有操作。选项银河原图不变，仅显示时应用0.92饱和度、0.98亮度并保留透明度。
- **记录呈现：**仅已验证全连显示**ALL COMBO**。最佳通关灯与最佳EX记录分别保留，整理长曲名/作者名、静态缩略图、指针光照和减少动态效果路径。EX银河徽章与通关渐变是独立于选项背景的素材。
- **修复：**房间输入检查已连接的PeerSession，不再依赖未配置的全局聊天。保留F11、Enter及256字节上限。添加文件夹仅出现在Library；修复Session Mix生成、重组、保存、导入与取消操作。

新房间要求hosted protocol 2和`ex-v1`，Web上传要求schema 2。保留旧规则集/回放与直接TCP PeerProtocol v9实现，但旧客户端不能混入新房间。记录仍为**client_submitted**，不保证服务器回放验证或反作弊。游客及来源不明旧记录不会转为可上传记录。

最终2.0.0构建、独立源码ZIP构建、包检查、CI和公开资产的实际验证结果记录于验证关卡，不将旧候选测试次数作为本次发布证据。真实输入法、物理输入、试听、真实账号对局、长时间、其他电脑和性能/成本须另行验证。

兼容性基准仍为**1.9.9.9-final4**。**2.0.0之后仅修复缺陷，不再添加新功能。** 验证及发布状态以[验证关卡](release-2.0.0-gate.md)为准。下方旧版本说明属于历史记录。

[EX Score](ex-score.zh-CN.md) · [Multiplayer](hosted-multiplayer.zh-CN.md) · [ALL COMBO / Song Select](menu-starlight.zh-CN.md) · [Options / Chat](options-chat.zh-CN.md) · [Library / Session Mix](library-session-mix-fix-20261114.zh-CN.md)
