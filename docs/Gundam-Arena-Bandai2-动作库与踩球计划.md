# Gundam Arena：Bandai 2 动作库计划

状态：**2026-09-17 用户放弃踩球，玩法改回踢球。** 工作流 B（站球移动）整线取消，不再实现、不再烧录、不再从本文件排期。Bandai 2 的 10+10 动作库（工作流 A）继续。分支：`feat/gundam-arena`。

球的默认玩法见 [Gundam Arena 技术文档](Gundam-Arena-技术文档.md) §3.1 / §6.4：脚前半径 `0.16` 的小球，A 短按播 Kick clip，脚向前碰到才击飞。

---

## 0. 决策

| 曾写过 | 现状 |
|---|---|
| 不要踢球，要像 microduck-basketball 站在球顶上滚 | **已放弃。** 运行时已从踢球提交 `e09ef64` 恢复。 |
| P0 站球（加大球、开局在顶、输入滚走、关 Kick） | **取消。** P0.1–P0.5 主机实现作废，不保留 `onBall` 路径。 |
| P0.6 真机滚走、P3 球上踏步皮肤 | **取消。** |
| Kick 只当实验、默认不踢飞 | **作废。** Kick 重新是默认球玩法。 |
| Bandai 2 筛 10 种 + 10 种备选，不要武器/道具 | **仍有效。** P1.1 已完成。 |

曾对齐过的 [microduck-basketball](https://huggingface.co/HannesVonEssen/microduck-basketball) 只作历史参考，不要再往 Arena 搬站球约束或 PPO。

---

## 1. 只剩一条工作流

```
A. Bandai 2 动作库     P1 索引 → P2 转换预览 → P4 手势进设备 → P5 风格
```

Bandai 2 没有踢球、没有站球、没有道具动作。踢球继续用集 1 已烘焙的 `arena_kick_clip.h`（§13 已完成）。走/跳仍是程序 IK，不拿集 2 的 walk/run 替换。Play 下 A/B **循环**圈内动作（Kick / Jump / 六手势），不把键绑死在踢或跳上。

**§6.5 仍适用。** 人体配平 1:1 写进 SD = 后仰和多余挥舞。

---

## 2. 工作流 A：Bandai 2 的 10 + 10

数据集 2 官方：2,902 条、10 内容 × 7 风格、30 fps。README Contents 把 `raise-up-right-hand` 写了两次；**权威是 cfg**：第 9 类 `raise-up-left-hand`，第 10 类 `raise-up-right-hand`。拉取：`python3 tools/arena_motion/fetch_bandai2_index.py`（只下标签，不下 BVH）。

不依赖武器或其它物品——集 2 这 10 类本来就都是空手，全部入围。

### 2.1 首批 10 种 = 集 2 全部内容（不是挑 10/N）

`normal` 为第一轮风格。P5 已打分：WAVE L/R 换成 `active`；双手挥/双手举的风格备选被拒。walk/run/turn 仍不进固件。

| 顺序 | 官方内容 | 首批用途 | 运行时 |
|---|---|---|---|
| 1 | `walk` | 地面走的节奏对照 | 根位移仍输入；脚 IK。不替换现程序走 |
| 2 | `run` | 更快节奏 / 前倾参考 | 幅度走 §6.5，禁止人体前倾变后仰 |
| 3 | `walk-turn-left` | 离线对照转弯落脚 | **不进固件、不写 heading** |
| 4 | `walk-turn-right` | 同上；核 L/R 与 `-X` | 同上 |
| 5 | `wave-left-hand` | 左手挥手 | 圈槽 2；P5 用 `active` |
| 6 | `wave-right-hand` | 右手挥手 | 圈槽 3；P5 用 `active` |
| 7 | `wave-both-hands` | 双手挥手 | 圈槽 4；保留 `normal` |
| 8 | `raise-up-left-hand` | 左手举手 | 圈槽 5；`normal`（无风格备选） |
| 9 | `raise-up-right-hand` | 右手举手 | 圈槽 6；`normal`（无风格备选） |
| 10 | `raise-up-both-hands` | 双手举起 | 圈槽 7；保留 `normal` |

没有蹲、跳、倒、起、出拳、踢球、站球。那些不能写进集 2 标签。若以后要更多**种类**，另开数据集 1 / 程序动作，不在本文件假装集 2 有第 11 种。

### 2.2 备选 10 种 = 风格变体（不是新种类）

这是对上面类别的第二套 take。先 `active` / `youthful`，再按足底、限位、SD 剪影淘汰。

| 顺序 | 类别 × 风格 | 淘汰 |
|---|---|---|
| B1 | `walk` × `active` | 仅离线；未替换 IK |
| B2 | `walk` × `youthful` | 仅离线；预览朝向约 180° 翻面 |
| B3 | `run` × `active` | 仅离线；钳位 34/1080，高于 normal |
| B4 | `run` × `youthful` | 仅离线；朝向翻面 |
| B5 | `walk-turn-left` × `active` | 支撑脚滑；仅离线 |
| B6 | `walk-turn-right` × `active` | 左右不对称；仅离线 |
| B7 | `wave-left-hand` × `active` | **未淘汰**，P5 进固件 |
| B8 | `wave-right-hand` × `active` | **未淘汰**，P5 进固件 |
| B9 | `wave-both-hands` × `youthful` | 淘汰：双臂同时打满上限 |
| B10 | `raise-up-both-hands` × `active` | 淘汰：双臂同时打满上限 |

`elderly` / `exhausted` / `feminine` / `masculine` 可离线对照，不进 UI。

### 2.3 任务

| 任务 | 做什么 | 完成标准 | 状态 |
|---|---|---|---|
| P1.1 | 拉 cfg 标签 | 第 10 类歧义消除 | 完成：cfg 第 9 类 `raise-up-left-hand`，第 10 类 `raise-up-right-hand` |
| P1.2 | 只下 10+10 条 BVH + LICENSE 到 `raw/`（gitignore） | 路径、内容、风格、ID、帧数、fps、sha256 | 完成：`fetch_bandai2.py` → `dataset-2_manifest.json` |
| P1.3 | 抽样对照集 1 的 22 骨 / ZXY | 一致才复用解析；否则适配层，不改 19 骨 | 完成：一致，复用 `retarget_bandai.py` FK |
| P2.1 | 通用化 `retarget_bandai.py`（集 1 kick 仍能烤，且踢球是玩法） | 中间格式离线用；固件只读精简数组 | 完成：六手势 `arena_gesture_clips.h` |
| P2.2 | walk / run 预览 + 限位报告 | 循环窗、接触帧、脚滑/穿模记录 | 完成短报告：`dataset2_preview.txt`（不进固件） |
| P2.3 | turn 预览 | 仅对照 | 完成短报告：同上 |
| P4 | 六类空手手势进设备 | 站立地面播放；短入/出；头肩不穿 | 完成主机：A/B 循环圈，空中不切条 |
| P5 | 更好的风格变体 | 每段 flash / 帧时是测量值 | 完成主机：WAVE L/R=`active`，其余 `normal`；每段 10800 B，主机 716–759 µs。未烧录 |

不要把 2,902 条整包推进仓库。现踢球数组约 9.5 KiB；不要 20 段同结构默认进固件。

重定向：腿用 Arena 双骨 IK；上身按 SD 放大或收；`Spine` 合入胸/骨盆；禁止人体角 1:1。

~~P3 球上踏步皮肤~~：**取消**（依赖已放弃的站球）。

---

## 3. 工作流 B：站球（已取消）

不要再做：

- 加大球、开局站顶、输入滚球、球面 IK
- 关掉 Kick 冲量、站球忽略 A/B
- 球上踏步皮肤、球面网格、滚动条纹
- 移植 microduck ONNX/PPO、脚前盘带

历史：2026-09-17 曾在脏工作区做过 P0 主机实现并烧过一版站球固件；同日按用户要求回退到踢球。细节只留技术文档 §12，不作为后续任务。

---

## 4. 阶段门

| 阶段 | 交付 | 关门 | 依赖 |
|---|---|---|---|
| P0 站球 | — | **取消** | — |
| P1 | 10+10 真实清单 | 路径/ID/哈希；标签歧义消除；骨架对照进技术文档 | 无站球 |
| P2 | 离线转换与预览 | walk/run（及 turn 对照）有报告 | P1 |
| P3 球上踏步 | — | **取消** | — |
| P4 | 六类空手手势 | 入/出、无头肩穿模 | P2 |
| P5 | 风格与体积 | 只留更好的；有 flash/帧时 | P4 |

P1.2–P5 已完成主机侧（2026-09-17）。2026-09-17 已烧入 A/B 循环 + 六手势固件（`0x3f56f0`，未测设备 FPS）。用户真机观感待反馈。walk/run 仍不替换程序 IK。代码改动写入技术文档 §3 / §6.4.2 / §12。踢球计划本身见技术文档 §13（已完成）。
