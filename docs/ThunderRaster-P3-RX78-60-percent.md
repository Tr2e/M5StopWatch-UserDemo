# ThunderRaster P3：RX-78 60% 片内行带研究

## 定位

**ThunderRaster（雷栅）** 是本仓库高性能嵌入式 3D 框架的对外名称。目标是将已在 ESP32-S3 上签署的资产编译、紧凑命令、共享顶点、精确软件光栅、双核行带、稀疏清理与合成能力，逐步收敛为 ESP32-S3，并可下探 ESP32 系列的通用高性能 3D 解决方案。

名称不触发 namespace/ABI 重命名；性能和正确性证据优先于目录纯度。

## 已锁定的 E9 基线

- 场景：纯黑背景 RX-78 转台，254×254 内部渲染，稀疏最近邻放大到 424×424。
- 三轮平均帧间隔：`50.276 / 50.261 / 50.260 ms`，合并基线 `50.266 ms / 19.89 FPS`。
- 阶段：显示 framebuffer 清理约 `4.42 ms`，raster begin 约 `2.05 ms`，panel prepare 约 `6.41 ms`，main/worker raster 约 `28.80 / 28.51 ms`，blit 约 `5.30 ms`。
- 路径：60% `fast_path_all=31`，片内投影 SoA、occupancy 和 12 B 直接命令流稳定命中。
- 内存：稳定 internal free `22,595 B`，最大连续块约 `7.5–9 KiB`。
- 22 FPS 门槛：`<45.455 ms`，尚需净省约 `4.81 ms`。

任何 P3 候选都必须与该基线在同机、同编译配置、同轨迹和同统计开关下 A/B，不使用早期 Museum 65% 数据替代当前基线。

## 专用调试固件

实验构建同时开启：

```text
STOPWATCH_BENCHMARK_AUTORUN=ON
STOPWATCH_BENCHMARK_RX60_ONLY=ON
STOPWATCH_COLLECT_TOPOLOGY_STATS=ON
```

启动后直接进入第四阶段，只运行三轮 6.8 s RX-78 60% 测量；不构建阶段 1–3 资源，不执行 100% 回合。日志仍保留 `stage=3`，便于复用现有解析器和 E9 证据。该开关必须与 autorun 同时使用，产品默认为 OFF。

2026-09-24 已在分支 `perf/thunderraster-rx60-p3` 完成首个真机基线：启动日志确认 `renderer=ThunderRaster`、`rx60_only=1`，三轮帧间隔为 `50.277 / 50.309 / 50.316 ms`，合并均值 `50.301 ms / 19.880 FPS`。相对 E9 的 `50.266 ms` 仅慢 `0.035 ms / 0.069%`，属于代码布局与测量噪声量级，可作为 P3 分支零点；三轮 `fast_path_all=31`，没有阶段 1–3 或 100% 样本混入。证据见 [`assets/thunderraster-p3-rx60-baseline/README.md`](assets/thunderraster-p3-rx60-baseline/README.md)。

## P3 研究边界

不重复已证明会退化的“每个微带重新 setup/回放所有图元”。原型必须同时解决：

1. 一次分箱，只为跨带图元保留必需持续状态。
2. 每行保持原始图元顺序、ABC→ACD 顺序、Q13 depth 和等深覆盖语义。
3. 两核对称使用片内工作集，并依真机耗时重新分配行区间；只加速主核不构成整帧收益。
4. 行带完成后直接写最终 framebuffer，避免完整 PSRAM color/depth 中间平面和独立 blit 重复读取。
5. 片内资源不能破坏 E9 的投影 SoA、occupancy、命令流和 worker 栈安全余量；容量失败必须完整回退 E9。

## 停损与成功门槛

- P3-0 先只加统计：测量 8/12/16 行带的总成员数、跨带重复、最大活跃图元、状态字节与两核预计工作量。
- 如精确持续状态+双核工作带无法在不驱逐 E9 快路径的条件下驻留，停止完整实现。
- 首个可渲染原型必须进入 `<47.5 ms`，且分箱/状态准备新增 `≤0.5 ms`，否则停止工程化。
- 成功仍以三轮稳定 `<45.455 ms / >22 FPS` 为准，不以局部内核变快或单轮噪声代替。
- 必须通过现有 60/65/90/100 framebuffer、256 组 RGB565/Q13 depth/occupancy 严格门禁与 ASan/UBSan；快速固件只减少设备采样时间，不减少合并前的完整回归。
