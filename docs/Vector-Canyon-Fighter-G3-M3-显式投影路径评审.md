# Vector Canyon Fighter G3：M3 显式投影路径评审

> 状态：Review 通过，允许提交
> 日期：2026-09-02
> 前置节点：M2 已确认并提交（`af3092d`）
> Feature flag：`VECTOR_CANYON_EXPLICIT_TERRAIN=0`
> 本节点新增未启用的显式投影路径，不显示新地图

## 1. 本阶段交付

新增：

```text
main/apps/app_vector_canyon_fighter/explicit_canyon_projection.h
tools/vector_canyon_explicit_projection_test.cpp
```

修改：

```text
main/apps/app_vector_canyon_fighter/vector_canyon_renderer.h
main/apps/app_vector_canyon_fighter/vector_canyon_renderer.cpp
```

未修改模型生成、旧地形绘制、碰撞、FlightModel、输入、战机、HUD 和配色。`render()` 仍只调用 `drawLegacyTerrain()`。

## 2. 统一相机矩阵

所有显式世界点使用同一个相机：

```text
cameraPosition = C(playerS) + Up * (flight.altitude + 0.45)
cameraForward  = normalize(T(playerS) + pitch)
cameraRight    = normalize(cross(worldUp, cameraForward))
cameraUp       = normalize(cross(cameraForward, cameraRight))
```

构图基线按 M0 从 450×450 归一化到 468×466：

| 参数 | 结果 |
| --- | ---: |
| principal X | 234.000 |
| principal Y | 161.702 |
| focal length | 387.036 |
| 中性相机高度 | 0.950 |
| 中性俯角 | -3.1° |

相机固定在路线中心，不应用 `flight.lateralOffset`；未来战机自身横移，不再同时移动地形，避免双倍横移。

## 3. 相机空间近裁剪

每个点先变换为 camera X/Y/Z：

- `cameraZ < 0.20` 使用 `INT16_MIN` sentinel，不写额外 bool 数组；
- 两端都在近裁剪面后方：丢弃线段；
- 两端都在前方：直接使用缓存投影；
- 一端在后方：在相机空间把线段插值到 `cameraZ=0.20`，再投影；
- 禁止先投影再在屏幕空间“拉回”。

Host test 覆盖完全不可见、完全可见和跨近裁剪面三种情况；投影坐标均为有限值，跨裁剪线不会产生贯穿屏幕的长线。

## 4. 固定投影工作集

```cpp
struct ProjectedCanyonPoint {
    int16_t x;
    int16_t y;
};

std::array<ProjectedCanyonPoint, 34 * 25> _explicitTerrainPoints;
```

| 项目 | 结果 |
| --- | ---: |
| 单点 | 4 B |
| 850 点缓存 | 3400 B |
| `drawExplicitTerrain()` 目标栈帧 | 256 B |
| Host 过渡期 Renderer 总尺寸 | 7624 B |
| Renderer 编译期上限 | 7700 B |

缓存属于堆上的 App/Renderer 对象，不占 8 KB 主任务调用栈。M3 同时保留 legacy 与 explicit 缓存用于安全回退；显式路径稳定后再按阶段移除过渡冗余。

## 5. 网格与 LOD

绘制顺序：

1. 由远至近画完整 25 点横截面肋线；
2. 按固定语义 profile index 画纵向导轨；
3. 近景悬崖面添加稀疏确定性对角线。

生产 structural 集合锁定 7 条：

```text
0, 3, 7, 12, 17, 21, 24
```

即左右台地外沿、左右崖顶、左右墙脚和谷底中心，始终可见。

Mid 集合锁定 4 条：

```text
5, 9, 15, 19
```

- mid：深度 20–25 使用 smoothstep 淡出；
- fine：深度 9–14 使用 smoothstep 淡出；
- 横截面肋线完整保留；
- 对角线使用 fine fade。

A4 评审文字和 M0 生产契约都要求 7 条 structural rail，但 A4 离线辅助函数实际漏列了 0/24 两条台地外沿。M3 按后续已确认的 M0 语义契约修正为 7 条，并用测试固定，避免远景台地边界消失。

## 6. Host Projection Test

严格版与 ASan/UBSan 版均通过：

- [x] 相机三轴单位化且两两正交；
- [x] `cross(worldUp, forward)` 方向正确；
- [x] 正 lateral 映射到屏幕右侧；
- [x] 相机不应用玩家 lateralOffset；
- [x] 近裁剪三种分支通过；
- [x] 7 structural / 4 mid 语义索引通过；
- [x] 所有 LOD 权重在 `[0,1]` 且随深度单调不增；
- [x] 850 点工作集无 NaN/Inf；
- [x] sanitizer 无越界或未定义行为。

默认静态模型指标：

| 指标 | 结果 |
| --- | ---: |
| 可见投影点 | 825 / 850 |
| 横截面肋线 | 816 |
| LOD 后纵轨 | 487 |
| 含近景对角线理论线段上限 | 1371 |

线段总量维持当前设备可接受的千级预算。

## 7. ESP32-S3 构建与烧录

最终构建：

- [x] 目标端编译和链接通过；
- [x] `drawExplicitTerrain()` 即使未启用也被目标工具链编译；
- [x] 850 点缓存和 Renderer 总尺寸 static_assert 通过；
- [x] BIN 与分区检查通过；
- 固件大小：`0x463fd0` B；
- 分区余量：`0x8c030` B（11%）；
- BIN SHA-256：`092d54dc35f2f9291a2d6e83354819113fe92c7b052d721854580be68080e488`。

烧录：

- [x] 串口 `/dev/cu.usbmodem83401`；
- [x] ESP32-S3 rev 0.2 / 8 MB PSRAM；
- [x] Bootloader、应用、分区表和 OTA 初始区写入成功；
- [x] 所有写入区 Hash 校验通过；
- [x] RTS 硬复位成功。

## 8. M3 Checklist

- [x] 新增 `drawExplicitTerrain()`；
- [x] 世界点统一经过相机矩阵；
- [x] 跨近裁剪面线段先裁剪后投影；
- [x] 无额外 850 B 可见性 bool 数组；
- [x] 投影缓存不在主任务栈；
- [x] structural/mid/fine 集合固定；
- [x] 线段预算通过；
- [x] Feature flag 保持 0；
- [x] legacy Renderer 调用和公式未修改；
- [x] 构建、host test、sanitizer 与烧录通过；
- [x] 用户确认旧地图、战机、HUD、输入和稳定性正常。

用户于 2026-09-02 确认真机表现“正常”，M3 legacy 回归通过。

## 9. 提交门禁

M3 提交条件现已全部满足。提交本阶段后进入 M4：打开显式地形开关，先只显示静态直线峡谷，关闭 HUD、战机、输入和运动，进行第一次新地图真机视觉 Review。
