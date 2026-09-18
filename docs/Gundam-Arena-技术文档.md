# Gundam Arena 技术文档

> 本文是 Gundam Arena 的**持续技术记录**。项目简介、当前实现、技术细节、验证边界和后续迭代都写在这里，不另开平行主文档。
>
> 当前代码基线：`feat/gundam-arena` @ `043f882`（文档提交；功能提交仍为 `3983c53`，2026-09-17）。
>
> 用户已于 2026-09-17 完成真机验收。本文仍不虚构固件哈希、设备帧率或外设故障矩阵。

---

## 1. 项目是什么

Gundam Arena 是 StopWatch 上的独立 Mooncake App，Launcher 显示名为 **Gundam Arena**，图标复用博物馆的 `icon_gundam_museum`。

它把一架可关节驱动的 **SD RX-78-2** 放到大网格地面上，用第三人称跟随相机查看。当前目标是训练沙盒，不是对战、任务或博物馆展品浏览器。

提交意图（`3983c53`）：给 Arena 独立 App 和分支，让可摆姿势的 RX-78 在大地面网格上行走，支持手柄/触控，**不改博物馆展品，也不动无关 WIP。**

和 Gundam Museum 的分工：

| | Gundam Museum | Gundam Arena |
|---|---|---|
| 目的 | 固定站姿展品浏览 | 可动角色在场地上走、跳、摆姿势 |
| 机体 | 多台 SD 展品，含装备 | 仅 RX-78，无枪/盾/剑 |
| 空间 | 8³ 线框展柜 | 40×40 地面网格，活动半径 ±16 |
| 相机 | 绕展品观察 | 第三人称跟随，可环视/俯仰 |
| 输入 | 浏览/切展品 | 移动、跳跃、关节编辑 |

Museum 的制作卡、复盘和视觉验收合同仍然只覆盖展品；Arena 不继承那些造型验收结论。

---

## 2. 当前状态

用户已于 **2026-09-17 完成真机验收**。代码与 `3983c53` 沙盒一致，仓库未补烧录证据。

已实现、可在主机测试验证：

- 独立 App 注册，进入后停 LVGL、直绘或画布回读
- 19 骨骨架、绑定网格、每帧蒙皮求值
- Play：走、转、跳、落地、原地踢球；A/B 循环 Kick/Jump/六手势（WAVE L/R 为 Bandai 2 `active`）；支撑脚钉地 IK、对侧摆臂
- Pose：循环选关节、拖动欧拉角、单关节复位、角度限位
- 第三人称跟随相机；Play 模式触屏上半区环视
- 触屏虚拟摇杆 + 机身键 + 可选 Joystick2 / Dual Button
- 主机测试覆盖骨骼、运动、渲染、输入合并

明确未做：

- 固件哈希、烧录日志、设备 FPS 数值（用户已完成真机验收，但这些测量未写入仓库）
- 武器、碰撞体、AI、对战、音效、独立图标
- 与博物馆 `buildRx78()` 网格的运行时共用（Arena 有一份绑定版生成器）
- 完整外设故障矩阵（总线超时、电源失败、长期重入）的 Arena 专项验收

已知代码事实：`rx78_rigged.cpp` 包含了 `rx78_assembly.h`，当前文件内未使用该头。

---

## 3. 怎么玩

### 3.1 Play

默认模式。机体朝向 `heading`，在 XZ 平面移动，Y 为高度。地面 `y=0`。

| 动作 | 条件 | 行为 |
|---|---|---|
| Idle | 前进/转向死区 ≤0.18，且着地 | 双脚放下，相位清零 |
| Walk | 前进轴 \|f\| > 0.18 | 速度 2.0，相位随位移前进 |
| Turn | 几乎不前进但在转向 | 原地踏步，步幅系数 ±0.35 |
| Jump | 圈内槽 1，着地短按切到该项 | 先地面下蹲 0.20s，再以 4.0 起跳，重力 18 |
| Kick | 圈内槽 0，着地短按切到该项 | 收腿再一次前踢（约 1.27s）；脚向前碰到球才给速度 |
| Gesture | 圈内槽 2–7 | Bandai 2 六手势 clip（WAVE L/R 为 `active`），脚钉地，播完回 Idle |
| Fall | 空中且 `vy≤0` | 伸腿准备落地 |
| Land | 落地后 0.20s | 屈膝缓冲并收回；着地时 A/B 可打断 |

Play 下 **A 短按下一个并立刻播，B 短按上一个并立刻播**。圈序：`Kick → Jump → WAVE L → WAVE R → WAVE 2 → UP L → UP R → UP 2 → BOW → BYE → BYE2 → GUIDE → PUNCH → DNC L → DNC S`。开局未播过：第一次 A 是踢球，第一次 B 是短舞。着地可打断当前 clip/蓄力跳/走/落地；空中切条忽略。走/转仍用触屏摇杆，不进圈。

空中前进为地面的 45%，转向为 60%。未着地且输入失效时前进速度每步衰减到 98%。位置钳在 `±16`。

### 3.2 Pose

暂停键切换。不再积分位移。HUD 显示 `POSE {关节名}`，左右画博物馆同款箭头热区。

可循环关节（18 个，不含 Root）：

`Head → Neck → Chest → Pelvis → LShoulder → LUpperArm → LForearm → LHand → RShoulder → RUpperArm → RForearm → RHand → LThigh → LShin → LFoot → RThigh → RShin → RFoot`

拖动改当前关节 yaw/pitch，确认键把当前关节欧拉角清零。限位见 §5.3。

### 3.3 操作映射

屏幕 468×466，圆形有效触区半径 233，圆心 (234, 233)。Arena 把画面分成上环视 / 下移动，分界 **y=266**。

| 意图 | 触屏 | 机身键（GPIO 2=A，GPIO 1=B） | Joystick2 + Dual Button |
|---|---|---|---|
| 前进/后退 | 下半区，`viewAxis=(363-y)/90` | — | 摇杆 Y → `viewAxis` |
| 转向 | 下半区，`steer=(x-234)/140` | — | 摇杆 X → `steer`，控制器再取负 |
| 环视（仅 Play） | 上半区拖，灵敏度 0.012 / 0.008 | — | 预览拖动手势 |
| 下一动作并播放 | — | **A 短按** = `cancelPressed` | Dual Button 红键短按 |
| 上一动作并播放 / 复位关节 | — | **B 短按** = `confirmPressed`（Play=上一槽；Pose=复位关节） | Dual Button 蓝键短按 |
| 切 Play/Pose | — | **B 长按** = pause | Dual Button 红键长按 |
| 选关节 | 左右箭头 `previous`/`next` | — | Pose 时水平导航步进 |
| 退出 App | — | A+B 和弦 | 外设退出和弦 |
| 回 Launcher | 机身 Home（`KeyManager::GoHome`） | 同左 | — |

触屏细节：

- 下半区按下立即作为虚拟摇杆，松手轴回中
- 上半区拖满 6 像素后成为环视，不与移动轴同时生效
- Pose 左右热区与博物馆相同：`previous{8,201,48,64}`、`next{412,201,48,64}`
- 触屏任务 20ms，按键任务 10ms；消费时按键超时 100ms、触屏超时 150ms 则判定输入不健康，轴清零

手柄合并 `mergeExternalPad`：外设有效时覆盖 `steer`/`viewAxis`；确认/暂停/退出做或；导航步进有值才覆盖。Grove 5V 打不开则只走触控，日志警告。

顶部状态字：Pose 为关节名；正在播 Kick/Jump/手势时显示该动作短名（智能 Kick 也写 `KICK`，不写 `A/B CLIP`）。Auton 待机显示技能短名 `HOLD/LOOK/FACE/WALK/KICK/JUMP/WAVE`。否则校准中 `CAL`，手柄就绪 `PAD`，故障 `PAD FAULT`，未播过 `A/B CLIP`。`PAD FAULT` 只表示摇杆失联，不是踢球失败。

---

## 4. 架构

```
AppGundamArena
 ├─ DeviceControlSource          机身键 + 触屏（复用 Racer 输入任务）
 ├─ HardwareRacerInputProvider   可选 Grove 外设
 ├─ KeyManager                   Home 回 Launcher
 ├─ ArenaController              输入 → 角色步进 + 相机跟随
 │    └─ CharacterModel          模式、动作、位姿、骨骼动画
 └─ ArenaRenderer                绑定网格 + 每帧蒙皮 + 软件光栅
      ├─ Mesh / Skeleton         开机构建一次，顶点在骨骼局部空间
      ├─ MuseumProjectionCache   同骨骼共享顶点投影
      └─ CarSurfaceRaster<424,424>  博物馆/赛车同源实体光栅
```

物理固定 `dt = 1/60`，用累加器追帧，单次 `update` 最多 5 步；打满 5 步则丢弃剩余累加，避免螺旋。渲染另有约 33ms 下限（约 30 FPS 封顶），每帧后再 `delay(5)`。

相机不跟随机体朝向：机体转身时 `camYaw` 仍趋向 `π + orbit`。环视只改 `orbit`/`pitch`，跟随只平滑 `lookX/Y/Z` 和 `camYaw`。

---

## 5. 骨架与网格

### 5.1 坐标

与博物馆 SD RX-78 一致：

- `+Y` 向上，`+Z` 朝机体正面，`+X` 是机体左侧
- 脚底名义高度 `kSoleY=0.025`，绑定后由骨骼带到世界
- 前进：`x += sin(heading)*speed*dt`，`z += cos(heading)*speed*dt`（`heading=0` 沿 +Z）

### 5.2 骨骼层级

19 骨，枚举顺序即求值顺序，父骨必须先于子骨：

```
Root
 └─ Pelvis
     ├─ Chest
     │   ├─ Neck → Head
     │   ├─ LShoulder → LUpperArm → LForearm → LHand
     │   └─ RShoulder → RUpperArm → RForearm → RHand
     ├─ LThigh → LShin → LFoot
     └─ RThigh → RShin → RFoot
```

绑定长度（世界单位）：

| 量 | 值 | 用途 |
|---|---|---|
| `kHipY` / `kHipX` | 1.02 / 0.285 | 骨盆与髋 |
| `kThighLen` / `kShinLen` | 0.33 / 0.44 | 腿 IK |
| `kChestY` | 1.50 | 胸 |
| `kNeckY` | 2.025−0.14 | 颈/头枢轴 |
| `kShoulderX` / `kShoulderY` | 0.72 / 1.76 | 肩 |
| `kUpperArmLen` / `kForearmLen` | 0.405 / 0.35 | 臂 |
| `kArmOut` / `kElbowBend` / `kSpread` | 0.18 / 0.22 / 0.12 | 绑定休息姿势 |
| `kFootYaw` | 0.15 | 左右脚外展 |

左半身用 `s=-1` 镜像休息变换。前臂/手的休息位从肩经上臂、肘弯递推，不是单独手写世界坐标。

### 5.3 关节

局部动画是 roll/pitch/yaw 欧拉，合成为 `restLocal * eulerTRS(anim)`，再乘父世界矩阵。Root 额外乘 `eulerTRS(root, 0, 0, rootYaw)`。

求值时一律 `clampJoint`。限位摘要：

| 骨 | pitch | yaw | 备注 |
|---|---|---|---|
| Head | −0.26…0.44 | −0.87…0.87 | 点头/摇头主自由 |
| Neck / Chest | 较小 | ±0.35 / ±0.40 | |
| Pelvis | −0.20…0.25 | 无限制（±π） | 转向跟随用 |
| Shoulder | −1.10…0.55 | ±0.70 | |
| UpperArm | −1.40…0.80 | ±0.80 | 走路摆臂 |
| Forearm | −2.20…0.05 | ±0.40 | 几乎只能屈肘 |
| Hand | ±0.70 | ±0.60 | |
| Thigh | −0.80…0.90 | ±0.40 | IK 写入 pitch |
| Shin | 0…1.80 | ±0.12 | 不允许反膝 |
| Foot | −0.45…0.35 | ±0.35 | 着地时由腿角推导 |

### 5.4 绑定网格

`buildRx78Rigged()`：

1. 用 Arena 内的 `SdBuilder` 生成素体（`equipment=false`）
2. `makeBindSkeleton` 写休息局部/世界
3. `localizeMesh`：每个 panel 用 `CarPanel.wheel`（骨 ID+1）变到该骨局部空间，法线一并旋转

网格来源是博物馆 SD RX-78 的生成思路（壳、盖、管、眼窝 `buildEyeSocket`），但**不是**调用 `gundam_museum::buildRx78()`。生成时把骨 ID 写入 `panels[i].wheel`。测试要求：

- panel 数 >400 且不超过 `Mesh::capacity`（4096）
- 头、前臂、小腿、手都有面
- 不含 `Part::Rifle` / `Shield` / `Sabers`

每面烘焙固定方向光：主光 `(-0.46, 0.65, 0.60)`，辅光 `(0.70, 0.25, -0.67)`，颜色 RGB565。手、金色胸缝、部分开口/眼区标双面。

开机只构建一次网格和投影索引；之后只改骨骼世界矩阵。主机测试走、转、跳、转头时 `mesh_builds` / `index_builds` 保持不变。

---

## 6. 运动与 IK

`stepCharacter` 每 1/60s 一次。Pose 模式只改选中关节后 `applyRoot` 返回。

### 6.1 走路 IK

相位 `walkPhase += |move| * dt * 8.5`。`sin(phase)` 为步幅，`cos(phase)` 为支撑/摆动切换。

- 摆动腿：取消钉地，抬脚最高 0.18
- 支撑腿：首次进入支撑时把脚掌钉在世界 XZ；之后用髋点与钉点的机体纵向投影当 IK 目标，钳在 ±0.40
- `twoBonePitch` 在矢状面解大腿/小腿 pitch；小腿 pitch ≥ 0
- 脚 pitch 由腿角和抬脚量推导，使脚掌接近地面
- 上臂对侧摆 ±0.35，前臂常屈 −0.25
- 主机测试：20 个步周期内同侧手腿同向计数为 0；支撑期单脚世界位移 < 0.45

### 6.2 转向跟随

`angularSpeed * 0.12` 钳到 ±0.18 后写入骨盆/胸/头 yaw，头再额外 ×1.15，最后头 yaw 再钳到 ±0.87。

### 6.3 跳跃姿势

跳跃仍是程序姿势（Bandai 集 1 没有 jump）。节奏参考 CMU `13_39`，**尺度按 §6.5，不抄人体关节角**。蓄力时脚钉在 `kPlantAnkleY`，Root 下沉 `kJumpSquatY=0.14`，腿用和走路相同的双骨 IK，所以蹲深是 SD 短腿该有的幅度，不是真人 −0.34 那种微屈。骨盆/胸正 pitch = 前倾。

按 B 后先在地面下蹲蓄力 `kJumpCrouch=0.20s`（约 0.12s 蹲到位再稍停），再离地。起飞速度 `kJumpVel=4.0`（顶点约 0.44，约 0.24 个 SD 身长）。空中随弹道：蹬伸（臂前摆到限位 0.80）→ 顶点收腿约 −0.55 / 1.10（不打满 −0.80 / 1.80）→ 下落屈膝。蓄力臂后摆约 −0.85。落地 `kJumpLand=0.20s` 再沉髋 `kJumpLandY=0.10` 缓冲。蓄力期间钉住 xz。

### 6.4 踢球 clip

走路仍用 §6.1 的程序 IK（无限循环、钉地）。踢球不走强化学习。Bandai `dataset-1_kick_normal_001.bvh` 第 48–62 帧经 IK 后是膝鞭：小腿先折再弹再折，看起来就是半途回抽再小踢一下。设备 clip **不再播这段**。改成作者写的收腿蓄力 → 一次伸直前踢 → 收回 Idle。烘焙时四肢 L/R 对调到 `-X` / `LThigh`。骨盆/胸钳成前倾（出脚时约 +0.12 / +0.16）。手臂不抄手部 IK：收腿、出脚各锁一个臂姿（支撑臂后收约 −0.48/−0.55，踢腿侧几乎不动，肘常屈 −0.28～−0.50），出脚全程保持不动。

设备上只播 `arena_kick_clip.h`：`kKickFrames=39`，按 `clipT * 30` 在相邻帧间线性插值。顺序是收腿 → 一次前踢 → 收回站立。Kick 期间 Root 位移/朝向锁住。机身 **A 短按**切到踢球槽并播放。切 Pose 会取消 clip。

开局在机体 `-X` 前方（`kBallSpawnX=-0.42, kBallSpawnZ=0.74`）放一个半径 `0.16` 的小球。每帧用 `LFoot` 踝→趾胶囊扫掠相交，且脚须向前摆（相对朝向速度 > 1.5）才给一次冲量，避免收腿阶段误碰。球画进机体同一套光栅/深度，按透视遮挡，不再是屏幕 overlay。Pose 模式也继续积球。不重建网格。

走路默认仍是 §6.1 程序 IK。Bandai walk 只烤成 `tools/arena_motion/arena_walk_clip.h` 作对照，不在设备上播放。

重定向规则见 §13。生成命令：

```bash
python3 tools/arena_motion/fetch_bandai.py
python3 tools/arena_motion/fetch_bandai2.py
python3 tools/arena_motion/retarget_bandai.py
```

BVH 原件不入库。clip 头文件是 CC BY-NC 4.0 衍生作品，商用发行前要另选数据或取得授权。

### 6.4.2 Bandai 2 空手手势

十三段进 `arena_gesture_clips.h`（多数 50 帧 / **10800 B** `.rodata`，短淡入 + 选窗 + 淡出）。源片只决定哪只手、切哪一段、挥手左右相位；SD 姿势按身份手写钥匙，**不再**共用矢状手 IK。WAVE 举起 pitch 锁死、源手 X 在头外侧放大成上臂 yaw；UP 过头顶举起并停住（上臂外展，避开 SD 头盔）；GUIDE 沿 +Z 指住；BYE 头侧偏高小幅 yaw；BYE2 头边双手小幅 yaw（外展）；PUNCH 先左后右各一记短刺，打完再收回；舞看沉髋窗，举起一侧同样外展，双臂交替/对向，不共用同一 pitch 上限。`DNC L` / `DNC S` 源片分别约 158s / 63s，不再都裁成 50 帧：长舞窗 76 帧（整段 90 / 约 3.0s），短舞窗 56 帧（整段 70 / 约 2.3s）。CALL / ANS / SLASH 已撤。躯干仍用胸相对髋再按 §6.5 放大。出拳不抄手部 IK。舞另有 `kGestureRootDip`，播放时只沉 `pose.root.y`，不改 `c.y`。前六段仍是集 2：`WAVE L` / `WAVE R` 按 yaw 行程在 `normal`/`active` 里选，其余 `normal`。后七段是集 1：`BOW BYE BYE2 GUIDE PUNCH DNC L DNC S`。Auton 仍只用 ID 0/1/5（WAVE L/R、UP 2）。walk/run/turn 仍只离线预览。集 2 与集 1 同为 22 骨、`ZXY` / `Rz*Rx*Ry`。

主机软件光栅中位耗时（8 次，非设备 FPS）：WAVE L 720 µs，WAVE R 757 µs，WAVE 2 752 µs，UP L 727 µs，UP R 762 µs，UP 2 734 µs，BOW 759 µs，BYE 708 µs，BYE2 727 µs，GUIDE 717 µs，PUNCH 726 µs，DNC L 723 µs，DNC S 728 µs。设备帧率未测。

### 6.5 人体 mocap → SD 尺度（冻结，所有动作）

2026-09-17 跳跃专项踩过两次：先把 CMU 关节角 1:1 写进 SD，蓄力几乎没蹲、整机却弹到约 0.6 个身长；再按映射角写臂和空中膝，摆臂只比走路大一点。踢球又踩了同一类：**人体配平 1:1 写进 SD**（出脚后仰、双臂泵摆屈肘）。人身上那是小角度找平衡，SD 头大肢短，看起来就是后仰和多余挥舞。再后接 Bandai 踢的「前摆窗」：源数据是膝上提再鞭踢，双骨 IK 之后变成小腿先折到 1.63、弹到 0.46、再折到 1.80。真机就是抬腿没问题、往前踢半途收回、再一小踢。后续任何走、跳、踢、挥、落地都按这条。

**禁止**

- 把人体 BVH / Mixamo 的关节角直接当 Arena 姿势。
- 只弯膝、髋高度不变。真人下蹲主要是髋往下走；IK 里髋锁在 `kHipY=1.02` 时，脚世界坐标几乎不变，求出来的角会退化成微屈。
- 用人体弹跳的绝对速度（或为“好看”随便加大 `kJumpVel`）当 SD 弹道。
- 把人体 IK 求出的臂角、空中膝角当最终值。SD 肢短，同样世界位移对应的角偏小，看起来会像走路。
- 把人体踢/打时的后仰配平写进骨盆/胸。人身上是小角度，SD 头大，看起来就是后仰。
- 把人体踢/打时的手臂配平（前摆后摆、大幅屈肘）写进上臂/前臂。人用胳膊找平衡；SD 上就是多余的挥舞。
- 把人体踢的膝鞭（折膝→弹直→再折）当「前摆窗」播出去。自检只看大腿有没有前摆、有没有后仰，看不出小腿打了两下。真机就是半途回抽。

**必须**

- 和踢球一样：先 FK 出世界坐标，按髋高缩到 `kHipY`，再用 Arena 双骨 IK 求角，最后 `limitOf()`。
- 下蹲、落地、重心降低要写进 **Root.y**（或等价的髋高度），脚钉在地面再求腿。
- 位移、跳高按 **SD 身长比例**，不按米制绝对值。原地跳顶点约 0.24 身长（当前 `kJumpVel=4` → 0.44）。
- 短肢动作按 SD 可读幅度放大：臂后摆/前摆要明显大于走路（当前 −0.85 / +0.80）；空中收腿约 −0.55 / 1.10。
- 踢球/出拳的手臂跟主动作分段锁姿势，不要抄手部世界坐标的双骨 IK，也不要按大腿符号过零点。当前踢：收腿支撑臂 −0.48、出脚锁 −0.55，踢腿侧上臂 0.18→0.08，肘常屈 −0.28～−0.50，出脚段不再摆。
- 顶点团身不要打满大腿 −0.80 / 小腿 1.80：那是短腿硬折人体姿势，剪影会缩成一团。地面蹲深已接近大腿上限就不要再拧。
- 一次踢只有一次伸：收腿后大腿单调前摆、小腿单调伸开，再收回站立。源窗若小腿有第二峰就丢掉，改作者出脚姿势。烘焙要断言前摆段 `LShin` 不得回升；主机测试同样断言。出脚终点还要让脚在高速段扫过球（`fwd>1.5`），不要在平滑末端才贴上球——那时速度已经掉没，击不飞。

核对口诀：蹲完头/髋有没有明显下降；跳起来是不是大约四分之一个机体；摆臂是否明显大于走路；空中收腿看得出但没缩成一团；踢/打时骨盆胸是不是前倾而不是后仰；踢的时候胳膊是不是一条干净的配平，而不是来回挥；**前踢是不是一下踢完，中间有没有收回再踢。** 有一项不对，就是尺度又错了。

### 6.6 相机

```
look 以 7 的指数平滑跟机体 (x, 0.35y+1.18, z)
camYaw 平滑到 π+orbit     // 不跟 heading
pitch 触屏钳位 −75°…0.55rad
distance 固定 9.2
```

Play 光栅比例 70%，Pose 100%。70% 时主机测量机体在圆形取景中的高度占比约 0.45–0.55。

---

## 7. 渲染

路径与博物馆/赛车相同：CPU 软件光栅，RGB565，无纹理 PBR、无实时阴影。

1. 清屏暗石板底 `0x298A`（约 `#293152`，不是纯黑）
2. 画 32 分割地面网格，半边长 20；近平面 0.20 裁线后投影
3. `evaluateSkeleton(pose)`
4. 按骨骼变换后的法线做背面剔除（阈值 −0.035）；双面面不剔
5. 两遍提交：背面 pass 0，正面 pass 1（减轻逆深度边缘翻面）
6. `MuseumProjectionCache` 按「同位置 + 同骨骼」共享顶点；近平面失败则回退逐顶点 `prepareCarPanel`
7. `CarSurfaceRaster<424,424>` 开 `setSolidFastPath(true)`；黄球用同一套相机做成朝向镜头的圆盘写入深度缓冲，再 `blitScaled` 到 `( (W-424)/2 , 21 , 424, 424 )`
8. 叠 HUD 字

Arena 地面是平面网格，不是博物馆的 8³ 线框盒。空间相机焦距 `88*7`，主点在画布中心偏上（`top + side/2`）。机体投影另用光栅主点在 70%/100% 缓冲中心，并用 `w/h` 校正横纵比。

有 framebuffer 时走 `DisplayFrameScope` 整屏直绘；否则画到 canvas 再 `updateCanvas()`。每 2s 打一条 `ArenaStage` 日志：panel 数、提交数、mesh/index 构建次数、绘制与送显微秒。

`arena_renderer.cpp` 与赛车/博物馆 renderer 一样在 CMake 中强制 `-O2 -fno-builtin-memset`。

---

## 8. App 生命周期

`onOpen`

1. 停 LVGL
2. 建 `KeyManager`，重置角色
3. `renderer.open()`：分配 `Surface`（网格 + 424² 光栅 + 投影缓存）
4. `setGrove5VPower(true)`，成功则开 `HardwareRacerInputProvider`，等 20ms
5. 开 `DeviceControlSource`，屏幕设为 `GameScreen::ArenaPlay` 并 `presentScreen`（释放门禁）
6. 先画一帧

`onRunning`

1. Home → `close()`
2. 采触屏/按键，合并外设
3. Pose/Play 切换时改外设导航模式（Pose=`Horizontal`，Play=`None`）并重新 present
4. `controller.update`；退出和弦则关闭
5. 帧间隔不足 33ms 则 delay 5ms 返回
6. 绘制，再 delay 5ms

`onClose`

关输入、关 Grove 5V、释放 renderer / controller / keys，恢复 LVGL。

固件入口：`main.cpp` 在 Museum 之后 `installApp(AppGundamArena)`。源文件由 `main/CMakeLists.txt` 的 `apps/*.cpp` glob 编入。

---

## 9. 文件地图

```
main/apps/app_gundam_arena/
  app_gundam_arena.{h,cpp}           App 生命周期、供电、帧循环
  controller/arena_controller.h      输入翻译、固定步进、相机跟随
  model/character_model.{h,cpp}      模式/动作状态机、走路 IK、Kick clip 播放
  model/rx78_bones.h                 骨、限位、仿射、姿态类型
  model/rx78_skeleton.cpp            绑定、FK、网格局部化
  model/rx78_rigged.cpp              素体网格 + 骨标签
  model/arena_kick_clip.h            Bandai kick 离线烘焙（生成文件）
  view/arena_space.h                 相机、地面网格
  view/arena_renderer.{h,cpp}        剔除、投影、光栅、HUD

共享接入（Arena 有改动，Racer/Museum 也编译这些文件）：
  main/apps/app_lets_and_go_racer/controller/game_flow.{h,cpp}   GameScreen::ArenaPlay
  main/apps/app_lets_and_go_racer/input/device_control_logic.h   摇杆/环视分区
  main/apps/app_lets_and_go_racer/input/device_touch_layout.h    Pose 左右热区
  main/apps/apps.h、main/main.cpp                               注册 App
  main/CMakeLists.txt                                           renderer -O2

测试：
  tools/gundam_arena_test.cpp
  tools/test_gundam_arena.sh
  tools/lets_and_go_device_control_test.cpp   Arena 触区回归
  tools/arena_motion/                    Bandai 拉取与 22→19 重定向
```

复用但未改实现：博物馆 `rx78.h` 网格类型、`sd_eye_socket`、`museum_projection_cache`、`museum_layout`、`museum_space::clipLine`；赛车 `CarSurfaceRaster`、`HardwareRacerInputProvider`、`DeviceControlSource`、`DisplayFrameScope`。

---

## 10. 验证

### 10.1 主机测试

```bash
bash tools/test_gundam_arena.sh [输出目录]
# 默认 /tmp/gundam-arena
# SANITIZE=1 可开 ASan/UBSan
```

断言包括：绑定网格完整、转头不掉胸、屈肘带动手、Idle 脚接近地面、前进/后退/转向/跳跃、20 步对侧摆臂与支撑钉地、Pose 限位、网格不重建、空闲占比、触屏分区、外设合并、右转使 heading 减小、俯视下限能看到地面。

输出 PPM（生产 renderer，不是设备截屏）：`arena-idle.ppm`、`arena-head.ppm`、`arena-walk.ppm`、`arena-turn.ppm`、`arena-far.ppm`、`arena-jump.ppm`、`arena-kick.ppm`、`arena-lookup.ppm`。

共享触控回归：`tools/lets_and_go_device_control_test.cpp` 在 `ArenaPlay` 下检查前进轴、转向、松手回中、环视不抢移动、左右箭头仍在；并确认 Racing 不会漏进 Arena 的前进轴。

### 10.2 目标构建与真机

- **2026-09-17：** 用户完成真机验收。本条是用户口头确认，仓库里仍没有固件哈希、烧录日志、设备帧率或操作录像。
- 主机 PPM / 单元测试不能替代上述设备证据。若后续要归档，补 `firmware.json`、flash 日志和帧率即可。

外设路径遵循 [外设输入接入与故障排查规范](外设输入接入与故障排查规范.md)。Arena 已做供电、独立采样任务、失联中性轴、退出和弦和切模式 present，但规范里的总线超时注入、电源失败清理、长期重入等 **Arena 尚未单独记为完成**。

---

## 11. 技术选择与边界

1. **独立 App，不改展品。** 可动实验与博物馆浏览隔离；共享的只是输入枚举和触区。
2. **线性混合蒙皮的简化版。** 每个 panel 只绑一根骨，没有多骨权重。
3. **矢状面双骨 IK，不是全身 IK。** 髋、肩的 roll/yaw 走路时基本不解；转弯靠附加 yaw。
4. **同源软件光栅。** 不引入第二套填充器；容量和颜色路径与博物馆相同。
5. **网格开机冻结。** 动画只改 19 个仿射，避免每帧重建 400+ panel。
6. **输入复用 Racer。** 新屏幕 `ArenaPlay` 特化分区，避免给 Racing 增加前进轴。

当前限制：

- 无地面碰撞以外的物理，无障碍，无坡
- 无武器、无手持约束、无足部全 6DoF
- 转向不带动相机，容易出现「侧面跟着走」
- Pose 与 Play 的动画姿态不混叠，切换会立刻清零走路 IK
- 踢球是原地 clip，有一颗深度测试小球；没有篮球尺寸、没有手持约束
- 没有独立 Launcher 图标
- `rx78_assembly.h` 悬空包含
- 未做 Arena 专属视觉制作卡；机体外形若要按官方板件精修，应另开节点，并说明与博物馆网格是否继续分叉

---

## 12. 迭代记录

后续改动按时间追加本节，不新开主文档。每条写：日期、范围、代码事实、验证了什么、**没有**验证什么。

### 2026-09-18 · 左右拳与舞加长

- **范围：** PUNCH 只打源窗更靠前的一只，L/R 对调后真机是一记左拳。DNC L/S 源片 4751/1901 帧，却都裁成 50 帧（约 1.7s），两段一样短。
- **代码：** PUNCH 改左刺→收回→右刺→停。长舞窗 76 帧、短舞窗 56 帧；dip 表按最长 clip 补齐。Auton ID 0/1/5 不变。
- **已验证：** 主机测试 `gundam_arena ok`（左右拳各一记、`DNC L` 90 帧 / `DNC S` 70 帧）。写入 `/dev/cu.usbmodem83301` MAC `44:1b:f6:c1:8a:00` 应用分区 `0x40ec40` B（4,254,784），余量 18%。esptool `Hash of data verified`。BIN SHA-256 `7be34b9941ab9d1b073f8982a677fde75fa420b14a344c4d060256cff4e89440`。
- **未验证：** 真机左右拳可读性和两段舞时长差。

### 2026-09-18 · 撤 ANS / SLASH

- **范围：** 真机 ANS 斜举和 SLASH 空手劈在 SD 胖身上都读不出来。
- **代码：** A/B 圈再去掉 ANS、SLASH（`kPlayClipCount=15`）。Auton ID 0/1/5 不变。
- **已验证：** `bash tools/test_gundam_arena.sh`。写入 `/dev/cu.usbmodem83301` MAC `44:1b:f6:c1:8a:00` 应用分区 `0x40b180` B（4,239,744），余量 18%。esptool `Hash of data verified`。BIN SHA-256 `d4a1f1d5588775d2989788b1c8e5ac3bdeeb02ce717a42a77dbf6b0d5c802c07`。
- **未验证：** 真机圈序。GUIDE/PUNCH 仍在圈里。

### 2026-09-18 · SD 胖身体：撤 CALL、抬 BYE、单臂劈

- **范围：** 真机 CALL 贴头被 SD 大脑袋吃掉；BYE 仍不够高；SLASH 双臂起落看不出在劈。细动作（贴头/前指/短刺）在短肢胖身上读不出。
- **代码：** A/B 圈去掉 CALL（`kPlayClipCount=17`）；BYE 钥匙抬到头侧并略外展；SLASH 改单臂外侧高举再斜劈过肚。Auton ID 0/1/5 不变。
- **已验证：** `bash tools/test_gundam_arena.sh`。写入 `/dev/cu.usbmodem83301` MAC `44:1b:f6:c1:8a:00` 应用分区 `0x410790` B（4,261,776），余量 18%。esptool `Hash of data verified`。BIN SHA-256 `92eddaaec3a0a97b5565b6252863c5d6262183e8031818fa99b4e2a464532820`。
- **未验证：** 真机 BYE 高度、SLASH 是否像劈。GUIDE/PUNCH 仍在圈里，同属细动作，待定。

### 2026-09-18 · 过顶手势避开 SD 头盔

- **范围：** CALL/BYE/SLASH 真机返修后，其余新手势按同一类排查：穿头盔、平举、过顶插值走面罩。WAVE/UP/舞的过顶钥匙 yaw=0 时手在脑门里（手网格 0.01–0.16）；WAVE 2 挥手还会收到 |x|≈0.45；BYE2 前臂蹭盔。BOW/ANS/GUIDE/PUNCH 手网格 ≥0.43，不是同一类，不改。
- **代码：** 过顶/头边钥匙加外展 yaw，挥手只在头外侧振荡；BYE 仍用胸前偏高钥匙、不跟头侧外展。WAVE L/R 风格片在映射 yaw 接近时仍取 `active`。
- **已验证：** `bash tools/test_gundam_arena.sh`。网格：WAVE/UP/BYE2/舞手≥0.34、臂≥0.19；CALL/SLASH 保持外侧；BYE 持高 y≈1.74；GUIDE/PUNCH 前伸臂距盔 0.16（点/刺近下颌，不是面罩）。写入 `/dev/cu.usbmodem83301` MAC `44:1b:f6:c1:8a:00` 应用分区 `0x4132a0` B（4,272,800），余量 17%。esptool `Hash of data verified`，RTS 重启。BIN SHA-256 `11b530d08d41658d63ee91ff089ce22f3fc3330e8b1acacc6b79675ef17533ee`。ELF SHA-256 前缀 `4da52265a`。Launcher 已起。
- **未验证：** 真机 WAVE/UP/BYE2/舞是否还穿头。

### 2026-09-18 · CALL/BYE/SLASH 真机返修

- **范围：** CALL 贴头穿进 SD 头盔；BYE 手停在 y≈1.30 像平举；SLASH 双手举在脑门前再垂下，像开合跳。
- **代码：** CALL 手留在头盔外侧（肩/上臂略外展、屈肘在耳高）；BYE 用胸前偏高钥匙加 yaw，不再用平举胸钥匙；SLASH 高段双手 V 字在头两侧，低段前劈到腰高，不落到垂手。
- **已验证：** `bash tools/test_gundam_arena.sh`。写入 `/dev/cu.usbmodem83301` Hash verified。
- **未验证：** 真机 CALL/BYE/SLASH 观感。

### 2026-09-18 · 手势身份钥匙

- **范围：** 十六段手势按 WAVE → UP → 社交 → 拳斩 → 舞拆身份钥匙，不再加大同一套矢状 pitch。§6.5 仍有效：不抄人体角 1:1；出拳不抄手部 IK；短肢用 SD 可读钥匙。Auton ID 0/1/5 不变。
- **代码：** `retarget_bandai.py` 的 `compose_wave` / `compose_hold` / `compose_punch_id` / `compose_slash_id` / `compose_dance_id`；WAVE 风格改看 yaw 行程；主机断言改手的世界坐标与振荡，不再只看峰值能量。`§6.4.2` 改为按身份钥匙。
- **已验证：** `bash tools/test_gundam_arena.sh`。身份断言：WAVE yaw 过零、UP 过顶停住、社交五槽剪影、拳胸前刺、斩先高后低、舞沉髋且双臂不同时顶同一上限。写入 `/dev/cu.usbmodem83301` MAC `44:1b:f6:c1:8a:00` 应用分区 `0x4132a0` B（4,272,800），余量 17%。esptool `Hash of data verified`，RTS 重启。BIN SHA-256 `3b25a395791610b580818a9e8ae0c617ddc8436850c0d444dd138bafcad1b149`。ELF SHA-256 前缀 `61d1d6cdd` 与启动日志一致，Launcher 已起。
- **未验证：** 真机挥/举/社交/拳斩/舞是否能分清。

### 2026-09-18 · 手势尺度四刀

- **范围：** 十六段手势一起按 §6.5 重烙。胸相对髋放大；手势去掉踢球躯干地板；按动作选窗；拳/斩分段锁姿势；舞用 Root dip。
- **代码：** `retarget_bandai.py`；`kGestureRootDip`；`applyGesture` 沉 `pose.root.y`。Auton ID 0–5 不变。
- **已验证：** `bash tools/test_gundam_arena.sh`。
- **未验证：** 真机鞠躬/挥手/出拳/舞是否像对应动作。

### 2026-09-18 · 集 1 社交/战斗/表演进 A/B 圈

- **范围：** Auton 先暂停。把集 1 的 bow/bye/byebye/respond/call/guide、punch/slash、dance-long/short 烘焙进手势库，接在原 8 槽后面给人看。
- **代码：** `kPlayClipCount=18`；每段仍 50 帧 / 10800 B，脚钉地。Auton 技能 ID 0–5 不变。
- **已验证：** `bash tools/test_gundam_arena.sh`。
- **未验证：** 真机鞠躬/出拳/舞是否像对应动作。

### 2026-09-17 · Auton 动机竞争（去掉强制链）

- **范围：** 真机观感是 WAVE→KICK→LOOK 固定循环，没有思考。进场挥手、看球中途跳、看完举手都是脚本；Strike 用瞬时 desire=1。
- **代码：** 删除强制 WAVE/Leap/UP。Strike/Leap 用爬升的 `play`；够得着也要先看 `kStrikeThink=0.6s`。踢完仍强制看球 1.2s，之后才选举。Hold 在球停且操作者在时看镜头。技能分 ±0.02 噪声。
- **已验证：** `bash tools/test_gundam_arena.sh`；已烧录。
- **未验证：** 真机是否像在想。

### 2026-09-17 · 烧录查看（Auton 四刀）

- **范围：** 用户确认 §12 数值与三条公式偏差全部保留，要求烧录当前 Auton 四刀固件。
- **代码：** `c3c18d8`（五维动机）加设计文待批勾选；构建版本 `V0.5-220-gc3c18d8-dirty`（dirty 仅为设计文档勾选）。
- **写入：** `/dev/cu.usbmodem83301`，MAC `44:1b:f6:c1:8a:00`，仅 app 分区 `0x20000`，`0x3f67f0` B（4,155,376），余量 20%。esptool `Hash of data verified`，RTS 重启。BIN SHA-256 `6cbb53fc6dbe37db9843e0953e6e580ee9f64c39ba0c6afc63931eebc7e72052`。ELF SHA-256 前缀 `28d882aac` 与启动日志一致。
- **已验证：** PSRAM `SPI SRAM memory test OK`；`Launcher` on create / on open / `LauncherView` init；无 panic/重启循环。串口已释放。
- **未验证：** 真机五维观感、设备 FPS。

### 2026-09-17 · 真机（踢中后只会连踢）

- **范围：** 能踢中后不再挥手/跳/举手。进 Auton 出生点立刻 Strike；摇杆 Fault 无 operator 则 Signal 永 0；球 4.8m/s 飞出 1.8 跳圈；Approach 侧抑制 Leap。
- **代码：** 进 Auton 先 WAVE；`kLeapDist=4`；刚踢飞不压 Leap；看球 1.2s 结束若球仍快则 UP 2，不要求摇杆记忆。
- **已验证：** 主机测试通过；已烧录。
- **未验证：** 真机挥手/跳/举手。

### 2026-09-17 · 真机日志（空踢间隙）

- **范围：** 串口 `ArenaKick`：走开后同一站位连踢三次 `hit=false`，`minGapFwd=0.313>0.30`；出生点 `hit=true`，`minGapFwd=0.277`。站位 dist=0.038 已过门闩，前踢接触余量只有约 2cm。
- **代码：** `kFootR` 0.14→0.20（接触半径 0.36）；主机回放真机空踢位姿应踢中。保留 `ArenaKick` 日志。
- **已验证：** `bash tools/test_gundam_arena.sh`；回放真机空踢位姿 `struck`。写入 `/dev/cu.usbmodem83301` Hash verified。
- **未验证：** 真机第二脚。

### 2026-09-17 · 真机修正（空踢循环 / PAD FAULT）

- **范围：** 第一脚踢走后循环空踢、抬头闪、其它技能看不见。根因是 `inStrikeRange` 量到「按 `kickHeading` 反求的站位」，朝向一偏距离仍为 0；公差 0.22/0.28 空踢后仍过门闩，立刻再 Strike。`PAD FAULT` 是摇杆故障字，Auton 空档被盖住。
- **代码：** `inStrikeRange` 改为当前朝向的 `kickStance` 距离 0.12；走到快照站位后转到快照朝向；空踢 2s 抑制连踢；clip 结束把头角接到注视；Auton HUD 显示技能名，动作中显示 `KICK`。
- **已验证：** `bash tools/test_gundam_arena.sh`；当前朝向站位 0.05 外不进 Kick；对准 0.22 rad 不够踢；走到 2.6m 外的球第二脚 `struck`；精确站位能踢中。写入 `/dev/cu.usbmodem83301` MAC `44:1b:f6:c1:8a:00` app 分区 `0x3f6b50` B，Hash verified，Launcher 已起。
- **未验证：** 真机第二脚是否踢中、头是否还闪、挥手/跳/UP。

### 2026-09-17 · 真机修正（隔空踢 / 头闪 / 上抛）

- **范围：** 真机反馈踢走后隔空踢、头瞬切盯镜头、球不上天、看不见跳/挥手/UP。走近按朝向球锁定站位；`inStrikeRange` 加对准门闩；空踢不进玩心抑制；注视 0.15s 跟上；Attend 不看相机；踢完看球时附近空中球可 Leap；`struckRecent` 3s；踢球最小上抛 5。
- **代码：** `character_model.cpp` 的 `kickStanceAt` / `kickHeading` / `applyLookAt`；`idle_pilot.h` 的 Approach 快照与 attend 中 Leap。
- **已验证：** `bash tools/test_gundam_arena.sh`；未对准不进 `inStrikeRange`；注视首帧小于稳态；第三刀/四刀门闩用例仍过。写入 `/dev/cu.usbmodem83301` MAC `44:1b:f6:c1:8a:00` app 分区 `0x3f69a0` B，Hash verified，Launcher 已起。
- **未验证：** 用户再看真机隔空踢/头闪/上抛/跳/UP。

### 2026-09-17 · Auton 第四刀（五维动机）

- **范围：** Auton 用 vigilance / curiosity / play / social / composure 打分选举 Hold / Attend / Face / Approach / Strike / Leap / Signal。侧抑制与最短时间迟滞按设计 §5。`inStrikeRange` / `inLeapTrigger` / `operatorPresent` 仍是硬门闩。Kick 结束后强制 Attend 1.2s；Signal 结束后进 Hold。
- **代码：** `idle_pilot.h` 的 `updateDrives` / `scoreAutonSkills` / `electSkill`；Face 走已有 `faceYaw`。不改 `ArenaController`，不改玩家圈。
- **已验证：** `bash tools/test_gundam_arena.sh`；Strike/Leap/Signal 分为 0 时不请对应马达；身后球先 Face；两次 Strike 后短时间不连踢；第三刀门闩用例仍过。
- **未验证：** 真机五维观感、烧录与设备 FPS。

### 2026-09-17 · Auton 第三刀（Leap + Signal）

- **范围：** Auton 在 `inLeapTrigger` 时 `playJump()`（附近空中球、冷却 4s）；`operatorPresent` 时才 `playGesture`（进 Auton 打招呼 WAVE L/R，踢中且球仍快则 UP 2，冷却 12s）。注视相机仅 Signal。不上五维动机。
- **代码：** `idle_pilot.h` 增加 Leap/Signal 与操作者记忆；`ArenaController` 把跟随相机和环视 cue 传进 `stepIdlePilot`。智能跳/手势不写 `clipStep`。
- **已验证：** `bash tools/test_gundam_arena.sh`；地面球不跳、够踢不跳、附近下落球跳且圈不变、Land 后 4s 内不连跳、无操作者不挥手、先推杆再 Auton 挥手、踢中后 UP 2。
- **未验证：** 五维动机、真机跳/挥手观感、烧录与设备 FPS。

### 2026-09-17 · Auton 第二刀（走到站位再踢）

- **范围：** Auton 用固定优先级 Attend / Approach / Strike：球在地面则 `walkTo(kickStance)`，仅 `inStrikeRange` 时 `playKick()`。踢完强制看球 1.2s。回 Pilot 时清 `lookAt` 和 `walkTo`。不请 Jump、不做 Signal、不上五维动机。
- **代码：** `idle_pilot.h` 的 `autonAct`；站位目标在球移动或到达后才重设，避免每帧围着球转。智能踢球不写 `clipStep`。
- **已验证：** `bash tools/test_gundam_arena.sh`；约 3m 外先走/转再踢、身后先转不侧步、够不着不踢、Kick 期间保持 Strike、`clipIndex` 不变、踢完 1.2s 不连踢。
- **未验证：** Leap / Signal、真机自己走到踢球、烧录与设备 FPS。

### 2026-09-17 · Auton 第一刀（交接 + 注视球）

- **范围：** Play 下松杆 0.80s 进入 Auton，只把注视对准球心。推杆、着地 A/B、Pose 立刻回 Pilot 并清 `lookAt`。不请 Kick/Jump/手势，不走 `walkTo`，不上五维动机。
- **代码：** `idle_pilot.h` 的 `ControlMode` 与 `stepIdlePilot`；`ArenaController` 定步循环在 `stepCharacter` 之前调用。`valid==false` 且轴清零不回 Pilot。Auton 不改 `forward` / `turn` / `clipStep`。
- **已验证：** `bash tools/test_gundam_arena.sh`；主机测试覆盖未满 0.8s 仍 Pilot、侧面球头 yaw 同号、推杆当帧回正、Pose 无 Auton、失联保持 Auton、着地 `clipStep` 回 Pilot 且圈按玩家前进。
- **未验证：** Approach / Strike / Leap / Signal、真机松杆注视、烧录与设备 FPS。

### 2026-09-17 · 动作层前置（M1–M5）

- **范围：** 只补待机智能所需的动作层：独立播放入口、注视、寻路原语、踢球站位查询、`busy` 导出。不写 Auton / 动机层。不改 `ArenaController` 输入映射，玩家沙盒路径仍是 `clipStep` + 摇杆。
- **代码：** `clipIndex` 只表示玩家 A/B 圈；`playKick` / `playJump` / `playGesture(id)` 不拨圈。`characterBusy()` 覆盖 Kick / Gesture / 蓄力跳 / 空中 / Land，与 `stepCharacter` 共用。`lookAt` 仅 Idle/Walk/Turn 叠头；Kick/Jump/手势/落地不叠；`None` 当帧回正。`kickStance` 按当前朝向反解出生点局部偏移 `(-0.42, 0.74)`；`inStrikeRange` 用站位距离 0.22，不用球方位角门闩。玩家 `clipStep` 踢球不加该门闩。`faceYaw` / `walkTo` 在无过死区摇杆且非 busy 时收成内部前进/转向；busy 不移根；玩家一推杆清寻路目标。
- **已验证：** `bash tools/test_gundam_arena.sh`；`tools/gundam_arena_test.cpp` 含设计文档 §13.3 八条主机测试。现有 A/B 圈 `playSlot` 断言保持。
- **未验证：** Auton / 动机选举、松杆 0.8s 交接、Leap 冷却、空中注视、真机动作层观感、烧录与设备 FPS。

### 2026-09-17 · 首版沙盒（`3983c53`）

- **范围：** 新增 `app_gundam_arena`，共享输入增加 `ArenaPlay`，注册 Launcher App。
- **代码：** 绑定 RX-78 素体、Play/Pose、跟随相机、触屏分区、可选 Grove 手柄、主机测试与 PPM。
- **已验证：** `tools/gundam_arena_test.cpp` 与 device control 中的 Arena 用例（以该提交所含测试为准）。本轮文档撰写未重新跑测试。
- **未验证（当时）：** 目标构建、烧录、真机操作、外设故障、用户外观验收、设备帧率。
- **文档：** 建立本文，并在根 README 加入项目简介入口。

### 2026-09-17 · 真机验收（用户确认）

- **范围：** 用户完成 Arena 真机验收。代码未改。
- **已验证：** 用户在设备上验收通过。具体操作项、外观评语、帧率未另述，本文不补写。
- **未验证 / 未入库：** 固件哈希、烧录日志、设备 FPS、外设故障矩阵。
- **后续调研：** 对照 Pollen Robotics Microduck 的可获取运动数据（静态关节关键帧、官方 ONNX 策略、第三方仿真轨迹）。清单见对话记录，尚未接入 Arena。鸭子 14 舵机数据不匹配 19 骨，未采用。

### 2026-09-17 · Bandai 22 骨重定向与原地踢球

- **范围：** 选定 Bandai-Namco-Research-Motiondataset-1；离线 22→19 重定向；Play 增加 `Action::Kick`。
- **代码：** `tools/arena_motion/` 拉取/转换脚本；生成 `arena_kick_clip.h`；`stepCharacter` 在 Idle+confirm 播 clip，走/转+confirm 仍跳；HUD 显示 `KICK`；主机测试增加踢球峰值、走中起跳、`arena-kick.ppm`。
- **已验证：** 实际 BVH 层级为 22 节点（`joint_Root`…`Toes_R`）；转换器自检该层级；`retarget_bandai.py` 右大腿峰值 pitch 顶到限位 0.90，限位命中 3/738；`bash tools/test_gundam_arena.sh` 通过（`gundam_arena ok`，含 Kick 峰值、走中起跳、`arena-kick.ppm`）。
- **未验证：** 真机踢球观感、用户外观验收、walk clip 替换 IK、目标构建/烧录。

### 2026-09-17 · 机身 A/B 映射与小球

- **范围：** P5 小球；StopWatch 机身键可触发踢球/跳跃，不依赖 Joystick2 / Dual Button。
- **代码：** ArenaPlay 下 A 短按=`cancelPressed`→Kick，B 短按=`confirmPressed`→Jump，B 长按仍切 Pose；数字键在 `valid=false` 时仍生效。机体右前方 overlay 小球，踢中给一次前上冲量。HUD 待机显示 `A KICK  B JUMP`。
- **已验证：** `bash tools/test_gundam_arena.sh` 通过；device_control 测试确认 Arena 下 A 短按=`cancelPressed`、B 短按=`confirmPressed`。
- **未验证：** 真机 A/B 手感、球与脚的遮挡精度。

### 2026-09-17 · P5 两轮自检与 P6 walk 对照

- **P5 第1轮：** Pose 会冻球；冲量只沿朝向飞向默认相机；接触锥不用右脚；深度裁剪会把待机球裁掉。
- **P5 修复：** Pose 也 `stepBall`；接触相对右脚；冲量沿球相对骨盆方向；待机球在右前方；overlay 不再按骨盆深度裁掉。
- **P5 第2轮：** 击中一次、身后不中、落地回半径、Pose 中球继续飞、idle/kick PPM 有橙色像素、occupancy 仍 0.45–0.55。`gundam_arena ok`。
- **P6：** `arena_walk_clip.h`（60 帧）只给主机对照，固件 `Walk` 仍用 `walkPhase` IK。转换器 walk 限位命中 0/1080。
- **未验证：** P7 真机。

### 2026-09-17 · 烧录查看（P7 写入）

- **范围：** 用户要求烧录当前 Arena 踢球/小球固件。
- **写入：** `/dev/cu.usbmodem83301`，MAC `44:1b:f6:c1:8a:00`，仅 app 分区 `0x20000`，`0x3e3d90` B（约 4,078,992），余量 21%。esptool `Hash of data verified`，RTS 重启。
- **启动：** PSRAM test OK，ELF SHA256 `3cb1c6bc9…`，`Sep 17 2026 11:05:49`，进入 Launcher。串口已释放。
- **未验证：** 用户踢球观感、A/B 手感、设备 FPS。

### 2026-09-17 · P7 真机：抬脚与球不同侧；跳跃无 mocap

- **现象：** 球在画面右脚前，clip 抬的是另一只脚。跳跃观感与接入踢球前相同。
- **原因：** 球在 Arena `-X`（`LThigh`），烘焙把 BVH 右腿写到 `RThigh`。跳跃从未接 clip；Bandai 集 1 没有 jump 文件，`applyJumpPose` 仍是程序收腿。
- **修正：** `retarget_bandai.py` 烘焙后对调四肢 L/R，踢腿落到 `LThigh`；主机断言改为左大腿峰值更高。跳跃仍不改。
- **已验证：** 转换器自检左大腿 pitch 高于右大腿；`bash tools/test_gundam_arena.sh` 通过（`gundam_arena ok`，occupancy=0.450644）。
- **未验证：** 用户再看踢球是否同脚。跳跃若要更明显需另做程序起跳。

### 2026-09-17 · 烧录查看（左右脚对调后）

- **范围：** 用户要求烧录对调踢腿后的固件。
- **写入：** `/dev/cu.usbmodem83301`，MAC `44:1b:f6:c1:8a:00`，仅 app 分区 `0x20000`，`0x3e3d90` B（约 4,078,992），余量 21%。esptool `Hash of data verified`，RTS 重启。
- **镜像：** `V0.5-210-g043f882-dirty`，ELF SHA256 `39728067c211291b…`，编译日 `Sep 17 2026`。
- **未验证：** 用户踢球是否同脚。

### 2026-09-17 · P7 真机：脚未碰到球就飞

- **现象：** 左右对了之后，脚还没碰到球，球已经飞走。
- **原因：** 接触用 clip 时间窗 0.42–0.85 + 假想脚点，半径 0.90；真实 `LFoot` 当时还在身后。`+pitch` 还会把 BVH 前踢甩向 `-Z`，球却在 `+Z`。
- **修正：** 重定向矢状面取 `-footZ`；球移到踢腿轨迹 `(-0.42, 0.74)`；每帧用左脚踝→趾胶囊扫掠相交，碰到才给冲量。主机断言 `clipT<0.50` 时尚未击中。
- **已验证：** `bash tools/test_gundam_arena.sh` 通过（`gundam_arena ok`，occupancy=0.450644）；`clipT<0.50` 时未击中。
- **未验证：** 用户看脚是否真正碰到再飞。

### 2026-09-17 · 烧录查看（脚球碰撞）

- **范围：** 用户要求烧录真实脚球碰撞固件。
- **写入：** `/dev/cu.usbmodem83301`，MAC `44:1b:f6:c1:8a:00`，仅 app 分区 `0x20000`，`0x3e4d30` B（约 4,082,992），余量 21%。esptool `Hash of data verified`，RTS 重启。
- **镜像：** `V0.5-210-g043f882-dirty`，ELF SHA256 `7288a1a9ed695d19…`，编译日 `Sep 17 2026`。
- **未验证：** 用户看脚是否碰到再飞。

### 2026-09-17 · 背景不再纯黑

- **范围：** 用户觉得 Arena 纯黑太暗。
- **修正：** 清屏改为暗石板蓝灰 `0x298A`；网格/边线改为 `0x8410` / `0x9CD3`，避免在浅一点的底上过亮。机体 blit 仍只覆盖有深度的像素。
- **已验证：** `bash tools/test_gundam_arena.sh` 通过（`gundam_arena ok`，occupancy=0.450644）。
- **未验证：** 用户观感。

### 2026-09-17 · 踢球改为先收腿再前踢

- **现象：** 前摆就把球踢走，随后后收，不像真实踢球。
- **原因：** 窗口 40–80 几乎没有蓄力，却包含踢完后的后收；碰撞又打在第一次前摆上。
- **修正：** clip 改成收腿蓄力 → Bandai 48–62 前摆 → 收到站立；碰撞要求脚向前摆。`kKickFrames=44`。
- **已验证：** `bash tools/test_gundam_arena.sh` 通过（`gundam_arena ok`）；收腿阶段 `LThigh` 峰值 > 0.40，且 `clipT<0.55` 时未击中。
- **未验证：** 用户踢球节奏。

### 2026-09-17 · 烧录查看（先收腿再前踢）

- **范围：** 用户要求烧录收腿后前踢固件。
- **写入：** `/dev/cu.usbmodem83301`，MAC `44:1b:f6:c1:8a:00`，仅 app 分区 `0x20000`，`0x3e5040` B（约 4,083,776），余量 21%。esptool `Hash of data verified`，RTS 重启。
- **镜像：** `V0.5-210-g043f882-dirty`，ELF SHA256 `51ff8162f2cf8da1…`，编译日 `Sep 17 2026`。
- **未验证：** 用户踢球节奏。

### 2026-09-17 · 黄球透视遮挡

- **现象：** 黄球是机体 blit 之后的 `fillCircle`，不考虑深度，叠在机体上。
- **修正：** 球用机体同一相机做成朝向镜头的圆盘，写入 `CarSurfaceRaster` 深度缓冲后再 blit。
- **已验证：** `bash tools/test_gundam_arena.sh` 通过（`gundam_arena ok`，occupancy=0.450644）；机体近侧球比躯干后球橙色像素更多。
- **未验证：** 用户真机遮挡观感。

### 2026-09-17 · 跳跃改为弹道姿势

- **现象：** 跳跃像后仰坐在空中：大腿正 pitch 把腿甩向身后，手臂也后摆，顶点突然换姿势。
- **修正：** 按 `vy` 与起跳时间在深蹲→蹬伸→顶点微收→伸腿落地之间插值；膝向前；落地 0.20s 收回。
- **已验证：** `bash tools/test_gundam_arena.sh` 通过（`gundam_arena ok`，occupancy=0.450644）；起跳大腿 pitch < 0，空中小腿会落到 < 0.40，手臂前摆 > 0.20。
- **未验证：** 用户真机跳跃观感。

### 2026-09-17 · 烧录查看（弹道跳跃 + 球遮挡）

- **范围：** 用户要求烧录当前跳跃算法与黄球透视遮挡固件。
- **写入：** `/dev/cu.usbmodem83301`，MAC `44:1b:f6:c1:8a:00`，仅 app 分区 `0x20000`，`0x3e5710` B（4,085,520），余量 21%。esptool `Hash of data verified`，RTS 重启。
- **镜像：** `V0.5-211-g66346cf-dirty`，ELF SHA256 `9dcf97a02d02bb37…`，编译日 `Sep 17 2026`。
- **未验证：** 用户跳跃/遮挡观感。

### 2026-09-17 · 跳跃增加地面下蹲蓄力

- **现象：** 一按就离地，没有下蹲攒力，看起来僵。
- **修正：** 起跳先在地面蹲 0.20s（0.12s 蹲满再停一下），位移钉住，然后再抛起。
- **已验证：** `bash tools/test_gundam_arena.sh` 通过（`gundam_arena ok`，occupancy=0.450644）；起跳后仍着地且小腿 > 0.60，随后才离地。
- **未验证：** 用户真机蓄力观感。

### 2026-09-17 · 烧录查看（地面下蹲蓄力）

- **范围：** 用户要求烧录带地面下蹲蓄力的跳跃固件。
- **写入：** `/dev/cu.usbmodem83301`，MAC `44:1b:f6:c1:8a:00`，仅 app 分区 `0x20000`，`0x3e5810` B（4,085,776），余量 21%。esptool `Hash of data verified`，RTS 重启。
- **镜像：** `V0.5-211-g66346cf-dirty`，ELF SHA256 `7e37b6cec1725702…`，编译日 `Sep 17 2026`。
- **已验证：** 用户确认本阶段先这样。
- **未继续：** 跳跃/踢球仍可再打磨，不在本计划内。

### 2026-09-17 · Bandai 踢球计划收口

- **范围：** 用户要求阶段性提交，并把 §13 P0–P7 标记完成。
- **结果：** 原地踢球 clip、A/B、真实脚碰球、黄球深度遮挡、程序跳跃（地面下蹲蓄力）均已进当前固件。walk 仍用钉地 IK。
- **本阶段不做：** 换 jump mocap、用 Bandai walk 替换 IK、篮球尺寸、push。

### 2026-09-17 · 跳跃专项：按 CMU 13_39 修正后仰

- **现象：** 蓄力/落地骨盆 pitch −0.18，胸 −0.12，胸口相对骨盆 ΔZ 为负，看起来后仰。
- **原因：** Bandai 集 1 没有 jump。CMU `13_39` 蹲姿骨盆是前倾（约 +0.04）；Arena 写反了符号。
- **修正：** 程序键位改为 CMU 映射且留在限位内：蓄力骨盆 +0.12 / 胸 +0.10 / 大腿 −0.34 / 小腿 0.58；蹬伸臂 +0.45；顶点大腿 −0.36 / 小腿 0.72（不打满 −0.80 / 1.80）；落地仍前倾 +0.10。不烤 clip、不改骨架。
- **已验证：** `bash tools/test_gundam_arena.sh` 通过（`gundam_arena ok`，occupancy=0.450644）；蓄力骨盆/胸 pitch > 0，空中不再后仰，落地仍前倾。
- **未验证：** 用户真机跳跃观感。

### 2026-09-17 · 烧录查看（CMU 前倾跳跃）

- **范围：** 用户要求写入修正后仰后的跳跃固件。
- **写入：** `/dev/cu.usbmodem83301`，MAC `44:1b:f6:c1:8a:00`，仅 app 分区 `0x20000`，`0x3e56a0` B（4,085,408），余量 21%。esptool `Hash of data verified`，RTS 重启。
- **镜像：** `V0.5-212-g6b88213-dirty`，ELF SHA256 `485fdab0f172834a…`，编译日 `Sep 17 2026`。
- **未验证：** 用户真机跳跃观感。

### 2026-09-17 · 跳跃：人体角 1:1 接到 SD，蹲小跳高

- **现象：** 修正前倾后，蓄力幅度在 SD 上几乎看不出，整机却跳得很高。
- **原因：** CMU 蹲主要靠髋下沉；重定向 IK 把髋锁在 `kHipY`，只留下微屈膝（大腿 −0.34，和走路站姿 −0.33 同一档）。`kJumpVel=6.2` 顶点 1.07，约 0.6 个身长（真人原地跳约 0.24）。
- **修正：** 蓄力/落地钉脚 + Root 下沉（0.14 / 0.10）再 IK；`kJumpVel=4`（顶点约 0.44）。规则写入 §6.5，踢球重定向注释和 §13.1 同步，避免其它动作再抄人体角。
- **已验证：** `bash tools/test_gundam_arena.sh` 通过（`gundam_arena ok`，occupancy=0.450644）；蓄力 Root.y < −0.08、大腿 < −0.50，顶点 `c.y` 在 0.20–0.70。
- **未验证：** 用户真机蹲深和跳高。

### 2026-09-17 · 烧录查看（SD 尺度跳跃）

- **范围：** 用户要求写入沉髋蓄力、压低弹道后的固件，并把人体角 1:1 的坑记进文档。
- **写入：** `/dev/cu.usbmodem83301`，MAC `44:1b:f6:c1:8a:00`，仅 app 分区 `0x20000`，`0x3e5780` B（4,085,632），余量 21%。esptool `Hash of data verified`，RTS 重启。
- **镜像：** `V0.5-212-g6b88213-dirty`，ELF SHA256 `c75c1ca74e517525…`，编译日 `Sep 17 2026`。
- **未验证：** 用户真机蹲深和跳高。

### 2026-09-17 · 跳跃：加大空中屈膝和摆臂

- **现象：** 沉髋后整体观感对了，空中屈膝和摆臂仍偏小。
- **原因：** 贴 CMU 映射角把臂写成 −0.15 / +0.55（几乎走路档），顶点收腿写成 −0.36 / 0.72；源动作顶点其实会打满限位。地面蹲已接近大腿上限，不能再拧。
- **修正：** 蓄力后摆 −0.85，蹬伸前摆 0.80，顶点 −0.55 / 1.10。蹲深和 `kJumpVel` 不动。
- **已验证：** `bash tools/test_gundam_arena.sh` 通过（`gundam_arena ok`，occupancy=0.450644）；蓄力臂 < −0.40，空中臂 > 0.55。
- **未验证：** 用户真机屈膝/摆臂。

### 2026-09-17 · 烧录查看（加大屈膝摆臂）

- **范围：** 用户要求再调一版空中屈膝和摆臂。
- **写入：** `/dev/cu.usbmodem83301`，MAC `44:1b:f6:c1:8a:00`，仅 app 分区 `0x20000`，`0x3e5780` B（4,085,632），余量 21%。esptool `Hash of data verified`，RTS 重启。
- **镜像：** `V0.5-212-g6b88213-dirty`，ELF SHA256 `9b125730f71cc5af…`，编译日 `Sep 17 2026`。
- **已验证：** 用户确认效果不错；§6.5 补上短肢放大规则。

### 2026-09-17 · 踢球后仰：人体配平 1:1 写进骨盆/胸

- **现象：** 跳跃后仰已修，踢球出脚时仍明显后仰。
- **原因：** Bandai 踢腿前摆时胸口落到髋后方（骨盆 −0.05、胸 −0.09），人是用后仰配平前踢。收腿段 clip 是前倾的，问题只在出脚。
- **修正：** `sd_kick_torso` 烘焙后把骨盆/胸钳在前倾，出脚再加到约 +0.16 / +0.18；头颈不再跟着后仰。不手改 clip。
- **已验证：** `bash tools/test_gundam_arena.sh` 通过（`gundam_arena ok`，occupancy=0.450644）；踢球全程骨盆/胸 pitch > 0，出脚时 > 0.05。用户真机确认后仰已去掉。

### 2026-09-17 · 烧录查看（踢球去掉后仰）

- **范围：** 用户要求修正踢球后仰。
- **写入：** `/dev/cu.usbmodem83301`，MAC `44:1b:f6:c1:8a:00`，仅 app 分区 `0x20000`，`0x3e5780` B（4,085,632），余量 21%。esptool `Hash of data verified`，RTS 重启。
- **镜像：** `V0.5-213-g8d5784a-dirty`，ELF SHA256 `8e5c938ff119e2a7…`，编译日 `Sep 17 2026`。
- **已验证：** 用户真机确认后仰已去掉。

### 2026-09-17 · 踢球胳膊多余动作：人体摆臂 1:1 写进上臂/前臂

- **现象：** 踢球后仰修了之后，胳膊仍有很多多余动作。
- **原因：** 收腿是手写安静臂姿（踢侧 +0.20、支撑 −0.50、肘 −0.35/−0.55）；出脚 48–62 抄 Bandai 手部 IK。人踢腿时双臂泵摆、肘收到约 −1.80。拼上之后踢侧上臂先甩到 −0.56 再回到 +0.32，支撑肘后半段从 −0.46 抽到 −1.80。
- **修正：** 丢掉手部 IK。收腿、出脚各锁一个臂姿，出脚全程手臂保持不动；不要按大腿过零点驱动，否则中间会掉回 idle 再抬起来。
- **已验证：** 烘焙脚本自检踢侧上臂不前甩、肘不深折、出脚段臂角冻结；`bash tools/test_gundam_arena.sh` 通过（`gundam_arena ok`，occupancy=0.450644）。用户真机确认胳膊不再来回甩。

### 2026-09-17 · 烧录查看（踢球去掉多余摆臂）

- **范围：** 用户要求烧录去掉踢球胳膊多余动作的固件。
- **写入：** `/dev/cu.usbmodem83301`，MAC `44:1b:f6:c1:8a:00`，仅 app 分区 `0x20000`，`0x3e5780` B（4,085,632），余量 21%。esptool `Hash of data verified`，RTS 重启。
- **镜像：** `V0.5-213-g8d5784a-dirty`，ELF SHA256 `a2debb39701e1f14…`，编译日 `Sep 17 2026`。
- **已验证：** 用户真机确认胳膊不再来回甩。

### 2026-09-17 · 站球移动 P0 + Bandai 2 标签 P1.1

- **范围：** 默认玩法改为站在加大后的球顶上滚走（microduck-basketball 合同，程序约束）；Kick 降为关 `onBall` 才测的 clip。并行只拉 Bandai 2 cfg 标签，不下 BVH。不烧录（P0.6 无摇杆挂起）。
- **代码：** `kBallR=0.50`，`kBallWalkSpeed=1.4`，开局 `ball=(0,R,0)`、`onBall=true`、Root `kBallRootY`。输入驱动球 xz，机体跟顶点；`applyBallStance` 双脚上半球 IK；`stepBall` 站球时跳过扫掠/抛体；A/B 站球忽略。HUD `BALL`。主机 `groundPlay()` 关站球测走/跳/踢 clip。新增 `arena-ball-top.ppm` / `arena-ball-side.ppm`。`tools/arena_motion/fetch_bandai2_index.py` 拉 `cfg/content_label.txt`、`style_label.txt` 到 gitignore 的 `raw/`。
- **已验证：** `bash tools/test_gundam_arena.sh` → `gundam_arena ok`，occupancy=0.450644。出生在球顶、直行滚走、零输入停、转向不掉、站球 A 不进 Kick、关 onBall 仍能播踢 clip。
- **Bandai 2 标签（权威 cfg，不是 README Contents）：** 10 内容 × 7 风格。内容：`walk`、`walk-turn-left`、`walk-turn-right`、`run`、`wave-both-hands`、`wave-left-hand`、`wave-right-hand`、`raise-up-both-hands`、`raise-up-left-hand`、`raise-up-right-hand`。风格：`active`、`elderly`、`exhausted`、`feminine`、`masculine`、`normal`、`youthful`。README Contents 把第 9/10 类都写成 `raise-up-right-hand`；cfg 与 Visualization 一致，第 9 类是 **`raise-up-left-hand`**，第 10 类是 `raise-up-right-hand`。歧义消除。本轮未下 `data.zip` / BVH。

### 2026-09-17 · 烧录查看（站球开局）

- **范围：** 用户要求烧录站球默认玩法，看人是否站在加大后的球顶上。
- **写入：** `/dev/cu.usbmodem83301`，MAC `44:1b:f6:c1:8a:00`，仅 app 分区 `0x20000`，`0x3e5a40` B（4,086,336），余量 21%。esptool `Hash of data verified`，RTS 重启。
- **镜像：** `V0.5-214-ge09ef64-dirty`。
- **已验证：** 写入与重启成功。
- **未验证：** 用户真机站球观感；无 Joystick2 时走/转可用触屏虚拟摇杆。站球时 A/B 不踢不跳。

### 2026-09-17 · 放弃踩球，改回踢球

- **范围：** 用户明确放弃站在球上滚走。运行时、测试、HUD 从 `e09ef64` 踢球实现恢复：`kBallR=0.16`，出生点脚前 `kBallSpawnX/Z`，A 短按 Kick 且脚碰球才击飞，B 短按跳。
- **计划：** `docs/Gundam-Arena-Bandai2-动作库与踩球计划.md` 工作流 B / P0 / P3 全部取消，不再推进。Bandai 2 的 10+10 动作库（工作流 A）仍有效；P1.1 标签结论保留。
- **已验证：** `bash tools/test_gundam_arena.sh` → `gundam_arena ok`，occupancy=0.450644。踢球回归（出生脚前小球、A 进 Kick、脚碰球才击飞）已恢复。

### 2026-09-17 · 烧录查看（改回踢球）

- **范围：** 用户要求把踢球固件烧回去，覆盖上一版站球镜像。
- **写入：** `/dev/cu.usbmodem83301`，MAC `44:1b:f6:c1:8a:00`，仅 app 分区 `0x20000`，`0x3e5780` B（4,085,632），余量 21%。esptool `Hash of data verified`，RTS 重启。
- **已验证：** 写入与重启成功。
- **未验证：** 用户真机踢球观感。

### 2026-09-17 · A/B 循环 + Bandai 2 六手势

- **范围：** P1.2 下 10+10 条 BVH；P1.3 确认集 2 与集 1 同 22 骨 / ZXY；P2.1 通用烘焙；P4 六手势进固件。Play 下 A 下一个、B 上一个，圈内 Kick/Jump/六手势。不烧录。
- **代码：** `fetch_bandai2.py`、`arena_gesture_clips.h`（6×50 帧）、`clipStep` 边沿、HUD 槽名。walk/run/turn 仅 `dataset2_preview.txt`。
- **已验证：** `bash tools/test_gundam_arena.sh` → `gundam_arena ok`，occupancy=0.450644；device-control A/B 仍是 cancel/confirm。
- **未验证：** 用户真机手势观感。

### 2026-09-17 · P5 风格变体（不烧录）

- **范围：** 对比计划里的风格备选，只把更好的写进固件手势库。walk/run/turn 风格只进离线报告。不烧录。
- **取舍：** WAVE L/R 换成 `active`（峰值能量 2.40→3.32、2.36→3.14，限位未变差）。WAVE 2 `youthful`、UP 2 `active` 因双臂同时打满上限拒绝。UP L/R 没有风格备选。
- **体积 / 帧时：** 每段 50 帧 = 10800 B；主机渲染中位 716–759 µs。设备 FPS / 固件哈希未测。
- **已验证：** `bash tools/test_gundam_arena.sh` → `gundam_arena ok`，occupancy=0.450644；中段手掌距头 >0.16、肩距 >0.90、脚 y <0.40。
- **未验证：** 用户真机风格观感。

### 2026-09-17 · 烧录查看（A/B 循环 + P5 手势）

- **范围：** 用户要求烧录当前 A/B 循环圈和六手势（WAVE L/R 为 `active`）固件，看真机效果。
- **写入：** `/dev/cu.usbmodem83301`，MAC `44:1b:f6:c1:8a:00`，仅 app 分区 `0x20000`，`0x3f56f0` B（4,151,024），余量 20%。相对上一版踢球固件 `0x3e5780` 大约多 65 KiB（六段手势数组）。esptool `Hash of data verified`，RTS 重启。
- **镜像：** `V0.5-214-ge09ef64-dirty`，ELF SHA256 `ec0f292b3c826307…`，编译日 `Sep 17 2026`。
- **已验证：** 写入与重启成功。
- **未验证：** 用户真机 A/B 循环与手势观感；设备 FPS。

### 2026-09-17 · 踢球去掉半途回抽

- **范围：** 真机反馈前踢半途收回再小踢。核对 clip：Bandai 48–62 经 IK 后小腿先折到 1.63 再弹到 0.46 再折到 1.80。
- **修正：** 丢掉这段膝鞭。收腿后作者姿势一次伸到 `LThigh=-0.80` / `LShin=0.50`，前摆小腿不得再折。`kKickFrames=39`。
- **已验证：** `bash tools/test_gundam_arena.sh` → `gundam_arena ok`，occupancy=0.450644；前摆小腿单调伸开，脚碰球仍击飞一次。用户 2026-09-17 确认真机踢球连贯。

### 2026-09-17 · 烧录查看（一次前踢）

- **范围：** 用户要求烧录去掉半途回抽后的踢球固件。
- **写入：** `/dev/cu.usbmodem83301`，MAC `44:1b:f6:c1:8a:00`，仅 app 分区 `0x20000`，`0x3f52b0` B（4,149,936），余量 20%。esptool `Hash of data verified`，RTS 重启。
- **镜像：** `V0.5-214-ge09ef64-dirty`，ELF SHA256 `4b6746db43159630…`，编译日 `Sep 17 2026`。
- **已验证：** 写入与重启成功。用户 2026-09-17 确认真机踢球连贯。

---

## 13. Bandai → Arena 运动计划

目标：用与 19 骨最接近的公开人形 mocap，先让 RX-78 **原地踢一脚**。不改骨架拓扑，不在设备上解析 BVH，不上 PPO。

### 13.1 冻结规格

| 项 | 决定 |
|---|---|
| 数据 | 集 1，`dataset-1_kick_normal_001.bvh` + 对照用 `walk_normal_001`（walk 不替换 IK） |
| 骨架 | Arena 19 骨不变 |
| Bandai 22 节点 | `joint_Root, Hips, Spine, Chest, Neck, Head, Shoulder/UpperArm/LowerArm/Hand L+R, UpperLeg/LowerLeg/Foot/Toes L+R` |
| 丢掉 | `joint_Root` 占位；`Spine` 折进 Chest/Pelvis；`Toes_*` 折进 Foot |
| 一对一 | Hips→Pelvis，Chest，Neck，Head，肩臂手，大腿/小腿/脚 |
| 通道 | BVH 每关节 6 通道；旋转序 `ZXY`，FK 为 `Rz*Rx*Ry`（已用站立脚高/踢腿峰值核对） |
| 休息姿势 | 不抄 BVH 欧拉。先 FK 出世界坐标，再按 Arena 矢状面双骨 IK 求解 |
| 比例 | 髋高缩到 `kHipY=1.02`；**下蹲/跳高走 §6.5，禁止人体关节角 1:1**；开踢帧左右髋对齐朝向；四肢 L/R 对调到球侧 `LThigh`；矢状面用 `-footZ` 使前踢对上 Arena `+Z` |
| 限位 | 写入前 `limitOf()`；SD 踢腿会被大腿 pitch 0.90 钳住 |
| 运行时 | `帧 × 18 关节 × 3 欧拉` 的 C 数组，30 fps 插值到 60 Hz 步进 |
| 许可 | CC BY-NC 4.0；`tools/arena_motion/raw/` gitignore |

### 13.2 阶段

| 阶段 | 内容 | 状态 |
|---|---|---|
| P0 规格 | 骨对照、折叠、clip 格式、许可 | 完成 |
| P1 数据 | 拉取 kick/walk BVH，核 22 骨 | 完成（仅本地 `raw/`） |
| P2 转换器 | `retarget_bandai.py`：收腿 + 一次作者前踢；Bandai 48–62 膝鞭丢弃 | 完成 |
| P3 播放 | `Action::Kick`，Idle+B 触发，HUD `KICK` | 完成 |
| P4 回归 | 主机测试 + `arena-kick.ppm` | 完成 |
| P5 球道具 | 小半径球体、踢球瞬间给速度 | 完成（两轮自检） |
| P6 可选 walk clip | 仅对照，默认仍 IK | 完成 |
| P7 真机 | 用户验收踢球剪影与 A/B 操作 | 完成（2026-09-17 用户确认本阶段先这样） |

### 13.3 明确不做

- 不把 Microduck ONNX/CSV 接到 Gundam
- 不在 ESP32 上跑 BVH/PPO
- 不增加 Spine/Toes 骨
- 不把 Bandai walk 替换钉地 IK（walk 会滑步）
- 不把 BVH 原件推进 git
- 不把人体关节角 1:1 写进 SD（见 §6.5）

### 13.4 重做 clip

改窗口或映射后只跑 `retarget_bandai.py`，再跑 `tools/test_gundam_arena.sh`。不要手改 `arena_kick_clip.h`。

