# Gundam Arena：RX-78 待机智能系统设计

> 状态：**草案修订。§13 动作层已落地。Auton 四刀已落地（主机测试）；已烧录 `c3c18d8`，真机观感待用户看。**
>
> 本文只描述待机智能的运行约定。已落地的沙盒行为仍以 [Gundam Arena 技术文档](Gundam-Arena-技术文档.md) 为准。两者冲突时，以技术文档中的代码事实为准，本文再改。
>
> **2026-09-17 审核已决：** Jump 交给智能；玩家 A/B 圈指针与智能请求分开；回 Pilot 时头立刻回正。§12 数值与门闩用户确认保留。
>
> **实现顺序：** Auton 四刀已做主机测试。真机观感待用户看。

---

## 0. 审核范围

产品行为（2026-09-17 已决）：

1. **你在开时，它就是现在的沙盒。** 摇杆走/转，A/B 循环 clip，Pose 摆关节，行为不变。
2. **你松手后，它自己看球、走到能踢的位置、转正再踢；踢完先看结果。** 偶尔对镜头挥手或举手。
3. **你一推杆，立刻把机体交还给你。** 正在播的 Kick / Gesture / Jump 可被着地输入打断，规则与现在 Play 相同。
4. **Jump 也交给智能。** Auton 可请求现有跳跃马达（地面下蹲蓄力 → 起跳 → 落地）；玩家 A/B 圈里的 Jump 槽仍在，互不改对方的圈指针。

实现不得偏离这四句。数值和门闩可在 §12 改，人设不能改。

---

## 1. 这是什么

在 Play 模式增加一层 **松杆后的待机计算机**：RX-78 仍是沙盘里的机体，不是对战 AI，也不是展品浏览。

控制权只有两种：

| 模式 | 谁出移动和 clip | 何时进入 |
|---|---|---|
| **Pilot** | 玩家。与当前 `stepCharacter` 相同 | 默认；摇杆过死区；着地 A/B 切 clip |
| **Auton** | 待机智能生成内部前进/转向，并按技能请求 clip | Play、着地、摇杆连续回中超过 `kAutonDelay` |

Pose 不进入 Auton。切 Pose 时清掉待机意图，回到现在的关节编辑。

人设约束：

- 关心场上那颗球，以及刚松手的操作者。
- 先看、再转、再走近、够得着才踢。禁止隔空踢。
- 球在附近空中、地面踢不到时，可以自己跳一下跟着看；落地前不换技能。
- 不追出活动半径。

---

## 2. 运行分层

每帧（现有固定 `dt = 1/60`）只多做一件事：在 `stepCharacter` **之前** 决定这一帧的 `ArenaInput` 从哪来。

```
触屏 / 机身键 / 外设
        │
        ▼
  判定 Pilot 或 Auton
        │
        ├─ Pilot：原样使用玩家输入
        └─ Auton：感知 → 动机竞争 → 技能 → 合成内部 ArenaInput
        │
        ▼
  stepCharacter（现有马达：走、转、踢、手势、跳、落地、球）
        │
        ▼
  渲染（现有光栅；HUD 可加一行技能名，审核后再定）
```

智能层 **不** 解 IK、不播 clip、不改蒙皮。马达层仍是现在的 `Action`：

`Idle, Walk, Turn, Jump, Fall, Land, Kick, Gesture`

智能层输出的是意图，再翻译成现有输入：

- 内部 `forward` / `turn`（死区与玩家相同：0.18）
- 直接请求某个技能槽：Kick、Jump 或某一手势；**禁止** `clipStep=±1`
- 注视目标（头/颈，马达层今天没有，见 §6）

Fall / Land 仍由跳跃物理强制。Auton 可以请求 Jump，请求之后的空中与落地走现有马达，技能层视为 `busy`。

---

## 3. 控制权

### 3.1 进入 Auton

同时满足：

- `mode == Play`
- `grounded == true`
- 当前不在 Jump 蓄力（地面蹲）
- `|forward| ≤ 0.18` 且 `|turn| ≤ 0.18` 连续 `kAutonDelay` 秒

建议初值：`kAutonDelay = 0.80s`。审核可改，不要改成 0（松手立刻抢方向会和点摇杆冲突）。

### 3.2 立刻回到 Pilot

任一条件：

- `|forward| > 0.18` 或 `|turn| > 0.18`
- 着地且玩家产生 `clipStep`（A/B）
- 切入 Pose
- 输入 `valid == false` 且按现有规则轴已清零——**轴清零本身不回 Pilot**；只有「曾经 live 又再次过死区」才回。失联中性轴应保持 Auton，避免外设掉线后机体冻住。

回到 Pilot 时：

- 丢弃 `seekXZ`、当前动机胜出、内部轴
- `lookAt` **立刻**清零，头/颈叠加立刻去掉（已决：立刻回正）
- 若正在 Kick / Gesture / Land / 蓄力跳：打断规则与现在 Play 相同（着地可打断，空中忽略 A/B；空中推杆仍按现规则给 45% 前进）

### 3.3 `busy`

下列情况技能层不得换条（可继续更新注视）：

- `Action::Kick` / `Gesture`
- `Action::Jump`（含地面蓄力）/ `Fall` / `Land`
- 当前技能未满最短持续时间（§5.3）

`busy` 时仍允许被 Pilot 抢回。

---

## 4. 感知

每帧从 `CharacterModel` 与球刚体计算，不存历史图像。需要保留的字段见 §8。

机体前方是 `heading=0` 的 +Z，左侧是 −X。踢球 clip 的接触脚是左脚，站位沿用现在的球出生局部偏移。

### 4.1 球

以骨盆世界 xz 为原点、机体朝向为参考：

| 量 | 定义 |
|---|---|
| `ballDist` | 骨盆 xz 到球 xz 的水平距离 |
| `ballBearing` | 球相对朝向的方位角，钳到 (−π, π]；0 为正前方 |
| `facingErr` | 等于 `ballBearing`（先转正再走近；走近时也用来给转向轴） |
| `ballSpeed` | 球水平速度模 |
| `ballAir` | `ball.y > kBallR + 0.02` |
| `ballStruckRecent` | 本段 Kick 内 `ball.struck` 曾为真，或球速从高变低后的短窗口（建议 1.2s） |

踢球站位 `kickStance`：把局部偏移 `(kBallSpawnX, kBallSpawnZ) = (-0.42, 0.74)` 当作「球应在左脚前」的几何，反求机体根应站的世界 xz。即：

- 机体站在 `kickStance` 时，球的水平位置落在左脚踢球点附近
- 不是走向球心；走向球心会踢空或踩球

`inStrikeRange` 同时成立才允许 Strike：

1. 根 xz 到 **当前朝向** 的 `kickStance` 的距离 `< kStrikePosTol`（0.05）。这等于：球在机体局部 xz 上靠近出生点 `(-0.42, 0.74)`。不得用「按 `kickHeading` 反求的理想站位」当距离——那会在朝向还偏着时把距离算成 0，原地空踢。
2. `|kickAlignErr| < kStrikeFaceTol`（0.05）
3. 没有未完成的 `walkTo` / `faceYaw`
4. 球在地面（`!ballAir`）
5. 不 `busy`

`kWalkArrive=0.03`、`kFaceArrive=0.04`：先走到快照站位，再转到快照朝向，未对准不得 Strike。空踢结束禁止立刻再 Strike，先重新走近。

数值可改，门闩本身不可删。否则 Auton 会复现「原地隔空踢」。

`inLeapTrigger` 同时成立才允许 Leap（自己跳）：

1. `grounded` 且不 `busy`
2. 非 `inStrikeRange`（地面够得着踢时，踢优先，不跳）
3. `ballAir`
4. `ballDist < kLeapDist`（4.0；踢飞后球飞得快，1.8 会立刻出圈）
5. 球在下落（`ball.vy < 0`）或刚被踢飞仍在升段但已离开脚（`ballStruckRecent`）
6. 距上一次 Land 结束 ≥ `kLeapCooldown`（建议 4s）

Leap 是「球在附近天上、踢不着时跳着看」，不是随机蹦。地面静止的球只走 Approach / Strike。

### 4.2 操作者

| 量 | 定义 |
|---|---|
| `operatorPresent` | 最近 `kOperatorMemory` 秒内出现过：摇杆过死区、A/B、或 Play 上半区环视 | 
| `cameraBearing` | 相机位置相对机体的方位角（由 `ArenaView` 的 look/orbit 反算） |

建议 `kOperatorMemory = 8s`。社交技能只在 `operatorPresent` 时允许，避免无人操作时对空挥手。

### 4.3 自身

| 量 | 定义 |
|---|---|
| `stickLive` | 本帧 `|forward|` 或 `|turn|` 过死区 |
| `idleAge` | 连续处于 Pilot 且杆回中、或已在 Auton 且技能为 Hold 的时间 |
| `busy` | §3.3 |

---

## 5. 动机

五维，均钳在 [0, 1]。不引入额外情绪名称。

| 驱动 | 上升 | 下降 |
|---|---|---|
| **vigilance** | `ballSpeed` 大，或球朝机体飞来 | 约 2s 半衰期 |
| **curiosity** | 球在动、刚踢飞、`idleAge` 较长且很久没看球 | 注视已对准且球几乎静止 |
| **play** | 球在地面且可接近，或球在附近空中 | Strike / Leap 结束都降；连续两次 Strike 再加冷却 |
| **social** | 刚进入 Auton 且 `operatorPresent` | Signal 播完进入冷却（建议 12s） |
| **composure** | Kick / Jump / Land 刚结束 | 约 4s 回到可玩 |

每帧对技能打分（不是对五维本身打分）。建议初值：

```
Attend    = 0.70*curiosity + 0.50*vigilance
Face      = Attend，且 |facingErr| > 0.35 时再 +0.25
Approach  = 0.80*play + 0.40*curiosity，且球在地面且 not inStrikeRange
Strike    = 0.95*play，且 inStrikeRange；否则 0
Leap      = 0.75*play + 0.45*curiosity + 0.20*vigilance，且 inLeapTrigger；否则 0
Signal    = 0.85*social - 0.50*play
Hold      = 0.60*composure + 0.25*(1-curiosity) + 0.20*(1-play)
```

侧抑制：

- Strike 或 Approach 高于 0.40 时，Signal 乘 0.4，Leap 乘 0.3
- Strike 可触发时 Leap 必须为 0（已由 `inLeapTrigger` 保证）
- Hold 高于 0.50 时，Approach / Strike / Leap 乘 0.7
- `busy` 时冻结胜出，不重新选举

平局噪声不超过 ±0.02。

### 5.1 胜出 → 技能

| 胜出 | 技能 | 可读行为 |
|---|---|---|
| Attend | Attend | 站住，注视球（若球几乎停且 operatorPresent，可注视相机） |
| Face | Face | 原地踏步转向球，注视球 |
| Approach | Approach | 走向 `kickStance`，边走边转正，注视球 |
| Strike | Strike | 请求 Kick（独立入口，不拨玩家圈） |
| Leap | Leap | 请求 Jump（独立入口，不拨玩家圈） |
| Signal | Signal | 请求手势（独立入口），注视相机 |
| Hold | Hold | 站住；若球在视野外则注视球，否则头回正 |

同一帧只允许一个技能。Approach 在已经 `inStrikeRange` 时应直接变成 Strike，不要在球边上闲逛。地面够踢时不得 Leap。

### 5.2 最短持续时间

| 技能 | 最短 |
|---|---|
| Attend | 1.2s |
| Face | 转到 `|facingErr| < 0.20` 或 1.5s（先到为准） |
| Approach | 0.8s（避免每帧重选站位） |
| Strike | 直到 Kick clip 结束 |
| Leap | 直到 Land 结束（含蓄力、空中、落地缓冲） |
| Signal | 直到 Gesture clip 结束 |
| Hold | 2.0s |

未到最短时间，新技能分必须高出当前 **0.25** 才允许抢。到期后高出 **0.08** 即可换。Strike / Leap / Signal 不受此抢占，只等各自马达结束或 Pilot 打断。

### 5.3 手势选用

Signal 不按 A/B 圈下一个槽，按理由选：

| 条件 | 槽 | HUD |
|---|---|---|
| 默认打招呼 | WAVE L 或 WAVE R（对相机左右手更近的一侧） | WAVE L / WAVE R |
| 刚踢中且 `ballSpeed` 仍较高 | UP 2 | UP 2 |

`WAVE 2` / 单手 UP 首版不由智能选用，仍留给玩家 A/B 圈。

---

## 6. 技能如何驱动现有马达

把技能写成对 `ArenaInput` + 注视的填充。玩家在 Pilot 时不走这张表。

| 技能 | `forward` | `turn` | clip | `lookAt` | 现有 Action |
|---|---|---|---|---|---|
| Hold | 0 | 0 | 无 | Ball（若 `|ballBearing| > 0.25`）否则 None | Idle |
| Attend | 0 | 0 | 无 | Ball；球静止且 operatorPresent 可为 Camera | Idle |
| Face | 0 | `clamp(facingErr / 0.6, -1, 1)` | 无 | Ball | Turn（过转向死区）或 Idle |
| Approach | 按到 `kickStance` 的前向分量，钳到 [0, 1]；若 `|facingErr| > 0.8` 则 forward=0 先转 | 同 Face | 无 | Ball | Walk 或 Turn |
| Strike | 0 | 0 | 请求 Kick | Ball | Kick |
| Leap | 0 | 0 | 请求 Jump | Ball（请求当帧记下；空中不叠头） | Jump → Fall → Land |
| Signal | 0 | 0 | 请求对应手势 | Camera | Gesture |

Approach 的转向优先：朝向误差大时先转后走，避免侧着走进踢球点。位移钳位仍是 `±16`。若 `kickStance` 会出界，改到最近合法点；若仍 `inStrikeRange` 失败，降为 Attend，**不得**原地 Kick。

### 6.1 注视（马达缺口）

今天 Idle 每帧把 `pose.anim` 清零再钉脚；走/转时头 yaw 只跟 `angularSpeed`。Auton 必须在马达写完姿态之后叠加头/颈 look-at，否则 Attend / Hold 在屏幕上等于没发生。

约定：

- 目标是世界点：球心，或相机位置
- 只改 `Head` yaw/pitch，必要时加一点 `Neck` yaw；限位用现有 `limitOf(Head/Neck)`
- Kick / Gesture clip 播放期间 **不** 覆盖 clip 里的头（避免和烘焙姿态打架）
- Jump / Fall / Land 不叠加注视
- Pilot 不叠加注视

没有独立 `Action::Look`。注视是 Idle/Walk/Turn 上的叠加层。

### 6.2 禁止事项

- Auton 在 `!inStrikeRange` 时请求 Kick
- Auton 在 `ballAir` 时请求 Kick
- Auton 在 `!inLeapTrigger` 时请求 Jump
- Auton 改 `CharacterModel.mode`
- Auton 写 Pose 关节
- 用 `nextPlayClip` 当智能选条（Kick / Jump / 手势都走独立请求入口）

---

## 7. 状态机（Auton）

```
        松杆 ≥ 0.8s
Pilot ──────────────────► Auton
  ▲                         │
  │  推杆 / 着地 A/B / Pose  │
  └─────────────────────────┘

Auton 内部（busy 时锁在当前技能）：

Hold ──好奇/警觉──► Attend
Attend ──球不在正面──► Face
Face   ──已转正且玩心──► Approach   （够得着则 Strike）
Approach ──inStrikeRange──► Strike
Attend / Hold ──球在附近天上──► Leap
Strike ──clip 结束──► Attend         （看球落地）
Leap   ──Land 结束──► Attend
Attend ──社交且玩心低──► Signal
Signal ──clip 结束──► Hold
任意非 busy ──驱动掉光──► Hold
```

Kick 结束后强制 Attend 至少 1.2s，再允许 Signal / Approach / Leap。这是「踢完看结果」，不要踢完立刻挥手、再踢或连跳。

Leap 结束后同样进 Attend（看球），并启动 `kLeapCooldown`。落地缓冲期间 `busy`，不得再请求 Jump。

---

## 8. 需要新增的数据（对照现状）

下列是闭环所必须的状态。没有它们，§0 的四句审核标准无法成立。

### 8.1 必须新增

| 状态 | 放哪 | 现状 |
|---|---|---|
| `ControlMode { Pilot, Auton }` | 角色或控制器 | 无，永远玩家 |
| `autonIdleTimer` | 松杆计时 | 无 |
| `PilotIntent`：skill / lookAt / seekXZ / 独立 clip 请求 | 智能层输出 | 无 |
| `ballDist, ballBearing, facingErr` | 感知 | 只有球世界坐标与速度 |
| `inStrikeRange` / `inLeapTrigger` | 感知 | 无；Kick/Jump 都不检查球态 |
| `leapCooldown` | 动机层 | 无 |
| `kickStance`（世界 xz） | 感知 | 出生点局部偏移有，运行时不反求站位 |
| `ballAir, ballStruckRecent` | 感知 | `struck` 只服务当次踢 |
| `operatorPresent, cameraBearing` | 感知 | 无 |
| 五维驱动 + 当前技能 + 技能已持续时 | 动机层 | 无 |
| `lookAt` + 头/颈叠加 | 马达后处理 | Idle 清头；走转头只跟转向轴 |
| Auton 内部 `forward/turn/clip` | 输入合成 | `ArenaInput` 只来自外设 |

### 8.2 现有可复用（不要另造一套）

| 已有 | 用作 |
|---|---|
| `Action` 八态 | 马达 |
| `Walk` / `Turn` IK | Approach / Face |
| `Kick` clip + 左脚胶囊碰撞 | Strike |
| 程序跳跃（蓄力 / 弹道 / 落地） | Leap |
| `Gesture` 六段 | Signal |
| `clipHold` 着地打断 / 空中忽略 | `busy` 与 Pilot 抢回 |
| 球刚体与场地钳位 | 感知输入 |
| `kBallSpawnX/Z` | 推导 `kickStance` |
| `Head`/`Neck` 限位 | look-at 钳位 |

### 8.3 明确不做（本设计范围外）

- 新的 `Action` 枚举值（Look / Seek / Sleep 等）；跳用现有 `Action::Jump`
- 无触发条件的连跳、走路过程中无故起跳
- 用 Bandai walk/run clip 替换现有走路 IK（智能仍走程序 IK）
- 障碍、武器、第二颗球、对战
- 改博物馆展品网格
- 为智能再扩一整包无标签 clip

Idle 就是钉脚站住，需要时转头。不为待机再加一层持续的身体微动。

---

## 9. 帧内伪代码

位置：建议 `ArenaController::update` 在调用 `stepCharacter` 前合成 `ArenaInput`。感知可读上一帧角色与球；本帧马达仍只跑一次。

```
in = 玩家输入

if mode==Pose:
    清 Auton 状态
    stepCharacter(in)          # 现状
    return

if 玩家要回 Pilot:
    control = Pilot
    清 seek / 技能持续 / 内部轴
    lookAt = None

if control==Pilot:
    if 杆回中且着地且非蓄力跳:
        autonIdleTimer += dt
        if autonIdleTimer >= 0.8: control = Auton
    else:
        autonIdleTimer = 0
    stepCharacter(in)
    return

# Auton
感知 ← 角色, 球, 相机, 上一帧技能
更新五维驱动
if not busy:
    选举技能（迟滞 + 最短时间）
合成内部 ArenaInput（§6 表）
stepCharacter(内部输入)
if 本帧 Action 不是 Kick/Gesture/Jump/Fall/Land:
    叠加 Head/Neck look-at
```

Auton 合成的 `clip` 请求必须写成「请求槽 N」，不要写成 `clipStep=+1`，否则会误走 A/B 圈。

---

## 10. 和当前 Play 的并存规则

| 情境 | 行为 |
|---|---|
| 推杆走路 | Pilot，与现在相同 |
| 松杆 0.8s 后球在左前方远处 | Auton：Attend → Face → Approach → Strike |
| 松杆时球已在踢球点 | Auton：Face（若需要）→ Strike |
| 踢飞后球还在滚 | Attend 看球，不立刻再踢 |
| 踢飞后球在附近天上 | Attend 后可 Leap，落地再看；冷却 4s |
| 松杆且最近 8s 你动过相机/杆 | 玩心不高时可 Signal |
| Auton 走近时你推杆 | 立刻 Pilot，走你的方向，头立刻回正 |
| Auton 正在 Kick / 蓄力跳时你按 A | 着地则按现规则切玩家圈的下一个 clip；不读智能刚请求的槽 |
| 空中 | 物理管落地；Auton 不选举；推杆则立刻 Pilot |
| Pose | 无智能 |

玩家 A/B 圈序不变：`Kick → Jump → WAVE L → WAVE R → WAVE 2 → UP L → UP R → UP 2`。

**已决：圈指针分开。** 智能请求 Kick / Jump / 手势走独立入口，不调用 `nextPlayClip`，也不改玩家的 `clipIndex` 圈位置。玩家下一次 A 仍从上次**玩家自己**停在的槽继续。若马达内部暂时需要一个槽号才能播，用旁路字段，播完恢复玩家圈指针，或根本不写 `clipIndex`。

---

## 11. 建议的验收（实现之后才跑，现在只作合同）

主机可测，不冒充真机 FPS：

**第一刀（交接 + 注视球，2026-09-17 主机测试已过）：** 1、6、7、8，以及失联保持 Auton、着地 A/B 回 Pilot 且不拨智能圈。

**第二刀（Approach / Strike，2026-09-17 主机测试已过）：** 2、3、4、5、12，以及踢完 Attend 1.2s 不连踢。

**第三刀（Leap / Signal，2026-09-17 主机测试已过）：** 9、10、11、智能 Jump 后圈不变。

**第四刀（五维动机，2026-09-17 主机测试已过）：** 技能分门闩、迟滞选举、Face、连踢玩心抑制。Signal 仅在 social 高且 play 仍低时胜出。

1. 杆回中 0.8s 内 `control` 仍为 Pilot
2. 球放在 3m 外正面，Auton 先出现非零 `turn` 或 `forward`，且在 `inStrikeRange` 前不进入 Kick
3. 球在身后时先转，不侧走踢
4. `inStrikeRange==false` 时 `Action` 绝不是 Kick（Auton）
5. Kick clip 结束前技能保持 Strike
6. 推杆后一帧 `control==Pilot`，内部 `forward` 不再写入，头 yaw/pitch 无 Auton 叠加
7. Pose 下无 Auton 选举
8. Look-at：Attend 时 `Head.yaw` 与 `ballBearing` 同号且非零（球在侧面时）
9. Signal 仅在 `operatorPresent` 时出现
10. Auton 在 `inStrikeRange` 时不进入 Jump
11. Auton 仅在 `inLeapTrigger` 时进入 Jump；地面静止球不跳
12. 智能播 Kick 或 Jump 后，玩家 `clipIndex` 圈位置不变

真机只在审核通过并实现后另做，不在本文写未测帧率。

---

## 12. 审核清单

已决（2026-09-17 用户确认保留）：

- [x] §0 四句产品行为（Jump 交给智能）
- [x] 玩家圈指针与智能请求分开
- [x] 回 Pilot 时头立刻回正
- [x] 松杆延迟 0.80s
- [x] 踢球必须先到 `kickStance`，禁止隔空踢（Auton 站位 0.05 且对准 0.05，按当前朝向局部偏移；空踢后重寻）
- [x] 踢完强制看球 ≥1.2s
- [x] Leap 仅 `inLeapTrigger`（附近空中球），冷却 4s，`kLeapDist=4.0`
- [x] 注视只叠头/颈，不进 Kick/Gesture/跳
- [x] Signal 只对操作者，默认挥手，踢中才双手举
- [x] 失联中性轴保持 Auton（§3.2）
- [x] Face：`|facingErr|≤0.35` 时分为 0，否则 Attend+0.25
- [x] Strike / Leap 用即时 `playDesire`，不用正在爬升的 `play`
- [x] 庆祝 Signal 另加 +0.55

真机观感待用户看，不以主机测试代替。

---

## 13. 前置：动作层缺口（对照当前代码）

对照：`character_model.{h,cpp}`、`arena_controller.h`、Kick / Gesture / Jump 马达。  
智能层以后只给目标、调用这些能力。下列能力 **现在没有**。不先做的话，Auton 只能伪造摇杆和 `clipStep=±1`：会打穿已决的圈指针，Attend 在屏幕上也等于没发生。

### 13.0 动作层已经有的（不要重做）

| 能力 | 代码事实 |
|---|---|
| 摇杆走 / 原地踏步转 | `Walk` / `Turn`，钉地 IK，速度 2.0 / 转向 1.8 |
| 原地踢球 | `Kick` clip + 左脚胶囊，碰到才给球速度；期间锁位移和朝向 |
| 程序跳跃 | 地面蹲 0.20s → 起跳 → Fall → Land 0.20s；蓄力期间锁 xz |
| 六手势 | `Gesture`，脚钉地，播完回 Idle |
| 着地切条 / 空中忽略 A/B | `stepCharacter` 内局部 `clipHold` |
| 球刚体 | 位置、速度、落地阻尼、场地钳位 |
| 头限位 | `Head` yaw ±0.87、pitch −0.26…0.44；**没有**朝目标看 |

Jump 马达本身已齐。缺的是独立 `playJump()`，不是再做一套跳跃。

### 13.1 必须先做

按依赖排序。每一项都要能在 **没有 Auton** 的情况下用主机测试（或临时调试输入）打到。

**状态（2026-09-17）：M1–M5 已在角色马达落地，见技术文档 §12。下面保留缺口说明，供对照。**

**M1. 独立动作请求，不拨玩家 A/B 圈**

现状：唯一入口是 `clipStep → nextPlayClip(clipIndex) → startPlayClip`。播什么和圈指针共用 `clipIndex`。`applyGesture` 用 `clipIndex-2` 取段。

要补：

- 玩家圈：`clipIndex` 只给 A/B 的 `nextPlayClip`
- 播放请求：`playKick()` / `playJump()` / `playGesture(id)`，不改圈指针
- Gesture 按 `id`（0…5）取样，不绑 `clipIndex`

没有这项，已决的「圈指针分开」无法落地。

**M2. 注视世界点（Idle / Walk / Turn 叠加）**

现状：每帧 `pose.anim={}`；Idle 只钉脚；走/转时 `applyTurnFollow` 把头 yaw 写成转向速度。Head pitch 未用于看东西。

要补：

- 角色上有 `lookAt { None, WorldPoint }`
- 马达写完身体后，仅 Idle/Walk/Turn 叠 `Head` yaw/pitch，必要时少量 `Neck` yaw，走 `limitOf`
- Kick / Gesture / Jump / Fall / Land **不叠**
- `lookAt=None` 的当帧头立刻回正（已决）

没有这项，Attend / Hold / 踢完看球在屏幕上不存在。

**M3. 走到世界 xz，并转到指定朝向**

现状：走/转只吃这一帧的 `forward`/`turn`。没有目标点、没有到达半径、没有「先转正再走」。Kick / Gesture / 蓄力跳锁根位移，这点保留。

要补马达原语（测试和以后的 Auton 共用）：

- `faceYaw`：原地转到目标朝向，`|err|` 小于门槛后停
- `walkTo(x,z)`：朝向误差大则只转；够小则前进；进入半径后 Idle
- 出界钳在 `±16`

不要让智能层自己 PID 摇杆。没有这项，Approach / Face 无法单独验收，后面会出现侧走踢、走过点再踢。

**M4. 踢球站位几何（只查询，不改玩家 Kick）**

现状：球出生在机体局部 `(-0.42, 0.74)`。球飞走后，马达不知道站哪才能再踢中。玩家 A/B **仍允许原地空踢**，不要给玩家加距离门闩。

要补纯函数：

- `kickStance(character, ball) → (x, z, heading)`：球落在左脚踢球点时，根该站哪、该朝哪
- `inStrikeRange(...)`：按 §4.1 四条，供测试；Auton 以后才用来决定请不请 Kick

没有这项，M3 的目标只能是球心（踩球或踢空）。

**M5. 导出 `busy`**

现状：`clipHold` 是 `stepCharacter` 局部变量。

要补查询：当前是否 Kick / Gesture / 蓄力跳 / 空中 / Land。M3 在 busy 时不得改根位姿（与现在锁定一致）。智能层以后用同一查询，不写第二套。

### 13.2 不是动作缺口（不要提前做）

| 看起来像动作 | 实际归属 |
|---|---|
| Pilot / Auton、松杆 0.8s | 控制权，智能层 |
| 五维动机、技能选举 | 智能层 |
| 禁止隔空踢的策略 | 智能决定请不请 Kick；玩家仍可原地踢 |
| Leap 冷却、踢完看球 1.2s | 智能层计时 |
| 新 `Action`、新 clip、换走路 IK | 设计明确不做 |
| 空中叠注视 | 设计明确禁止 |

### 13.3 建议验收（只测动作层）

状态：**完成**（主机测试，2026-09-17）。动作层验收；Auton 见上文四刀。

1. 玩家停在 WAVE L，调用 `playKick()`，播完后圈仍是 WAVE L，下一次 `clipStep=+1` 是 WAVE R
2. Idle 看侧面世界点：`Head.yaw` 与目标同号且非零；清 `lookAt` 后当帧 yaw 回 0
3. Walk 时看侧向点：头朝目标，脚仍沿 `heading` 走
4. Kick / Jump 过程中改 `lookAt`，不改 clip / 跳跃姿势里的头
5. `walkTo` 从约 3m 外走到 `kickStance` 半径内停下，过程中先转正再进
6. 球在身后时 `walkTo(kickStance)` 先转后走，不侧步接近
7. `busy` 期间 `walkTo` 不移动根
8. 玩家 `clipStep` 踢球仍不要求 `inStrikeRange`（沙盒不变）

以上 8 条过了，再立项智能层。
