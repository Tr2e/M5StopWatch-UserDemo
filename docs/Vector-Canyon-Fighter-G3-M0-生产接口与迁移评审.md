# Vector Canyon Fighter G3：M0 生产接口与迁移评审

> 状态：等待用户确认
> 日期：2026-09-02
> 算法基线：A0–A4 已确认并提交
> 本节点只做迁移设计，不修改生产代码、不构建新固件、不烧录

## 1. 迁移目标

把 A1–A4 已验证的显式峡谷算法安全迁入 StopWatch 游戏，同时满足：

- 平谷底、平顶台地和显式陡峭侧壁保持不变；
- 中心线局部标架决定横纵网格方向；
- 左右边界事件同时服务渲染和碰撞；
- 固定种子流式世界可重复；
- 未来 Joystick2 / Dual Button 只替换 InputProvider；
- 每个实现阶段可独立 Review、commit 和回退；
- 静态地形通过真机 Review 前，不恢复完整 HUD、战机和游戏功能。

算法证据节点：

| 节点 | 内容 | Commit |
| --- | --- | --- |
| A0 | 显式悬崖峡谷算法研究 | `e8d5569` |
| A1 | 直线固定横截面 | `4af6b43` |
| A2 | 曲线中心线、弧长采样和局部标架 | `8e645ca` |
| A3 | 左右独立单侧突出 | `19fe3ef` |
| A4 | 固定种子 S-gate、流式连续性和 LOD | `8600bf6` |

## 2. 当前生产耦合

### 2.1 TerrainStream

当前 `TerrainSlice` 同时承担：

- 相机相对深度 `z`；
- 世界纵深 `worldZ`；
- 全局 X 方向中心 `center`；
- 对称通道半宽 `halfWidth`；
- 谷底高度、倾斜和拱度；
- 左右最高山体高度；
- 37 列高度场 `surfaceHeights`。

结构性限制：

- 横截面只能沿全局 X 轴；
- 左右边界不能独立；
- 渲染和碰撞依赖不同的派生字段；
- 高度场存储了目标图不需要的大量表面信息。

### 2.2 Renderer

当前地形渲染器：

- 使用 `center + columnX` 得到世界 X；
- 假设世界 Z 永远等于相机前方；
- 直接读取 `surfaceHeights`；
- 在栈上分配约 4 KB 的投影数组；
- 通过列 stride 离散删除远景纵线；
- 同时包含校准页、地形、战机和 HUD，修改地形容易影响整帧。

### 2.3 CollisionModel

当前横向净空：

```text
halfWidth - abs(flight.lateralOffset - slice.center) - shipHalfWidth
```

它隐含：

- 左右宽度相同；
- `flight.lateralOffset` 与 `slice.center` 属于同一个全局 X 坐标；
- 碰撞可以从渲染 slice 的近景集合推断。

显式弯曲峡谷中这三个假设都不再成立。

### 2.4 FlightModel 与 InputProvider

以下接口可以保留：

```text
InputProvider -> FlightInput -> FlightModel
```

- `steer`、`pitch`、`throttle`、`boost` 语义不变；
- `FlightState::lateralOffset` 重新明确为峡谷局部坐标 `u`；
- `forwardDistance` 继续由 FlightModel 累计；
- IMU/按钮与未来 Grove 外设不感知地形类型。

## 3. 生产坐标契约

所有模块必须使用同一套定义：

```text
s  = 沿峡谷中心线的累计世界弧长
C  = 中心线世界位置 (x,z)
T  = 中心线单位切线
N  = 水平面单位法线 (Tz,-Tx)
u  = 相对中心线的横向位置；正值为世界/屏幕右侧
h  = 相对平坦谷底的高度
P  = C + N*u + Up*h
```

硬性规则：

- Renderer 不得把 `u` 当成全局 X；
- FlightModel 不保存世界 X，只保存局部 `u`；
- CollisionModel 不读取屏幕坐标；
- 相机右向量必须使用 `cross(worldUp, forward)`；
- 相机、战机和地形不得重复应用 lateralOffset；
- 任何模块不得单独维护另一份峡谷宽度。

## 4. 建议的生产数据结构

### 4.1 固定剖面

```cpp
enum class CanyonProfilePoint : uint8_t {
    LeftPlateauOuter,
    LeftPlateauMid,
    LeftPlateauInner,
    LeftCap,
    LeftFaceHigh,
    LeftFaceLow,
    LeftToe,
    LeftFloorEdge,
    FloorLeft4,
    FloorLeft3,
    FloorLeft2,
    FloorLeft1,
    FloorCenter,
    FloorRight1,
    FloorRight2,
    FloorRight3,
    FloorRight4,
    RightFloorEdge,
    RightToe,
    RightFaceLow,
    RightFaceHigh,
    RightCap,
    RightPlateauInner,
    RightPlateauMid,
    RightPlateauOuter,
};

struct CanyonProfileSample {
    float lateral;
    float height;
};

inline constexpr std::array<CanyonProfileSample, 25> kCanyonProfile = { /* A1 */ };
```

剖面是 `constexpr` 只读表，应进入 flash，不在每个 slice 中重复保存。

### 4.2 Slice 只保存生成状态

```cpp
struct ExplicitCanyonSlice {
    uint32_t segmentId;
    float worldS;
    float centerX;
    float centerZ;
    float tangentX;
    float tangentZ;
    float leftWidth;
    float rightWidth;
};

struct CanyonBoundary {
    float leftWidth;
    float rightWidth;
};

struct CanyonRouteFrame {
    float worldS;
    float centerX;
    float centerZ;
    float tangentX;
    float tangentZ;
};
```

不保存：

- 25 个世界顶点；
- surface height 数组；
- 法线——由切线即时得到；
- floor tilt / crown；
- leftWall / rightWall 峰高；
- 相机相对 z——由 Renderer 统一变换。

目标：`sizeof(ExplicitCanyonSlice) == 32`，必须使用 `static_assert` 固化。

### 4.3 世界点查询

```cpp
struct CanyonWorldPoint {
    float x;
    float y;
    float z;
};

class ExplicitCanyonStream {
public:
    static constexpr size_t kSliceCount = 34;
    static constexpr size_t kProfileCount = 25;

    void reset(uint32_t seed);
    void update(float flightForwardDistance);

    const std::array<ExplicitCanyonSlice, kSliceCount>& slices() const;
    CanyonWorldPoint worldPoint(size_t slice, size_t profilePoint) const;
    CanyonBoundary boundaryAt(float worldS) const;
    CanyonRouteFrame routeFrameAt(float worldS) const;
    float playerWorldS() const;
};
```

`worldPoint()` 属于模型层。它只把缓存 slice、固定剖面和左右宽度组合成世界点；Renderer 只能消费结果，不能自己决定坡角、突出量或谷底宽度。

## 5. 内存方案

### 5.1 当前估算

根据现有字段布局：

| 项目 | 约占用 |
| --- | ---: |
| `TerrainSlice` | 192 B |
| 26 个 TerrainSlice | 4992 B |
| Renderer 栈上投影数组 | 约 4104 B |
| 地形模型 + 投影工作集 | 约 9.1 KB |

当前主任务栈配置只有 `8192 B`。虽然 TerrainStream 是 App 成员，但 Renderer 的约 4 KB 数组位于调用栈，再叠加函数局部变量和显示调用，必须避免继续扩大栈帧。

### 5.2 新方案目标

| 项目 | 目标占用 |
| --- | ---: |
| 34 × 32 B ExplicitCanyonSlice | 1088 B |
| 48 点弧长查找缓存 | 约 576 B |
| 8–10 个路线控制点 | 64–120 B |
| 6 个事件固定窗口 | 约 96 B |
| 其他 stream 状态 | `<128 B` |
| 模型总计 | 约 1.9 KB |
| 850 × 4 B 投影点 | 3400 B |

投影缓存：

```cpp
struct ProjectedCanyonPoint {
    int16_t x;
    int16_t y;
};

std::array<ProjectedCanyonPoint, 34 * 25> _projectedTerrain;
```

必须成为 Renderer 成员，不在 `render()` 栈上分配。近裁剪状态使用坐标 sentinel 或逐线段裁剪，不再增加 850 B 的 bool 数组。

预期结果：模型与投影工作集约 `5.3 KB`，比当前约 `9.1 KB` 更低，同时 Renderer 地形路径栈增量目标 `<1 KB`。

实现阶段必须用真机编译结果、`sizeof`/`static_assert` 和栈水位日志重新核实，不能把本表当作最终实测值。

## 6. 路线流迁移

### 6.1 固定容量 RouteStream

建议内部保留：

- 8–10 个中心线控制点环形缓冲；
- 最多 48 个稠密 Catmull–Rom / 累计弧长查找点；
- 34 个最终等弧长 slice；
- 控制点 index、segment ID 和固定 seed。

控制点生成：

```text
headingDelta = hash(seed, controlIndex) * boundedTurn
nextHeading  = previousHeading + headingDelta
nextPoint    = previousPoint + direction(nextHeading) * controlSpacing
```

生成后必须检查：

- Z/前进方向不回头；
- 相邻控制点间距有效；
- `curvature * outerOffset < 0.95`；
- 若失败，按确定性规则缩小 headingDelta，不重新抽随机数；
- 相同 seed/controlIndex 的结果可复现。

### 6.2 弧长重采样

- 稠密曲线查找表是成员缓存，不放栈；
- slice 使用固定目标弧长间距；
- 新 slice 只在远端生成一次；
- 已进入窗口的 segment ID 和几何不可改变；
- 相机移动只改变相对坐标，不修改世界数据。

### 6.3 ForwardDistance 映射

当前视觉使用约：

```text
terrainWorldS = flight.forwardDistance * 0.018
```

第一轮迁移保留该比例，避免同时改变速度手感。比例只能定义在 ExplicitCanyonStream 内部一处，并通过 `playerWorldS()` 暴露结果。

## 7. 事件流迁移

直接移植 A4 已验证规则：

- `streamEvent(seed, eventIndex)` 纯函数；
- 44 世界单位一个 grammar cycle；
- 左 shoulder -> recovery -> 右 shoulder -> recovery；
- `std::array<ShoulderEvent, 6>` 当前事件窗口；
- 边界使用有限支撑五次平滑函数；
- 每次生成后检查最小宽度、边界步长和可达性；
- 视觉与碰撞调用同一个 `boundaryAt(worldS)`。

第一轮生产参数必须与 A4 一致，不在移植同时“顺便调得更刺激”。

## 8. Renderer 迁移

### 8.1 先拆分旧地形路径

当前 `Renderer::render()` 同时处理校准、地形、战机和 HUD。第一步只重构为：

```text
render()
  -> calibration early return
  -> clear
  -> drawLegacyTerrain() 或 drawExplicitTerrain()
  -> drawShipAndHud()
  -> endWrite
```

这一步使用旧 TerrainStream，画面必须逐像素/实机一致。未通过回归前不得加入显式地形。

### 8.2 世界到相机变换

```text
cameraPosition = C(playerS) + Up * (flight.altitude + cameraLift)
cameraForward  = normalize(T(playerS) + pitchComponent)
cameraRight    = normalize(cross(worldUp, cameraForward))
cameraUp       = normalize(cross(cameraForward, cameraRight))

relative = worldPoint - cameraPosition
cameraX = dot(relative, cameraRight)
cameraY = dot(relative, cameraUp)
cameraZ = dot(relative, cameraForward)
```

只有 `cameraZ > nearPlane` 的线段进入投影。跨越近裁剪面的线段先在相机空间裁剪，再投影。

### 8.3 构图基线

离线 450×450 参数换算为屏幕归一化值：

- principal X：`0.5 × width`；
- principal Y：约 `0.347 × height`；
- focal：约 `0.827 × width`；
- cameraLift：先使用 A1–A4 等价值；
- 不直接沿用当前 `112` 的地形焦距。

真机 A1 静态 Review 后才能微调 focal、principal Y 和 cameraLift；坡角、剖面和宽度不属于相机调参范围。

### 8.4 LOD 与线段预算

| 区域 | 规则 |
| --- | --- |
| structural 7 rails | 始终绘制 |
| mid 4 rails | 深度 20–25 平滑衰减 |
| fine rails | 深度 9–14 平滑衰减 |
| 横截面 ribs | 保留全部，颜色按深度衰减 |
| cliff diagonals | 只在近景绘制 |

预计总线段仍在当前约千级预算内。若帧率不足，严格按 A4 LOD 顺序减负，不删除墙脚、崖顶或全部侧壁纵线。

### 8.5 相机与战机横移策略

第一版：

- 相机横向锚定路线中心 `u=0`；
- 战机屏幕横移来自 `FlightState::lateralOffset`；
- 地形不再同时减去 lateralOffset；
- 避免当前“移动地形 + 移动战机”可能造成的双重横移；
- 真机确认玩法读数后再讨论相机部分跟随，不在首轮加入。

## 9. CollisionModel 迁移

### 9.1 非对称横向净空

对局部横向位置 `u`：

```text
leftClearance  = u + leftWidth  - shipHalfWidth
rightClearance = rightWidth - u - shipHalfWidth
lateralClearance = min(leftClearance, rightClearance)
```

禁止再使用对称 `halfWidth - abs(...)`。

### 9.2 纵向探针

碰撞不再遍历“屏幕上 z < 4.8 的所有 slice”。建议对战机长度使用 3 个世界弧长探针：

```text
tail / center / nose
```

每个探针调用同一个：

```text
boundaryAt(playerWorldS + probeOffset)
```

首轮可以让三个探针共享 flight 的局部 `u`；若后续战机航向与中心线差异明显，再将战机世界位置投影到各探针法线，不能提前增加复杂度。

### 9.3 地面

显式基线谷底高度固定为 0：

```text
floorClearance = flight.altitude - shipFloorClearance
```

删除 floorTilt、floorCrown 和 highestFloor 补偿。台地高度不参与通道内地面碰撞。

## 10. 兼容与回退开关

新增配置头：

```cpp
#ifndef VECTOR_CANYON_EXPLICIT_TERRAIN
#define VECTOR_CANYON_EXPLICIT_TERRAIN 0
#endif
```

App 只切换 terrain 类型：

```cpp
#if VECTOR_CANYON_EXPLICIT_TERRAIN
using ActiveTerrainStream = ExplicitCanyonStream;
#else
using ActiveTerrainStream = TerrainStream;
#endif
```

Renderer 和 CollisionModel 提供 legacy / explicit 重载，App 的调用流程不变：

```text
reset -> update -> evaluate -> render
```

要求：

- 新文件始终参与编译，避免开关打开时才暴露编译错误；
- 默认开关为 `0`，直到显式路径离线测试通过；
- 每次真机烧录前记录开关、commit 和固件哈希；
- 回退只需关闭开关并重新构建，不删除新代码、不覆盖旧实现。

## 11. 文件级迁移清单

### 新增

```text
main/apps/app_vector_canyon_fighter/model/explicit_canyon_types.h
main/apps/app_vector_canyon_fighter/model/explicit_canyon_stream.h
main/apps/app_vector_canyon_fighter/model/explicit_canyon_stream.cpp
main/apps/app_vector_canyon_fighter/vector_canyon_config.h
tools/vector_canyon_explicit_production_test.cpp
```

### 修改

| 文件 | 修改范围 |
| --- | --- |
| `vector_canyon_renderer.h/.cpp` | 拆分旧地形绘制；新增显式重载和成员投影缓存 |
| `collision_model.h/.cpp` | 新增 ExplicitCanyonStream 重载和非对称公式 |
| `app_vector_canyon_fighter.h` | ActiveTerrainStream 类型别名 |
| `app_vector_canyon_fighter.cpp` | 只增加模式/性能日志，不改变 update 顺序 |

### 首轮禁止修改

```text
input/input_provider.h
input/imu_button_input_provider.*
model/flight_model.*
战机顶点/边表
HUD 布局与文字
全局配色
```

`main/CMakeLists.txt` 已使用 `GLOB_RECURSE apps/*.cpp`，新增模型源文件会自动进入 component；首轮不需要手动维护源文件列表。

## 12. 分阶段实施、Review 与 Commit

### M1：Renderer 旧路径解耦

- [ ] 把现有地形绘制抽为 `drawLegacyTerrain()`；
- [ ] 将投影数组移为 Renderer 成员；
- [ ] 默认开关保持 `0`；
- [ ] 构建成功；
- [ ] 真机旧地图、战机、HUD 与当前版本一致；
- [ ] 性能日志不得退化超过 Review 容差；
- [ ] Review 通过后单独 commit。

失败即回退：M1 不得包含任何新峡谷几何。

### M2：显式模型移植，仍不接 Renderer

- [ ] 新建 25 点 constexpr profile；
- [ ] 新建 34 slice 固定缓存；
- [ ] 移植 RouteStream、弧长采样和局部标架；
- [ ] 移植左右事件流和 6 事件窗口；
- [ ] host test 复现 A1–A4 指标；
- [ ] `static_assert` 内存目标；
- [ ] 开关关闭时固件构建成功；
- [ ] Review 通过后单独 commit。

### M3：显式投影路径，开关仍关闭

- [ ] 新增 `drawExplicitTerrain()`；
- [ ] 世界点统一经过相机矩阵；
- [ ] 近裁剪无长线；
- [ ] 结构/mid/fine LOD 索引与 A4 一致；
- [ ] legacy 回归构建、运行正常；
- [ ] Review 通过后单独 commit。

### M4：真机 A1 静态地形

- [ ] 开关打开；
- [ ] 固定直线中心线；
- [ ] 关闭 HUD、战机、输入和运动；
- [ ] 真机显示平谷底、陡侧壁和平顶台地；
- [ ] 侧壁纵线清晰；
- [ ] 圆屏构图与 A1 对照；
- [ ] 用户实机 Review 通过后单独 commit。

### M5：真机 A2 曲线标架

- [ ] 启用曲线中心线和弧长 slice；
- [ ] 横排随路线旋转；
- [ ] 无翻面、交叉或左右镜像；
- [ ] 真机俯视调试模式可临时打开；
- [ ] 用户实机 Review 通过后单独 commit。

### M6：真机 A3/A4 事件流与性能

- [ ] 启用固定种子 S-gate；
- [ ] 连续运行 60 秒无接缝和跳排；
- [ ] FPS `>=24`，目标 `30`；
- [ ] render max、simulation clamps、栈水位无异常；
- [ ] boost 下 LOD 无明显突跳；
- [ ] 用户录像 Review 通过后单独 commit。

### M7：碰撞、战机、HUD 与输入恢复

- [ ] 非对称碰撞使用同一左右边界；
- [ ] 左右 warning/collision 调试值可记录；
- [ ] 战机横移不与地形产生双倍位移；
- [ ] IMU/按钮恢复；
- [ ] HUD 和战机最后恢复；
- [ ] Joystick2/Dual Button provider 无需修改地形接口；
- [ ] 完整游戏 Review 通过后单独 commit。

## 13. 每次烧录前 Checklist

- [ ] 当前阶段的离线/构建测试已通过；
- [ ] `git status` 只包含本阶段文件；
- [ ] feature flag 值已记录；
- [ ] 固件 commit 与 bin 哈希已记录；
- [ ] 设备电量/USB 连接正常；
- [ ] 不同时改地形、HUD、输入和配色；
- [ ] 烧录后先观察静态 10 秒，再运行动态；
- [ ] 记录 FPS、render avg/max、clamp 和栈水位；
- [ ] 拍照或录像作为 Review 证据；
- [ ] Review 未通过不得 commit 下一阶段。

## 14. 硬性否决条件

出现任一项立即停止当前阶段：

- 删除 legacy 路径后才验证新路径；
- 在 M1 解耦时顺便修改画面；
- 在 M2 模型阶段提前接真机 Renderer；
- Renderer 重新实现坡角、宽度事件或碰撞边界；
- 使用全局 X 生成横截面；
- 将 850 点投影缓存放在 8 KB 主任务栈；
- 用动态 vector 保存 slice、profile 或事件；
- 屏幕空间修正网格角度；
- 继续保留对称 halfWidth 碰撞公式；
- 相机和战机重复应用 lateralOffset；
- 通过关闭侧壁纵线解决性能问题；
- 静态峡谷未通过就恢复 HUD、战机和特效；
- 一次 commit 横跨两个迁移阶段。

## 15. M0 Review 判断

生产迁移具备可行性，且不需要扩大现有主要工作集。显式模型预计比高度场模型更省 RAM，真正需要控制的风险是：

1. Renderer 大函数的安全拆分；
2. 世界到相机坐标的统一；
3. FlightState 局部横向坐标与非对称碰撞的契约；
4. 8 KB 主任务栈内禁止大型局部数组；
5. 真机调试时严格保持一次只改变一个变量。

建议按 M1–M7 顺序执行。M0 用户确认前，不提交本设计，也不修改生产代码。
