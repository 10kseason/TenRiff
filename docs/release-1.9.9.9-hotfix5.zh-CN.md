# TenRiff 1.9.9.9-hotfix5

本版完整继承hotfix4的击打动画、特效设置、多核扫描及缓存加载。本次运行时修复仅校正发送到网站的原始键位／转换模式标记。

原始键位模式（`auto`/`none`/`native`）的新Online游玩，无论记住的KRRCREAM/NK2/NK3设置是什么，均发送`key_conversion: none`。明确指定转换目标键数时，即使输出键数相同也保留转换标记。模式缺失或未知的记录不会被推断为原始键位。

运营网站v25的曲目列表、排名、个人最佳、曲目数、玩家数和记录数只统计原始键位游玩。旧客户端在原始模式下也发送转换器名称，因此现有服务器信息无法确定实际是否转换。按用户选择，这些分类不明的记录也排除出排名。保存数据保持不变，不重新分类、删除或换条件重新提交。新原始键位排名请使用hotfix5。

将运行ZIP解压到新文件夹并启动`launch_win.bat`。将转换目标设为原始／None，通过F10设置Online及网站连接，然后开始新游玩。保留已有安装与配置。转换游玩的本地分数和回放保存保持不变。

Scoring, judgement, replay format and R13/R14 policies remain unchanged. Fixed baseline: 1.9.6.

[Validation](release-1.9.9.9-hotfix5-gate.md)
