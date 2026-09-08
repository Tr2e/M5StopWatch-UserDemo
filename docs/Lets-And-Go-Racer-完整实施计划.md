# Let's & Go!! Racer：完整实施计划

> 二次审查进展：R11 修正比赛与输入逻辑；R12 修正生产画面、桥面遮挡及渲染栈占用。详见同目录两份审查记录。R0–R10 的阶段完成记录不替代后续缺陷修正与真机验收。

> 后续 R13 增加 384 场输入策略回归，修正慢车型追逐难度；当前测试门禁为 28 套。详见 `Lets-And-Go-Racer-R13-可玩性回归.md`。

> 实施分支：`feat/lets-and-go-racer`
>
> 基线：`github/feat/vector-canyon-fighter` 的远端最新提交 `0f5b1ff`
>
> 目标设备：M5Stack StopWatch，ESP32-S3 240 MHz，466×466 圆形 AMOLED，PSRAM，Joystick2 + Dual Button
>
> 产品边界：仅供设备所有者私人使用；车辆名称、配色和轮廓按《爆走兄弟 Let's & Go!!》/田宫官方模型还原，不作公开发布资产。

## 1. 产品目标

在 StopWatch 上实现一个完整的手绘线框迷你四驱车比赛闭环：

1. 进入 App 后检测并校准 Joystick2 + Dual Button。
2. 从四辆官方车型中选择玩家车辆。
3. 以高细节伪 3D 线框预览车辆，展示轮胎、导轮、马达振动和逐风效果。
4. 从剩余车辆中选择 0～3 名对手。
5. 在赛道页选择首版唯一的无中间隔板立交环形赛道。
6. 玩家固定从最后一位发车，完成三圈追逐比赛。
7. 结算页支持保留设置重赛、返回车库和退出 App。

App 显示名称固定为 `Let's & Go!!`，Launcher 图标使用田宫 TAMIYA 红蓝双星标志的圆屏适配版。

## 2. 首发车辆

| 车辆 | 内部 ID | 高辨识度结构 | 预设倾向 |
| --- | --- | --- | --- |
| Cyclone Magnum | `cyclone_magnum` | 尖锐蓝白车鼻、红色火焰纹、大型尾翼 | 加速/直线 |
| Hurricane Sonic | `hurricane_sonic` | 低矮红白车体、绿色点缀、三段尾翼 | 转向/稳定 |
| Neo Tridagger ZMC | `neo_tridagger_zmc` | 黑红楔形车体、三叉式前部轮廓 | 均衡/响应 |
| Brocken Gigant | `brocken_gigant` | 黑红厚重车身、前置马达比例、宽车鼻 | 极速/抗碰 |

所有车辆共享统一的车体坐标系、轮胎接口和 LOD 契约，但不通过仅更换颜色伪装成不同车型。

## 3. 游戏状态机

```text
Boot
  -> InputCheck
  -> InputCalibration
  -> CarSelect
  -> CarShowcase
  -> RivalSelect
  -> TrackSelect
  -> GridIntro
  -> Countdown
  -> Racing <-> Pause
  -> Finish
  -> Results
       |-> RetrySameRace -> GridIntro
       |-> BackToGarage -> CarSelect
       `-> Exit
```

任何状态都必须保留明确的返回路径。Joystick2 失联时不沿用最后一次转向；比赛中进入安全减速和暂停状态。Dual Button 是被动 GPIO，不声称可软件检测实体拔线。

## 4. 操作契约

| 输入 | 选择页 | 比赛中 |
| --- | --- | --- |
| Joystick X | 切换候选项 | 连续转向 |
| Joystick Y | 属性/详情滚动（预留） | 首版不映射，避免误操作 |
| Blue 短按 | 确认/添加对手 | 无 |
| Blue 按住 | 无 | Boost |
| Red 短按 | 取消/移除对手 | 刹车 |
| Red 长按 | 返回上一页 | 暂停/继续 |
| Red + Blue 长按 | 退出 App | 退出 App |

玩家车辆默认自动油门，操作集中在走线、刹车和 Boost，保留迷你四驱车持续运转的感觉。

## 5. 核心技术设计

### 5.1 赛道表达

- 单一闭合中心线，以弧长 `s` 表示比赛进度。
- 赛道属性包含中心、切线、法线、宽度、高度、曲率、路面倾斜和段标记。
- 赛车状态以 `(s, lateralOffset, speed, headingOffset)` 为主，不引入通用刚体引擎。
- 立交跨越点在世界坐标中使用不同高度，不将两段路误判为平面交叉。
- 只有左右外护墙，无中间隔板；车辆可在整个赛道宽度内自由抢线。

### 5.2 比赛模型

- 60 Hz 固定步长，每帧最多补算 3～5 步。
- 玩家固定在所有对手之后的发车位。
- 发车性能由可复现随机种子产生，基础偏差限制在 ±3%，不允许不可胜种子。
- 玩家起步前 2～3 秒比对手慢 8～12%，之后恢复车辆正常性能，建立追逐感。
- 撞击护墙导致减速、振动和短暂转向抑制，不立即结束比赛。
- 圈数通过有向跨越终点检测，名次按 `completedLaps + normalizedProgress` 排序。

### 5.3 AI

- 每辆 AI 只保存进度、速度、目标走线、变道冷却和可复现误差状态。
- 根据前方曲率减速，根据前车距离选择左/右超车线。
- 车辆之间只做轻量包络撞击和速度交换，不做完整刚体求解。
- 追赶机制限定在小幅马达效率调整，最后半圈关闭强干预。

### 5.4 渲染

- 复用 Vector Run 的世界到相机变换、透视投影、近裁切、线段预算和 RenderBudgetController。
- 单帧顺序：米白纸面 -> 远山/远景 -> 下层赛道 -> 上层立交 -> 对手 -> 玩家车 -> 速度线 -> HUD。
- 手绘偏移由几何 ID 和固定种子生成，不使用逐帧随机抖动。
- 车库使用 High LOD（80～140 条主线段），比赛使用 Race LOD（35～60 条主线段）。
- 轮胎和导轮通过预定义圆环点旋转，不使用视频帧或序列图。
- 目标 30 FPS，高压场景不低于 20 FPS；模型更新继续保持 60 Hz。
- R0 基线固件仅剩 `0x72e60` 字节（约 459 KiB，9%）App 分区空间；Let's & Go!! 新增代码和静态数据以 180 KiB 为预警线、250 KiB 为硬上限，不编译车辆照片或大位图。

### 5.5 手绘色彩

RGB565 设计令牌的初始语义：

- `paper.base`：中等亮度暖米白，不使用全白。
- `pencil.primary`：低饱和蓝灰主线。
- `pencil.faint`：纸张纹理和远景。
- `course.edge`：略深蓝灰，保证赛道边界读向。
- `car.accent`：各车官方主色，同屏限制高饱和区域面积。
- `state.warn`：撞墙、赛道风险和倒计时短时使用。

考虑 AMOLED 功耗，米白背景与屏幕亮度必须经过真机电流/温升验证；预留低亮纸面配色，但首版不改变手绘视觉方向。

## 6. 代码结构

```text
main/apps/app_lets_and_go_racer/
  app_lets_and_go_racer.{h,cpp}       # Mooncake 生命周期、调度和输入分发
  lets_and_go_config.h                # 统一调参和编译开关
  controller/
    game_flow.{h,cpp}                 # 页面/比赛状态机
    race_controller.{h,cpp}           # 固定步长、圈数、名次、结束判定
  input/
    racer_input.h                     # 游戏输入契约
    racer_input_provider.{h,cpp}       # 适配已有 Joystick2/Dual Button
  model/
    car_catalog.{h,cpp}               # 官方车辆元数据/属性
    car_geometry.{h,cpp}              # 车辆高/低 LOD 线框
    track_types.h
    overpass_track.{h,cpp}            # 闭合立交赛道、标架、取样
    racer_model.{h,cpp}               # 单车速度/转向/碰撞
    rival_ai.{h,cpp}
    race_state.h
  view/
    pencil_palette.h
    pencil_renderer.{h,cpp}            # 纸面、赛道、山地和手绘线条
    car_renderer.{h,cpp}
    garage_renderer.{h,cpp}
    race_renderer.{h,cpp}
    hud_layout.h
    icon_lets_and_go.{h,cpp}
```

模型和控制层不得依赖 HAL/M5GFX，以便主机端编译测试。渲染层不得修改比赛状态。外置控制器驱动不复制 I2C/GPIO 初始化，必须复用现有设备层。

## 7. 分轮实施与提交计划

### R0：基线与计划冻结

- 从远端 Vector Run 最新分支创建功能分支。
- 写入本计划、产品边界、输入契约和硬件延后门禁。
- 验证当前基线的宿主测试和固件编译状态。
- 提交：`docs(lets-go): add complete implementation plan`

### R1：App 骨架与纯状态机

- 新增 App，注册名称、生命周期和临时图标接口。
- 实现无渲染依赖的 `GameFlow`、选择数据和返回语义。
- 覆盖 0/1/3 对手、取消、重赛、回车库和退出测试。
- 提交：`feat(lets-go): add app shell and game flow`

### R2：车辆目录与官方线框几何

- 建立四辆车的官方名称、配色、比例、属性和线段数据。
- 完成 High/Race 两级 LOD，建立线段上限和几何边界测试。
- 完成田宫红蓝双星 Launcher 图标并绑定 `Let's & Go!!`。
- 提交：`feat(lets-go): add official car wireframes and icon`

### R3：车库、车辆选择与对手选择

- 完成圆屏车辆轮播、选中局部放大、属性和确认反馈。
- 完成轮胎/导轮旋转、底盘振动、逐风线和冲出动画。
- 完成剩余车辆的 0～3 对手多选界面。
- 提交：`feat(lets-go): build garage and rival selection`

### R4：立交赛道模型与投影

- 完成单一闭合立交赛道、高度层和赛道取样契约。
- 完成无中隔板左右护墙、远山和简化场景线框。
- 完成上/下层立交绘制顺序、近裁切、视锥裁剪和赛道预览。
- 提交：`feat(lets-go): add overpass track and projection`

### R5：玩家赛车动力学和控制器适配

- 实现自动油门、转向、刹车、Boost、撞墙减速和视觉姿态。
- 将现有 Joystick2/Dual Button 驱动适配为 RacerInput，不把飞行 `pitch` 语义泄漏到赛车模型。
- 补齐失联安全、校准、去抖和动作边沿测试。
- 提交：`feat(lets-go): implement player driving and controls`

### R6：AI、发车与三圈比赛

- 实现 0～3 名 AI、理想走线、超车、曲率减速和轻量车辆碰撞。
- 实现玩家末位发车、可复现起步差异、倒计时和渐进追逐窗口。
- 实现三圈、名次、最佳圈、结束顺序和结算快照。
- 提交：`feat(lets-go): complete deterministic race simulation`

### R7：手绘比赛渲染与 HUD

- 完成米白纸面、静态纸纹、蓝灰双线、车辆官方色和固定偏移手绘效果。
- 完成玩家追尾镜头、对手 LOD、轮胎动画、速度线、撞墙反馈。
- 完成圈数、名次、速度、Boost、小地图和暂停 HUD，保持圆屏安全区。
- 提交：`feat(lets-go): render pencil-style races and HUD`

### R8：全流程集成与结算

- 串联外设检查、车库、对手、赛道、发车、比赛、暂停和结算。
- 支持同配置重赛与返回车库，保存本地最佳圈和最后选车。
- 检查重复打开/关闭、Launcher 返回、任务释放、LVGL/Canvas 切换和看门狗。
- 提交：`feat(lets-go): integrate full race journey`

### R9：性能、音效、振动与故障处理

- 实现车库/比赛独立绘制预算、开销计数和自动细节降级。
- 加入发车、Boost、撞墙、最后一圈和结束音效/振动。
- 验证输入失联、模拟慢帧、赛道边界、圈数跨越和长时间运行。
- 提交：`perf(lets-go): harden rendering and feedback`

### R10：固件门禁与交接

- 运行全量主机端测试、ESP-IDF 清洁编译、固件大小和栈/堆风险检查。
- 生成静态基准画面和可复现比赛摘要，供未来真机对照。
- 写入烧录、操作、调参和硬件验证文档。
- 提交：`docs(lets-go): complete verification handoff`

## 8. 每轮强制自审流程

每一轮编码后必须执行两轮彼此独立的审查，审查和修正完成前不提交。

### Self-review A：正确性与架构

1. 根据本轮验收项逐条对照 diff。
2. 检查状态所有权、生命周期、时间溢出、数组边界、数值稳定和可复现性。
3. 检查 model/controller 是否意外依赖 HAL/M5GFX/LVGL。
4. 检查新功能是否破坏 Vector Run、Launcher 或返回主页约定。
5. 补测试或修正，重跑本轮测试。

### Self-review B：性能、视觉与失败路径

1. 检查每帧动态分配、浮点/除法数量、线段上限、格式化开销和画布提交次数。
2. 检查圆屏安全区、线条层级、官方车型辨识度和手绘偏移稳定性。
3. 检查失联、无对手、最多对手、撞墙、慢帧、重复打开/关闭和中途退出。
4. 检查低细节模式是否仍保留核心游戏信息。
5. 修正后运行本轮测试、既有 Vector Run 回归测试和固件增量编译。

每轮提交前保存一份简短的审查记录到对应阶段文档或提交说明，包含发现、修正和未解决的真机项。

## 9. 自动测试门禁

最低覆盖：

- 状态机转移、取消和重赛。
- 车辆目录唯一性、LOD 数量上限和几何有效性。
- 闭合赛道连续性、标架正交性、立交高度分离和边界距离。
- 赛车转向、刹车、Boost、撞墙和失联安全。
- 0/1/3 AI、超车、排名、三圈结束和可复现随机。
- HUD 安全区和线段预算。
- 所有现有 `vector_canyon_*_test.cpp` 与 Launcher 外设导航测试继续通过。
- `idf.py build` 通过，分区容量有可接受余量。

## 10. 暂时无法在本机完成的真机门禁

以下项目需在手表和外设回到手边后执行，不用桌面模拟结果代替：

1. Joystick2 实际中点、极性、死区、滤波延迟和 I2C 长时间稳定性。
2. Dual Button GPIO3/4 实际按键极性、去抖、组合长按和返回 Launcher。
3. 466×466 AMOLED 上的车辆辨识度、线宽、米白背景亮度、拖影和圆屏裁切。
4. 比赛最坏画面、车库 High LOD 和立交交叉时的真机 FPS/渲染耗时/栈水位。
5. 米白全屏的 AMOLED 功耗、温升和电池影响。
6. 音效音量、振动强度和连续游戏体感。

代码阶段会为上述各项预留日志、编译开关和集中调参，使真机验证不需要重构。

## 11. 完成定义

在无真机阶段，代码完成需同时满足：

- R0～R10 所有阶段都有独立提交。
- 所有新增主机端测试与现有回归测试通过。
- ESP-IDF 清洁编译通过，无 Let's & Go!! 新增编译警告；基线已有警告单独记录，不归因于本 App。
- App 可从 Launcher 打开并安全关闭，不泄漏任务、输入设备或画布状态。
- 固定种子比赛在桌面测试中产生稳定的圈数、排名和渲染摘要。
- 真机不可验证项全部写入交接清单，不将其标记为已通过。
