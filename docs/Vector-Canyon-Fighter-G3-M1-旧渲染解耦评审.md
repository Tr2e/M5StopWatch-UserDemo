# Vector Canyon Fighter G3：M1 旧渲染解耦评审

> 状态：Review 通过，允许提交
> 日期：2026-09-02
> 前置节点：M0 生产接口与迁移评审已确认并提交（`143e2df`）
> 本节点只重构旧地形渲染边界，不迁入显式峡谷、不改变画面设计

## 1. 本节点目标

- 从整帧 `Renderer::render()` 中抽出旧地图绘制函数；
- 保持旧投影公式、颜色、LOD、线条方向和绘制顺序不变；
- 把约 4 KB 地形投影缓存移出 8 KB 主任务栈；
- 为 M2 的显式峡谷生产模型建立可替换的渲染边界；
- 先通过旧地图真机视觉回归，再允许提交。

## 2. 实际改动

### 2.1 渲染边界

新增私有函数：

```cpp
bool drawLegacyTerrain(
    const FlightState& flight,
    const TerrainStream& terrain,
    int centerX,
    uint16_t terrainPrimary,
    uint16_t terrainMid,
    uint16_t terrainSecondary,
    int& vanishingX);
```

`render()` 仍负责：

- 开始和结束显示事务；
- 校准页；
- 清屏；
- 战机；
- HUD；
- 碰撞提示。

`drawLegacyTerrain()` 只负责原有地形投影和线框绘制，并返回 HUD 需要的消失点 X。

### 2.2 投影缓存所有权

原先每帧在 `render()` 栈上创建：

```cpp
ProjectedTerrainRow rows[27];
size_t sourceRows[27];
```

现在改为 `Renderer` 的固定成员缓存：

| 缓存 | ESP32-S3 占用 |
| --- | ---: |
| 27 × 148 B 投影行 | 3996 B |
| 27 × 4 B 源行索引 | 108 B |
| 合计移出调用栈 | 4104 B |

新增编译期约束，防止投影行因后续字段或对齐变化而无声膨胀。

## 3. 语义等价 Review

逐项对照拆分前实现：

- [x] 近裁剪面插值公式未改；
- [x] 世界 X、地形高度和 Z 的取值未改；
- [x] `projectX()` / `projectY()` 未改；
- [x] 远、中、近三档颜色阈值未改；
- [x] 横向行线 stride 未改；
- [x] 纵向列线 stride 未改；
- [x] 最远行先画、随后由远至近连接的顺序未改；
- [x] `rowCount == 0` 时仍结束显示事务并返回；
- [x] 消失点计算时机与公式未改；
- [x] 战机、HUD、校准页和碰撞绘制代码未改；
- [x] 未创建第二张全屏 Sprite；
- [x] 未启用或迁入显式峡谷算法。

结论：代码 Review 未发现可见语义变化。最终结论仍以真机画面为准。

## 4. 栈与固件证据

ESP32-S3 目标文件反汇编的函数入口：

| 函数 | 当前栈帧 |
| --- | ---: |
| `Renderer::render()` | 384 B |
| `Renderer::drawLegacyTerrain()` | 112 B |

两层同时存在时的固定函数栈帧约 496 B；约 4104 B 的投影数组已经转为对象成员，不再随每帧调用占用主任务栈。

构建命令：

```text
IDF_PATH=/Users/xudanyang/Documents/stopwatch/esp-idf cmake --build build -j4
```

加入编译期内存约束后的最终构建结果：

- [x] C/C++ 编译通过；
- [x] ELF 链接通过；
- [x] ESP32-S3 BIN 生成通过；
- [x] 分区大小检查通过；
- 固件大小：`0x463f60` B；
- 最小应用分区余量：`0x8c0a0` B（11%）；
- BIN SHA-256：`309146f2b930fa90d644c0b86fd5706faee044b0f32a62ccf69279ef5b3c8801`。

## 5. 真机视觉回归 Checklist

烧录证据：

- [x] 目标串口：`/dev/cu.usbmodem83401`；
- [x] 芯片识别：ESP32-S3 rev 0.2，8 MB PSRAM；
- [x] Bootloader、应用、分区表和 OTA 初始数据写入成功；
- [x] 所有写入区 Hash 校验通过；
- [x] 烧录后 RTS 硬复位成功。

进入 Vector Canyon Fighter，完成校准后检查：

- [x] 启动画面和校准页正常；
- [x] 地图构图与 M0 前一致；
- [x] 横线、纵线数量与方向一致；
- [x] 地形运动连续，无闪烁、断帧或残影；
- [x] 战机尺寸、位置、姿态与原版一致；
- [x] HUD 布局、数值和消失点指示一致；
- [x] 操作响应、碰撞提示与原版一致；
- [x] 连续运行至少 60 秒，无重启或异常退出。

用户于 2026-09-02 确认真机表现“正常”，M1 视觉回归通过。

## 6. 提交门禁

M1 提交条件：

1. 增量构建再次通过；
2. 固件成功烧录；
3. 用户确认上述真机视觉回归无误；

三项现已全部满足，可以提交 M1 并进入 M2（显式峡谷生产模型与固定剖面）。
