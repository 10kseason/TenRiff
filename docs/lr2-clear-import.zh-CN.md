# 导入 LR2 / LR2oraja 记录

通过同一按钮选择LR2oraja的`player/<档案>/score.db`。以谱面的SHA-256匹配，按LN、CN、HCN模式将通关类型、EX分数、最大连击和最小BP保存到`<档案>/lr2oraja_import.json`。选曲界面优先显示LN，其次为CN、HCN，并标注模式。Fast索引会对当前选择的谱面计算一次哈希进行匹配。

EX等于原始PG总数的两倍加GR总数。通关、连击和最小BP可能来自不同次游玩的个人最佳。要恢复结果的毫秒分布，还需要含实际输入时间的回放。DB中的ghost保存每个音符的判定类别。 [LR2oraja存储格式](https://github.com/wcko87/lr2oraja/blob/lr2oraja/src/bms/player/beatoraja/ScoreData.java)

源数据库以只读方式访问，成功导入后更新同一来源的记录。课程、特殊模式以及无法识别的行会跳过。若数据库中存在待处理的WAL帧，请关闭使用它的游戏后重试。

## LR2 通关灯

把 LR2 成绩数据库中的通关灯导入 TenRiff 档案,并显示在选曲界面。仅用于显示:导入的灯不会成为 TenRiff 成绩、BEST 或在线提交对象。

EX 检查读取音符、RANK、TOTAL、LN、地雷和 RANDOM 信息，不查找音频或背景文件。导入只计算 LR2 所需的 MD5，避免重复遍历重叠文件夹，并在找到全部目标谱面后停止。灯标转换和保守的 EX 认定条件保持不变。仍有未找到的谱面时会继续搜索其余文件夹，因此大型曲库仍可能耗时。 同时修复了原始 RANK 缺失或超出 0–3 时按 EASY 回退而错误授予 EX 的问题；这些谱面仅保留原始 LR2 灯标。

## 用法

### 在游戏中导入

关闭 LR2，打开 **选项 → 档案 → 导入成绩数据库**，选择成绩 `.db` 文件。通关灯保存到当前档案，并使用已注册及当前歌曲文件夹验证 EX。即使没有匹配的歌曲，也可导入基础灯。

导入在后台进行，再次选择相同按钮可取消。完成后无需重启即可刷新选曲灯。重新导入会替换原有 LR2 灯列表；取消、读取或保存失败、关闭游戏时保留原列表。TenRiff 成绩、BEST 和在线提交资格不变。

### 命令行

```
tenriff-lr2-import.exe <LR2 成绩 .db> <档案文件夹> [歌曲文件夹 ...]
```

- LR2 数据库以只读方式打开。
- 输出为 `<档案文件夹>\lr2_import.json`。删除该文件即可移除导入的灯。再次运行会重新生成。

## 显示规则

| LR2 | TenRiff 灯 |
|---|---|
| EASY | EZ CLEAR |
| NORMAL (GROOVE) | NM CLEAR |
| HARD、FULL COMBO | HD CLEAR |
| 通过下述检查的游玩 | EX CLEAR |

- 如果 TenRiff 中已有相同或更高的通关记录,则显示该记录。
- 显示导入的灯时,所选歌曲的灯文字前会加上 `LR2 `;LR2 全连会加上 `(FC)`。
- 不导入分数、评级和准确率。

## EX 检查

指定歌曲文件夹后,会把 LR2 的 `ghost`(最高分游玩的逐音符判定顺序)放入 TenRiff 的 EX-HARD 血槽重放。每个 LR2 判定都替换为**该 LR2 判定窗口内 TenRiff Normal 判定可能出现的最差判定**。仍然存活的游玩记为 EX。

- LR2 判定窗口(ms,RANK VERY HARD/HARD/NORMAL/EASY):PGREAT 8/15/18/21,GREAT 27/32/40/60,GOOD 40/60/100/120,BAD 200。PGREAT 与 BAD 在 lr2oraja 的 LR2 判定表和 Lunatic Vibes 中一致,GREAT/GOOD 相差几毫秒,取较宽的值。
- 血槽与实际游玩使用同一个 `GaugeManager`(基于 BMS TOTAL 的 EX-HARD)。

不检查的情况(只导入 LR2 的灯):歌曲文件夹中找不到谱面、含长条/地雷/`#RANDOM` 的谱面、`#RANK` 缺失或超出范围、音符数与 LR2 记录不同、ghost 缺失、未到结尾或与保存的判定数不一致。

## 限制

- ghost 格式并非来自规范,而是根据一个真实的 LR2 数据库反推得到(5,285 行中有 5,274 行判定数一致)。
- ghost 属于最高**分数**的那次游玩,可能与获得通关灯的那次不同。
- 最差情况规则会拒绝一些实际上能够存活的游玩。
- LR2 与 TenRiff 的血槽规则不同,同名的灯难度并不相同。
