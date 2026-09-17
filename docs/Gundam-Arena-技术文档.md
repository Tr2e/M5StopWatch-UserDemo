# Gundam Arena 技术文档

> 本文是 Gundam Arena 的**持续技术记录**。项目简介、当前实现、技术细节、验证边界和后续迭代都写在这里，不另开平行主文档。
>
> 当前代码基线：`feat/gundam-arena` @ `3983c53`（2026-09-17）。相对 `feat/gundam-museum` 仅此一个提交。
>
> 本文记录代码事实与主机测试承诺；**不代表真机外观验收、设备帧率或外设故障场景已经完成。**

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

已实现、可在主机测试验证：

- 独立 App 注册，进入后停 LVGL、直绘或画布回读
- 19 骨骨架、绑定网格、每帧蒙皮求值
- Play：走、转、跳、落地；支撑脚钉地 IK、对侧摆臂
- Pose：循环选关节、拖动欧拉角、单关节复位、角度限位
- 第三人称跟随相机；Play 模式触屏上半区环视
- 触屏虚拟摇杆 + 机身键 + 可选 Joystick2 / Dual Button
- 主机测试覆盖骨骼、运动、渲染、输入合并

明确未做：

- 真机烧录记录、设备 FPS、用户视觉验收
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
| Jump | 确认键且着地 | 起跳速度 6.2，重力 18 |
| Fall | 空中且 `vy≤0` | 收腿姿势略放松 |
| Land | 落地后 0.10s | 屈膝缓冲，期间不响应新走/跳 |

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
| 跳 / 复位关节 | — | B 短按 = confirm | Dual Button B 短按 |
| 切 Play/Pose | — | B 长按 = pause | Dual Button B 长按 |
| 选关节 | 左右箭头 `previous`/`next` | — | Pose 时水平导航步进 |
| 退出 App | — | A+B 和弦 | 外设退出和弦 |
| 回 Launcher | 机身 Home（`KeyManager::GoHome`） | 同左 | — |

触屏细节：

- 下半区按下立即作为虚拟摇杆，松手轴回中
- 上半区拖满 6 像素后成为环视，不与移动轴同时生效
- Pose 左右热区与博物馆相同：`previous{8,201,48,64}`、`next{412,201,48,64}`
- 触屏任务 20ms，按键任务 10ms；消费时按键超时 100ms、触屏超时 150ms 则判定输入不健康，轴清零

手柄合并 `mergeExternalPad`：外设有效时覆盖 `steer`/`viewAxis`；确认/暂停/退出做或；导航步进有值才覆盖。Grove 5V 打不开则只走触控，日志警告。

顶部状态字：校准中 `CAL`，就绪 `PAD`，故障 `PAD FAULT`。Pose 模式覆盖为关节名。

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

上升收腿更紧（小腿 tuck 0.55），下落 0.20；双臂后摆。落地屈膝约 0.16–0.20 / 0.50–0.55。

### 6.4 相机

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

1. 清屏黑底 `0x0000`
2. 画 32 分割地面网格，半边长 20；近平面 0.20 裁线后投影
3. `evaluateSkeleton(pose)`
4. 按骨骼变换后的法线做背面剔除（阈值 −0.035）；双面面不剔
5. 两遍提交：背面 pass 0，正面 pass 1（减轻逆深度边缘翻面）
6. `MuseumProjectionCache` 按「同位置 + 同骨骼」共享顶点；近平面失败则回退逐顶点 `prepareCarPanel`
7. `CarSurfaceRaster<424,424>` 开 `setSolidFastPath(true)`，再 `blitScaled` 到 `( (W-424)/2 , 21 , 424, 424 )`
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
  model/character_model.{h,cpp}      模式/动作状态机、走路 IK
  model/rx78_bones.h                 骨、限位、仿射、姿态类型
  model/rx78_skeleton.cpp            绑定、FK、网格局部化
  model/rx78_rigged.cpp              素体网格 + 骨标签
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

输出 PPM（生产 renderer，不是设备截屏）：`arena-idle.ppm`、`arena-head.ppm`、`arena-walk.ppm`、`arena-turn.ppm`、`arena-far.ppm`、`arena-jump.ppm`、`arena-lookup.ppm`。

共享触控回归：`tools/lets_and_go_device_control_test.cpp` 在 `ArenaPlay` 下检查前进轴、转向、松手回中、环视不抢移动、左右箭头仍在；并确认 Racing 不会漏进 Arena 的前进轴。

### 10.2 目标构建与真机

本基线提交**没有**写入固件哈希、烧录日志或设备帧率。需要真机时按仓库常规 `idf.py build` / `idf.py flash`，验收不得用主机 PPM 或测试通过代替。

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
- 没有独立 Launcher 图标
- `rx78_assembly.h` 悬空包含
- 未做 Arena 专属视觉制作卡；机体外形若要按官方板件精修，应另开节点，并说明与博物馆网格是否继续分叉

---

## 12. 迭代记录

后续改动按时间追加本节，不新开主文档。每条写：日期、范围、代码事实、验证了什么、**没有**验证什么。

### 2026-09-17 · 首版沙盒（`3983c53`）

- **范围：** 新增 `app_gundam_arena`，共享输入增加 `ArenaPlay`，注册 Launcher App。
- **代码：** 绑定 RX-78 素体、Play/Pose、跟随相机、触屏分区、可选 Grove 手柄、主机测试与 PPM。
- **已验证：** `tools/gundam_arena_test.cpp` 与 device control 中的 Arena 用例（以该提交所含测试为准）。本轮文档撰写未重新跑测试。
- **未验证：** 目标构建、烧录、真机操作、外设故障、用户外观验收、设备帧率。
- **文档：** 建立本文，并在根 README 加入项目简介入口。
