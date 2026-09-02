# Vector Canyon Fighter G3：M2 显式峡谷生产模型评审

> 状态：内部 Review 通过，等待用户确认
> 日期：2026-09-02
> 前置节点：M1 已通过真机 Review 并提交（`af47bc9`）
> Feature flag：`VECTOR_CANYON_EXPLICIT_TERRAIN=0`
> 本节点只新增生产模型和 host test，不连接 Renderer、CollisionModel 或 App

## 1. 交付内容

新增：

```text
main/apps/app_vector_canyon_fighter/vector_canyon_config.h
main/apps/app_vector_canyon_fighter/model/explicit_canyon_types.h
main/apps/app_vector_canyon_fighter/model/explicit_canyon_stream.h
main/apps/app_vector_canyon_fighter/model/explicit_canyon_stream.cpp
tools/vector_canyon_explicit_production_test.cpp
```

未修改：

```text
legacy TerrainStream
Renderer
CollisionModel
AppVectorCanyonFighter
FlightModel
InputProvider
战机、HUD、配色
```

因此 M2 不产生任何真机画面或操作变化，也不烧录一份与 M1 行为相同的固件。

## 2. 生产数据契约

### 2.1 固定 25 点剖面

`kExplicitCanyonProfile` 是 `inline constexpr std::array`：

- 11 个谷底点严格为高度 0；
- 左右台地严格为高度 3.80；
- 主崖面角度维持 A1 的 80.78°；
- 基线剖面左右镜像；
- 事件只移动侧壁边界，不制造山脊噪声或谷底起伏。

### 2.2 32 B Slice

每个 `ExplicitCanyonSlice` 只保存：

```text
segmentId
worldS
centerX / centerZ
tangentX / tangentZ
leftWidth / rightWidth
```

编译期约束：

```cpp
static_assert(sizeof(ExplicitCanyonSlice) == 32);
```

不保存 25 个世界点、法线、高度场、山脊、floor tilt 或 crown。世界点由固定剖面、局部法线与左右边界即时组合。

### 2.3 坐标与标架

生产模型使用 M0 契约：

```text
s = 中心线累计弧长
T = 单位切线 (tangentX, tangentZ)
N = (tangentZ, -tangentX)
P = C + N*u + Up*h
```

路线由固定种子的 Catmull-Rom 控制点生成，再通过数值弧长反解为等弧长 slice。Renderer 将来只能读取 `worldPoint()`，不允许重新定义网格方向。

## 3. 确定性事件流

事件仍是 A4 的纯函数：

```text
event = eventAtIndex(seed, eventIndex)
```

保持已确认的 S-gate grammar：

- 周期 44 世界单位；
- 左事件中心约 +10；
- 右事件中心约 +28；
- 半长度 5.55–6.30；
- 振幅 0.88–1.15；
- 两个事件之间保留 recovery；
- 固定 6 事件窗口，不使用 `vector` 或帧内堆分配。

左右宽度独立存储。左事件不会修改右边界，右事件不会修改左边界；侧壁、墙脚和台地作为一个剖面整体横移。

## 4. 固定内存结果

Host 与 ESP32-S3 编译期约束均通过：

| 项目 | 结果 | M0 门槛 |
| --- | ---: | ---: |
| `CanyonProfileSample` | 8 B | 8 B |
| `CanyonShoulderEvent` | 16 B | `<=16 B` |
| `CanyonEventWindow` | 100 B | `<=100 B` |
| `ExplicitCanyonSlice` | 32 B | 32 B |
| 34 slice | 1088 B | 1088 B |
| `ExplicitCanyonStream` 总计 | 1200 B | `<=1280 B` |

相较 legacy `TerrainStream` 约 5 KB，显式生产模型减少约 3.8 KB 常驻对象内存。

## 5. Host Test

严格编译：

```text
c++ -std=c++17 -O2 -Wall -Wextra -Werror -pedantic \
  -I main/apps/app_vector_canyon_fighter/model \
  tools/vector_canyon_explicit_production_test.cpp \
  main/apps/app_vector_canyon_fighter/model/explicit_canyon_stream.cpp \
  -o /tmp/vector_canyon_explicit_production_test
```

另用 AddressSanitizer + UndefinedBehaviorSanitizer 重新编译并运行；两种测试均以退出码 0 通过，无越界和未定义行为报告。

### 5.1 A1/A2 几何指标

| 指标 | 结果 | 门槛 |
| --- | ---: | ---: |
| 初始路线 X 跨度 | 2.628900 | `>=2.5` |
| 初始最大 slice 间距误差 | 0.001827 | `<0.025` |
| 60 秒流式最大间距误差 | 0.012512 | `<0.025` |
| 最大单位标架误差 | 0.00000012 | `<0.001` |
| 最大曲率 × 台地半宽 | 0.728224 | `<0.95` |
| 最小外沿前进点积 | 1.425300 | `>0` |

结果说明等弧长采样、局部标架和最外侧台地导轨在 60 秒窗口内均未翻面或向后折叠。

### 5.2 A3/A4 流式指标

模拟条件与 A4 一致：30 FPS、1800 帧、最高飞行速度 176、世界流速 3.168、种子 `0xC4A71001`。

| 指标 | 结果 | 门槛 |
| --- | ---: | ---: |
| slice 回收 | 169 | `>150` |
| 最大事件窗口 | 3 / 6 | 不溢出 |
| 窗口溢出 | 0 | `=0` |
| 最小总通道宽度 | 3.229742 | `>1.44` |
| 缓存/纯函数重算误差 | 0.000000 | `<0.000001` |
| 相邻边界最大变化 | 0.393731 | `<0.40` |
| 最小横移可达余量 | 2.213046 | `>0` |

确定性哈希：

```text
boundary first:       17445385684075537385
boundary repeat:      17445385684075537385
boundary other seed:   7905140194812917043
route first:           7460444554023639947
route repeat:          7460444554023639947
route other seed:     14173910777848830315
```

边界三个哈希与 A4 原型逐值一致；路线也满足同种子一致、不同种子不同。

## 6. ESP32-S3 生产构建

构建命令：

```text
IDF_PATH=/Users/xudanyang/Documents/stopwatch/esp-idf cmake --build build -j4
```

结果：

- [x] 新 `explicit_canyon_stream.cpp` 被 ESP32-S3 工具链实际编译；
- [x] 所有目标端 `static_assert` 通过；
- [x] ELF 链接通过；
- [x] BIN 与分区检查通过；
- [x] Feature flag 保持 0；
- 固件大小：`0x463f60` B；
- 分区余量：`0x8c0a0` B（11%）；
- BIN SHA-256：`ec156dcdb60c2e7d188e32264f6fa5af04e8cefecc5c46bbc8cfdd38d3d7c7ff`。

BIN 大小与 M1 完全相同。新模型始终参与编译，但因 App 尚未引用，在关闭开关时由链接器移除，不改变当前真机路径。

## 7. M2 Checklist

- [x] 新建 25 点 constexpr profile；
- [x] 新建 34 slice 固定缓存；
- [x] 移植 Catmull-Rom 路线、弧长采样和局部标架；
- [x] 移植左右独立事件和 6 事件窗口；
- [x] 同一左右边界同时生成剖面世界点；
- [x] host test 复现 A1–A4 指标；
- [x] 60 秒流式路线无折叠；
- [x] AddressSanitizer / UndefinedBehaviorSanitizer 通过；
- [x] 固定内存 `static_assert` 通过；
- [x] 开关关闭时生产固件构建成功；
- [x] 未接 Renderer、碰撞、App、HUD 或输入；
- [x] 未修改 legacy 路径；
- [x] 内部 Review 发现的窗口首行浮点索引风险已修正并加入回归测试。

## 8. Review 判断与下一步

M2 通过内部 Review。模型满足固定内存、确定性、非对称边界和统一世界坐标契约，没有越过阶段边界，也没有改变当前真机表现。

用户确认本评审后才提交 M2。下一阶段 M3 只新增 `drawExplicitTerrain()` 和固定投影缓存，feature flag 仍保持关闭，并继续用 legacy 真机路径做回归；不会提前展示或启用新地图。
