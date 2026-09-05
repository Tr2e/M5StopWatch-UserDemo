# Vector Canyon Fighter：技术设计

> 阶段：G2（技术设计）
>
> 依赖：[项目设计与执行计划](Vector-Canyon-Fighter-项目设计与执行计划.md)、[UI / UE 设计](Vector-Canyon-Fighter-UI-UE设计.md)、[外设接线设计](科技游戏外设接线设计.md)
>
> 本文冻结 v0.1 的模块边界、数据流、输入适配、地形表达、性能预算和验证方法。实现不得绕过这些边界。

## 1. 实现原则

1. **先可玩闭环，后细节。** 静态场景、推进、输入、风险、外设依次增加。
2. **模型与硬件解耦。** 飞行和地形逻辑不读取 GPIO/I2C/IMU，也不操作 LVGL。
3. **确定性优先。** 相同地形种子、相同输入序列和相同步长必须重现同一场景。
4. **几何优先。** 使用透视投影线段构造峡谷，不引入纹理、实时光照或通用 3D 场景图。
5. **真机预算优先。** 所有上限以 StopWatch 上的最坏峡谷状态验证，而非桌面预览。

## 2. 模块与数据流

```text
IMU ─────────── AxisSource ───┐
Joystick2 ───── AxisSource ───┤
                              ├─ InputProvider ─ FlightInput ─┐
表身按钮 ──── ActionSource ───┤                                │
Dual Button ─ ActionSource ───┘                                ▼
                 UiStateMachine ← FlightModel ← FlightController
                                      │
                                      ▼
                               TerrainStream
                                      │
                                      ▼
                                  SceneState
                                      │
                                      ▼
                                   Renderer
                                      │
                                      ▼
                            M5Canvas / StopWatch Display
```

建议目录结构：

```text
main/apps/app_vector_canyon_fighter/
  app_vector_canyon_fighter.{h,cpp}
  input/{flight_input,input_provider,imu_button_input,joystick_dual_input}.{h,cpp}
  model/{flight_model,terrain_stream,scene_state,collision_model}.{h,cpp}
  view/{renderer,hud_renderer,ship_renderer,ui_state_machine}.{h,cpp}
  test/{replay_script,scene_fixture}.{h,cpp}
```

### 2.1 模块职责

| 模块 | 允许做什么 | 禁止做什么 |
| --- | --- | --- |
| `InputProvider` | 采样硬件、校准、去抖、失联检测，输出规范化输入 | 改变飞行速度、绘制 UI |
| `FlightController` | 处理暂停、推进沿事件、输入失效和固定步长调度 | 读取具体外设 |
| `FlightModel` | 积分速度/姿态/高度/横移，产生可复现飞行状态 | 调用 HAL、随机生成地形 |
| `TerrainStream` | 用种子生成并循环复用峡谷切片 | 操作屏幕、读取输入 |
| `CollisionModel` | 根据飞行状态和可见/前方地形计算余量和撞击 | 直接决定 UI 排版 |
| `UiStateMachine` | 管理标题、校准、飞行、暂停、风险、重试和失联提示 | 执行 I2C 或几何投影 |
| `Renderer` | 将只读 `SceneState` 绘制到画布 | 变更模型、阻塞等待输入 |

## 3. 核心数据契约

### 3.1 输入

```cpp
struct FlightActions {
    uint16_t pressed;  // 每个物理边沿只在一个 sample 中出现
    uint16_t held;     // 可跨固定模拟步重复使用的持续状态
};

struct FlightInput {
    float steer;       // [-1, +1]，左负右正
    float pitch;       // [-1, +1]，俯冲负、爬升正
    float throttle;    // [0, 1]
    FlightActions actions;
    bool valid;        // 轴输入健康且完成必要校准
    uint32_t sequence;
};
```

接口：

```cpp
class InputProvider {
public:
    virtual ~InputProvider() = default;
    virtual void open() = 0;
    virtual FlightInput sample(uint32_t nowMs) = 0;
    virtual InputStatus status(uint32_t nowMs) const = 0;
    virtual void requestCalibration(uint32_t nowMs) = 0;
    virtual void close() = 0;
};
```

轴来源与动作来源独立报告：`FlightAxisSource` 区分 `IMU / Joystick2`，`FlightActionSource` 区分 `BodyButtons / DualButton`。因此允许 Joystick2 + Dual Button、Joystick2 + 表身按键、IMU + Dual Button 和完整 IMU 降级等组合，不把两个电气设备错误地压成一个在线状态。

`InputStatus` 统一报告来源、连接、`Disconnected / Calibrating / Ready / Degraded / Fault`、校准能力、校准进度、最后有效样本时间和连续错误数。无需校准的按钮设备不实现伪校准；外设异常返回 `valid=false` 和中性轴值，不能沿用最后一次极端姿态。

一次性动作包括 `Pause / Reset / ToggleImmersive / Recalibrate / ThrottleUp / ThrottleDown`；持续动作目前包含 `Boost`。应用在固定模拟步循环外只消费一次 `pressed`，FlightModel 只读取规范化连续轴和 `held`，避免慢帧把一次按键重复执行。K1 长按由当前 Provider 映射为上下文动作：正常飞行切换沉浸模式，撞击后请求重置；应用不再直接读取 HAL 游戏按键。Renderer 只读 `aircraftVisible` 和 `InputStatus`：`aircraftVisible=true` 使用 world-up 稳定追尾相机并绘制战机层；`false` 使用完整 pitch、roll、turnYaw 的机体随动相机并隐藏战机、尾焰、投影与频闪。峡谷、共形 HUD、碰撞和飞行模拟在两种模式下持续运行。

### 3.2 飞行状态

```cpp
struct FlightState {
    float forwardDistance;
    float lateralOffset;
    float altitude;
    float speed;
    float roll;
    float pitch;
    float turnYaw;        // 相对航道的瞬时视觉偏航，不是累计航向
    float boostAmount;
    bool paused;
    bool collided;
};
```

模型将 `steer/pitch` 转换为平滑的目标横向速度、垂直速度，以及临界阻尼的视觉滚转、俯仰和局部转向偏航；`throttle` 决定目标巡航速度，持续 `Boost` 状态控制推进建立与衰减。暂停、重置、沉浸和重新校准等边沿由应用控制层处理。渲染只读取状态，不反向修正物理。

### 3.3 场景快照

`SceneState` 是每帧唯一的渲染输入，包含：飞行状态、相机/消失点、固定数量的地形切片、碰撞余量、UI 状态和性能等级。它不得携带 HAL 指针、动态分配的临时集合或可写硬件句柄。

## 4. 固定步长与输入采样

- 模型步长：`1/60 s`；每帧最多补算 3 步，超过的时间丢弃并记录诊断，避免掉帧后“穿墙”。
- 渲染目标：`30 FPS`（约 33 ms）；允许在静止/暂停时降低刷新率。
- 输入采样：IMU 50–100 Hz；Joystick2 50 Hz；Dual Button 以 10–20 ms 去抖或事件采样。
- 输入滤波：IMU 和摇杆各自在 Provider/AxisSource 内应用适合自身量纲的校准、死区和曲线，最终只输出相同的 `[-1,+1]` 意图；不得把 IMU 的角度或重力阈值写入 FlightModel。
- 输入动作：每个硬件边沿只允许在一个 `sample()` 快照中出现；App 在固定步循环前消费，`held` 状态才可跨多个模拟步重复使用。
- 所有时间使用单调毫秒时钟；不得用阻塞 `delay()` 等待外设或动画。

初始可调参数集中在一个配置结构中：最大速度、巡航加速度、推进时长、转向响应、滚转上限、俯仰上限、IMU 死区、摇杆死区、碰撞安全余量和地形难度。业务代码中不散落魔法数。

## 5. 地形流与透视投影

### 5.1 切片模型

地形采用沿前进轴 `z` 排列的固定数量横截面。每个截面至少含：中心线、可飞行半宽、地面高度、左右墙高度、左右墙轮廓偏移和局部风险标记。

- 固定保留 18 个深度切片；前方切片离开近裁切面后复用到远端。
- 使用固定种子 PRNG 结合段编号生成，段编号而非帧号决定地形，因此可重放。
- 邻近切片用受限斜率插值，防止无法预判的直角墙或不可飞越坡面。
- 近裁切面后方的线段不绘制；横截面跨越可见圆时裁切，而不是让文字或 HUD 参与裁切。

### 5.2 投影

使用简化透视：以战机追尾镜头为参考，先应用飞行横移/高度/有限滚转和俯仰，再将世界点投影至屏幕。消失点默认靠近 `(233, 142)`，由转向和俯仰有限偏移。

```text
screenX = centerX + focalLength * worldX / worldZ
screenY = horizonY + focalLength * worldY / worldZ
```

`worldZ <= nearPlane` 的点直接丢弃；跨近裁切面的线段先裁切再投影。G2 不引入全矩阵引擎：运算限定为预计算三角函数、简单二维旋转和透视除法。

### 5.3 碰撞模型

碰撞使用与渲染相同的地形切片，不使用屏幕像素碰撞。

- 横向：飞行器半宽与截面可飞行半宽比较。
- 垂直：飞行器安全包络与地面、墙体/门洞高度比较。
- 前方预警：沿未来固定距离采样余量，提前触发 `TERRAIN`，而不是仅在撞击帧提示。
- 撞击发生后冻结前进、关闭推进并交给 `UiStateMachine` 显示重试状态。

## 6. 渲染与资源预算

Renderer 使用一个复用的 `M5Canvas` 或等价离屏画布完成单帧绘制，再一次性提交到显示层；不得把完整场景拆成大量随帧创建/销毁的 LVGL 控件。

| 项目 | High | Medium | Low |
| --- | ---: | ---: | ---: |
| 地形切片 | 18 | 12 | 8 |
| 每帧线段上限 | 580 | 360 | 220 |
| 战机线段 | ≤ 32 | ≤ 24 | ≤ 16 |
| HUD 文本项 | ≤ 8 | ≤ 6 | ≤ 5 |
| 可选发光/航迹 | 有限 | 仅推进 | 无 |
| 每帧动态堆分配 | 0 | 0 | 0 |

绘制顺序固定：背景 → L0 远景 → L1/L2 地形 → 战机暗面/轮廓 → 推进与风险 → HUD。高亮颜色在同一像素区域最多叠加一种可选特效。

首个可玩版本先实现 Low 预算，再逐步打开 Medium/High；性能下降时按 High→Medium→Low 降级，禁止自动删掉战机、航道、风险或输入状态。

## 7. 输入提供者实现策略

### 7.1 IMU + 表身按键

复用 `GetHAL().updateImuData()` 和已有 K1/K2 按键状态。启动稳定窗口同时校准重力姿态零点与 X/Y 陀螺零偏；运行时以重力角维持长期基准、以陀螺仪提供即时响应，并在加速度模长显著偏离 1 g 时降低重力修正权重。设备特有的角度、死区、曲线、异常帧和陈旧样本处理全部收敛在 Provider 内，外部仍只接收规范化 `steer/pitch`。当前映射为 K1/K2 短按降低/提高巡航速度、K2 按住推进、正常飞行中 K1 长按切换战机视觉层、碰撞后 K1 长按重新校准，并保留 K1+K2 长按返回 Launcher 的现有系统约定。

### 7.2 Joystick2 + Dual Button

- Joystick2：在 PORT.A 的 GPIO10（SDA）/GPIO11（SCL）建立**独立于内部 GPIO47/48 总线**的 I2C 主机，默认 100 kHz，探测地址 `0x63`。
- Joystick2 按官方协议先读 `0xFE` 固件版本确认身份，再以 50 Hz 从 `0x50` 读取小端序有符号 X/Y 偏移；本地静止校准、死区、曲线和低通滤波均留在 AxisSource 内。
- Dual Button：GPIO3/4 配置为输入，按下低电平；使用软件去抖并以边沿事件输出。
- Dual Button 是无身份寄存器、无心跳的被动 GPIO 单元，物理拔线与“未按下”无法由软件区分；其 `connected` 只表示 GPIO 路径已成功配置。实体连接必须依赖通电前导通检查及人工按键自检，不得宣称自动热插拔检测。
- I2C 总线只使用本工程既有的 ESP-IDF/I2C 组件风格；不得同一端口混用 legacy 与 driver-ng，避免驱动冲突。
- Joystick2 作为 `AxisSource`、Dual Button 作为 `ActionSource` 独立报告连接与错误；任一失联不伪装成整套控制器同时掉线。
- 两者分别实现 `FlightAxisProvider` 与 `FlightActionProvider`，再由 `CompositeInputProvider` 合成为既有 `FlightInput`；按键单侧失联进入可玩的 `Degraded`，摇杆失联则输出安全中性轴。
- 任何连续读取超时、地址消失或信号异常都更新 `InputStatus`，不阻塞主循环；未来 Router 按固定优先级组合外设或安全回退到 IMU/表身按键。

外设实现安排在 G4；默认编译开关 `VECTOR_CANYON_USE_EXTERNAL_INPUT=0` 继续使用 `ImuButtonInputProvider`。仅在接线与电压门禁全部通过后才可置为 `1`，无需修改 App、FlightModel 或 Renderer。

## 8. 测试、重放与观测

### 8.1 可重放测试

`ReplayScript` 记录种子、固定步长和 `FlightInput` 序列。它支持：

- 中性巡航 60 秒。
- 左右连续转向、爬升/俯冲和推进组合。
- 边界贴墙、预警触发和撞击。
- 输入失联后恢复中性状态。

每个脚本产出固定时间点的场景摘要、线段计数、帧时间、碰撞余量与截图，作为视觉回归依据。

### 8.2 真机指标

| 指标 | 验收方式 | 初始门槛 |
| --- | --- | --- |
| 帧时间 | 最复杂峡谷段的 95 分位 | ≤ 33 ms（Low） |
| 帧时间尖峰 | 最复杂段最大值 | ≤ 66 ms，且无持续尖峰 |
| 内存 | 进入/退出 10 次后的可用内存 | 无持续下降 |
| 输入延迟 | 物理动作至可见姿态变化 | 目标 ≤ 80 ms |
| I2C 失联 | 拔除/重连测试 | 不崩溃，安全中性 |
| 视觉回归 | 固定种子截图 | 与 G1 构图和语义一致 |

## 9. G2 审查与校准

| 检查项 | 结论 | 校准结果 |
| --- | --- | --- |
| 是否把硬件逻辑混入游戏模型 | 通过 | `InputProvider` 是唯一硬件边界 |
| 是否允许渲染器主导物理 | 通过 | Renderer 只读 `SceneState` |
| 是否可在外设到货前推进 | 通过 | IMU/按键提供同一 `FlightInput` |
| 是否有确定性回归手段 | 通过 | 固定种子 + 重放输入 + 场景摘要 |
| 是否有明确性能降级路径 | 通过 | 三档切片/线段预算与固定降级顺序 |
| 是否覆盖 I2C/按键失联 | 通过 | 输入健康状态和安全中性行为已定义 |

### G2 退出检查表

- [x] 模块职责、数据流和禁止跨层访问的规则已定义。
- [x] 临时与最终控制器使用统一 `FlightInput` 契约。
- [x] 切片地形、透视裁切、碰撞和固定种子重放已定义。
- [x] 帧率、线段、内存分配和低细节预算已定义。
- [x] IMU、I2C、GPIO 和失联处理与现有 StopWatch HAL 边界兼容。
- [x] 实现顺序限定为 G3 的低细节可玩垂直切片。

**结论：** G2 通过。G3 只能从“App 外壳 + Low 预算的静态场景”开始，不能直接实现特效、复杂地形或外设驱动。
