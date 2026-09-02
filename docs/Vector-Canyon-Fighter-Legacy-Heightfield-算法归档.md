# Vector Canyon Fighter · Legacy Heightfield 地图算法归档

> 归档状态：旧实现已清理，归档仅作算法资产与迁移复盘
>
> 归档日期：2026-09-02
>
> 最后完整源码快照：Git commit `beb655e`

## 1. 归档目的

第一代峡谷使用“沿前进方向滚动的二维高度场”生成山脉，再把规则采样网格透视投影成线框地形。它解决了低内存、确定性生成、无限滚动和近景连续等问题，但很难稳定表达游戏最终需要的平顶台地、陡直悬崖、左右独立侵入及路线局部坐标。

M7 已由 `ExplicitCanyonStream` 完整取代该路径。本文保存旧算法的设计意图、关键常量、核心公式、渲染与碰撞方式以及退役原因。需要精确恢复原实现时，可读取：

```text
git show beb655e:main/apps/app_vector_canyon_fighter/model/terrain_stream.h
git show beb655e:main/apps/app_vector_canyon_fighter/model/terrain_stream.cpp
git show beb655e:main/apps/app_vector_canyon_fighter/vector_canyon_renderer.cpp
git show beb655e:main/apps/app_vector_canyon_fighter/model/collision_model.cpp
git show beb655e:tools/vector_canyon_terrain_preview.cpp
```

## 2. 思路总览

```text
seed
  ├─ 整数哈希 → 二维 value noise
  ├─ Catmull–Rom 控制点 → 1 条谷底中心骨架
  └─ Catmull–Rom 控制点 → 左右各 3 条山脉骨架
                         ↓
              每个 worldZ 生成 37 个高度样本
                         ↓
               26 个 TerrainSlice 环形前移
                         ↓
       近裁切插值 + 透视投影 + 深度 LOD 网格连线
                         ↓
      未来 4.8 单位内用半宽/最高谷底估算碰撞
```

模型本质是标量函数：

```text
height = H(worldX, worldZ, seed)
```

每个切片保存固定世界深度上的 37 个高度值。屏幕横线来自同一切片内相邻列，纵线来自相邻切片的同一列。

## 3. 数据模型与内存策略

关键结构：

```cpp
inline constexpr std::size_t kTerrainColumnCount = 37;

struct TerrainSlice {
    float z;             // 当前相机空间深度，会随飞行递减
    float worldZ;        // 确定性生成使用的世界深度
    float center;        // 峡谷中心 X
    float halfWidth;     // 可飞行区域的对称半宽
    float floor;
    float floorTilt;
    float floorCrown;
    float leftWall;
    float rightWall;
    std::array<float, 37> surfaceHeights;
};

class TerrainStream {
public:
    static constexpr size_t kSliceCount = 26;
    static constexpr float kTerrainMinX = -5.6f;
    static constexpr float kTerrainMaxX = 5.6f;
};
```

设计约束：

- 固定 26 × 37 采样，不在帧循环动态分配；
- `worldZ` 决定地形内容，`z` 只决定当前投影深度；
- 列坐标是相对切片中心的局部偏移，使纵线能大致跟随峡谷弯曲；
- 使用相同 seed 和 segment 可重建完全相同的切片。

## 4. 确定性噪声

### 4.1 整数混洗与有符号哈希

```cpp
uint32_t mixBits(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ (value >> 16);
}

float hashSigned(int x, int z, uint32_t seed)
{
    const uint32_t value = mixBits(
        static_cast<uint32_t>(x) * 0x9e3779b9u ^
        static_cast<uint32_t>(z) * 0x85ebca6bu ^ seed);
    return static_cast<float>(value & 0xffffu) / 32767.5f - 1.0f;
}
```

输出稳定落在 `[-1, 1]`，不维护随机数状态，因此任意切片都可按索引独立重建。

### 4.2 平滑二维 value noise

先使用三次 smoothstep：

```text
s(t) = t²(3 - 2t)
```

再对整数格点四角哈希做双线性插值：

```cpp
float valueNoise(float x, float z, uint32_t seed)
{
    const int x0 = static_cast<int>(std::floor(x));
    const int z0 = static_cast<int>(std::floor(z));
    const float tx = smoothStep(x - x0);
    const float tz = smoothStep(z - z0);
    const float a = hashSigned(x0,     z0,     seed);
    const float b = hashSigned(x0 + 1, z0,     seed);
    const float c = hashSigned(x0,     z0 + 1, seed);
    const float d = hashSigned(x0 + 1, z0 + 1, seed);
    return lerp(lerp(a, b, tx), lerp(c, d, tx), tz);
}
```

不同频率、不同派生 seed 的 value noise 叠加，分别提供低频山体、细节扰动与谷底起伏。

## 5. 七条连续骨架

旧算法不直接让噪声决定所有山形，而是先生成 7 条沿 Z 延伸的骨架：

- `0`：峡谷中心/谷底骨架；
- `1..3`：左侧由近到远三层山脉；
- `4..6`：右侧由近到远三层山脉。

控制点间距为 `3.4` 世界单位，控制点之间用 Catmull–Rom 曲线：

```text
C(t) = 0.5 × [2P1 + (-P0+P2)t
       + (2P0-5P1+4P2-P3)t²
       + (-P0+3P1-3P2+P3)t³]
```

中心线由两组正弦波和低通哈希噪声构成；最初 5 个控制点使用 ramp 从直线逐渐进入弯曲，避免启动画面立刻急转。

左右山脉不是中心线的简单平移。每一层组合：

```text
skeletonX = center × coupling
          + side × (baseOffset + independentDrift + slowFold)
```

其中：

```text
baseOffset = [2.35, 3.70, 4.82]
coupling   = [0.84, 0.48, 0.22]
driftScale = [0.34, 0.49, 0.62]
```

越外层越少追随中心线、独立漂移越强，从而避免三条等距平行曲线。

每条山脉还有独立的随 Z 变化振幅：

```text
amplitude = max(0.32,
                CatmullRom(base × sinusoidalPulse + hashedVariation))
```

## 6. 高度合成

### 6.1 基础表面

所有位置先叠加两层小幅噪声：

```cpp
height  = noise(x * 0.43, z * 0.22) * 0.17;
height += noise(x * 0.91, z * 0.47) * 0.07;
```

再按位置与中心骨架的距离生成谷底/山体掩码：

```text
sideDistance = abs(x - centerSkeletonX)
mountainMask = smoothstep((sideDistance - 1.30) / 1.90)
floorMask    = 1 - mountainMask
```

谷底保留较柔和、较宽的滚动噪声；山体使用幅度更大的低频噪声。

### 6.2 骨架包络叠加

对每条骨架使用有限支撑的幂函数包络：

```text
d = abs(x - skeletonX)
e = max(0, 1 - d / radius)
height += amplitude × e^sharpness
```

中心骨架的 `amplitude=-1.55`，并把中心 `1.15` 范围视为零距离，形成宽槽；其余 6 条骨架为正高度，形成左右山脉。半径约 `1.08…1.46`，锐度约 `1.38…1.82`。

### 6.3 谷底局部修饰

每个切片额外计算：

- `floorTilt`：谷底横向斜度；
- `floorCrown`：只允许非负的中部拱起；
- `halfWidth`：约 `1.42 ± 0.18` 的对称飞行半宽。

位于半宽内的采样点使用向边缘衰减的权重加入 tilt/crown。该设计让近场不再完全像平坦跑道，但也使“谷底高度”和实际 37 列表面之间需要保守估算。

## 7. 无限流式更新

关键常量：

```text
sliceSpacing = 0.78
recycleZ     = -0.56
movement     = traveled × 0.018
```

更新时所有切片向相机移动；最前切片越过 `recycleZ` 后，固定数组整体前移一格，并按新的 segment 索引生成末端切片。

```cpp
while (slices.front().z < recycleZ) {
    for (size_t i = 1; i < slices.size(); ++i)
        slices[i - 1] = slices[i];
    ++firstSegment;
    slices.back() = makeSlice(firstSegment + slices.size() - 1);
    slices.back().z = slices[slices.size() - 2].z + sliceSpacing;
}
```

这里不是严格环形缓冲区，而是小型固定数组搬移；26 个元素的成本可控，代码简单且没有堆分配。

## 8. 旧线框渲染算法

### 8.1 透视投影

```text
screenX = centerX + focalX × worldX / z
screenY = horizonY - scaleY × (height - altitude) / z

focalX  = 112
scaleY  = 94
horizon = 150
nearZ   = 0.22
```

战机横移通过 `worldX - lateralOffset` 作用到整个高度场，所以视觉上相机横移、地图反向移动。

### 8.2 近裁切连续性

若一个切片在近裁切面之后、下一个切片在其之前，渲染器按深度比例插值两行的中心和 37 个高度，在固定 `nearZ` 上生成一条临时行。这样切片回收时近景不会跳过完整的 `0.78` 间距。

### 8.3 网格与深度 LOD

每行内部相邻列连成横向网格，相邻行的同列连成纵向网格。远处降低列密度：

```cpp
size_t columnStride(size_t sourceRow)
{
    if (sourceRow > 17) return 3;
    if (sourceRow > 7) return 2;
    return 1;
}
```

颜色也按近、中、远三档衰减。绘制顺序为远到近，使近景线条覆盖远景线条。

## 9. 旧碰撞模型

旧碰撞在相机前方 `z <= 4.8` 的所有切片中寻找最小净空：

```text
lateralClearance = halfWidth
                 - abs(playerLateral - sliceCenter)
                 - shipHalfWidth

highestFloor = floor + abs(floorTilt) + max(floorCrown, 0)
floorClearance = altitude - highestFloor - shipFloorClearance

clearance = min(all lateralClearance, all floorClearance)
warning   = clearance < 0.48
collided  = clearance < 0
```

它采用对称 `halfWidth`，且未来切片既用于 warning 也会提前触发 collision。后来的显式峡谷改成独立 `leftWidth/rightWidth`，并规定前视仅预警、当前位置才碰撞。

## 10. 有价值的经验

- 确定性整数哈希很适合资源受限设备上的可重建程序地形；
- 固定容量切片流能提供无限前进且没有帧内堆分配；
- Catmull–Rom 控制骨架比直接噪声更容易获得连续山脉；
- 近裁切面插值能有效消除滚动网格的前景跳变；
- 深度方向的列 stride 是低成本 LOD；
- 将世界生成深度与相机投影深度分开，是流式生成的重要边界。

## 11. 退役原因

该模型最终被替换，不是因为它不能生成山，而是其表示法与目标玩法不匹配：

1. 高度场只能给每个 `(x,z)` 一个高度，不能自然表达近乎垂直或内凹的悬崖侧面；
2. 规则 37 列网格强调“山脊起伏”，难以突出目标画面的平顶、平谷和陡峭侧壁；
3. 单一对称 `halfWidth` 不能表达左、右悬崖独立侵入；
4. 所有纵线来自固定列索引，弯道中容易产生与路线标架不一致的斜线关系；
5. 相机横移通过整体平移地形实现，和第三人称战机横移组合后容易重复位移；
6. 碰撞依赖保守的谷底最高值估算，视觉表面与玩法边界不是同一几何来源。

`ExplicitCanyonStream` 通过“路线局部标架 + 25 个语义截面点 + 左右独立边界 + 同源渲染/碰撞”解决了这些结构性问题。

## 12. 清理边界

本次退役清理删除：

- `model/terrain_stream.h/.cpp`；
- `Renderer::drawLegacyTerrain()`、旧投影缓存和 legacy render 重载；
- `CollisionModel::evaluate(... TerrainStream ...)`；
- `VECTOR_CANYON_EXPLICIT_TERRAIN` 回退开关与 ActiveTerrain 类型别名；
- `tools/vector_canyon_terrain_preview.cpp`。

保留：

- 本归档文档；
- M0–M7 阶段评审和算法研究文档；
- 新显式峡谷的静态、曲线、事件流、投影、集成测试；
- 显式峡谷自身的 preview/debug feature flags。

## 13. 清理验证

清理候选已完成以下静态和构建验证：

- `main/`、`tools/` 与重新生成的 `compile_commands.json` 中，旧类型、渲染入口、回退 flag 和源文件引用均为 0；
- ESP-IDF 重新配置成功，确认构建图不再包含已删除的 `terrain_stream.cpp`；
- ESP32-S3 全量链接通过，固件由 M7 的 `0x465630` 降至 `0x464fd0`，减少 `0x660`（1632）B；
- App 内的 `Renderer` 对象由 `7624 B` 降至 `3408 B`，释放 `4216 B` 常驻内存；
- 6 组显式峡谷 host 测试通过严格警告门禁；
- 同 6 组测试以 AddressSanitizer + UndefinedBehaviorSanitizer 重建、运行，退出码均为 0；
- 固定 seed 地形哈希仍为 `17445385684075537385`；
- 最大流式 slice 间距误差仍为 `0.00182676`；
- 投影工作集仍为 825 个可见点、最大 1371 条线段；
- 左右独立边界、前视预警、谷底碰撞和相机/战机横移分离测试均通过。

真机门禁亦已完成：

- bootloader、主固件、分区表和 OTA 数据均写入成功并通过 Hash 校验；
- 校准、显式峡谷、线框战机、HUD、左右/前后倾斜和 A/B 输入正常；
- 谷底预警/碰撞、右侧预警/碰撞及长按 A 重开均由日志验证；
- 完整负载稳定 `21.6…22.0 FPS`，render `44…45 ms`，simulation clamp 始终为 0；
- IMU 任务最低栈余量 `2032 B`，主任务最低栈余量 `4324 B`；
- 连续运行超过 200 秒，无崩溃、卡死或重启；
- 退出到 Launcher 后重新进入成功，异步 IMU 任务正常关闭并重建；
- 用户于 2026-09-02 确认画面、战机、HUD 与操控正常。

旧 heightfield 清理提交门禁已满足。
