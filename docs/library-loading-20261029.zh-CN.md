# 曲目列表、文件夹与难度表加载改进

此本地候选完整继承1.9.9.5指示条与结算候选。本次不涉及选曲后的游玩准备。公开版本与固定基线1.9.6保持不变。

- 切换难度表时，直接对已加载曲目的哈希应用新表，不再重新读取完整曲目缓存、成绩及回放验证。相同URL的新内容仍会重新获取。单个文件夹和ALL SONG均支持。
- Fast转Safe、空列表、扫描进行中或扫描结果尚未交付时，保留原有完整更新流程。F5和文件夹刷新仍执行验证。外部修改的曲目/成绩文件需要显式刷新。
- 统计文件数量时跳过重复的路径解析、大小与修改时间查询；实际读取仍保留路径处理、权限错误、取消、批次内存限制及元数据验证。
- 缓存解析结果改为移动而非复制；保留选曲位置时读取可见索引，无需复制所有曲目。排序、筛选和选中路径规则不变。

文件格式、分数、判定、输入、音频、回放/在线协议和模型均不变。使用独立library-loading-local配置，保留旧候选与安装。

测量使用2,000个合成BMS/200文件夹及10,000项缓存、本地HTTP响应和空成绩列表，不清除OS文件缓存。这不代表真实曲库、冷磁盘、网络延迟、长期使用或其他电脑的表现。结果与初始失败记录位于候选的evidence/VERIFICATION.ko.md及benchmark-comparison.json。

## Benchmark

Build the explicit `library_loading_benchmark` target, then run `build/Release/library_loading_benchmark.exe <new-output-directory>`. It refuses existing output paths and uses no real profile or network. The synthetic 10,000-entry cache includes copied metadata entries whose extra paths do not exist; only the first 2,000 charts are used to benchmark filesystem scans. This tool is excluded from default builds and end-user packages.

## 重复测量中位数

| 阶段 | 之前(ms) | 修改后(ms) | 缩短比例 |
|---|---:|---:|---:|
| 文件夹扫描(2,000曲) | 1774.08 | 1320.67 | 25.6% |
| 打开已缓存列表(10,000项) | 233.99 | 230.95 | 1.3% |
| 应用难度表(10,000项) | 382.94 | 103.05 | 73.1% |
| 返回基本LV(10,000项) | 283.67 | 20.68 | 92.7% |

依次运行3组前后对比，第2组反转顺序。扫描/列表各9个样本，难度表/基本LV各15个样本。缓存列表打开时间仅差1.3%，处于运行波动范围内，不宣称有明显改善。
