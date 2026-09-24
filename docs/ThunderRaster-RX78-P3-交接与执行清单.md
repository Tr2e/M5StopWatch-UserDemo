# ThunderRaster RX-78 P3：交接与执行清单

更新日期：2026-09-24

当前分支：`perf/thunderraster-rx60-p3`

代码锚点：`ecc0068`（P3-0 已完成；本交接提交只增加/修正文档）

设备：StopWatch ESP32-S3 rev 0.2，串口 `/dev/cu.usbmodem83301`，MAC `44:1b:f6:c1:8a:00`

这份文件是继续当前优化任务的**唯一入口**。先按本页恢复上下文和验证零点，再读链接中的细节；不要从历史 TODO 自行挑选另一条路线。

## 1. 接手后的前十分钟

按顺序执行，不跳步：

1. `git switch perf/thunderraster-rx60-p3`
2. `git status --short --branch`：除用户已有的 `tools/arena_motion/__pycache__/` 外应无未提交修改；不要暂存、删除或改动该目录。
3. `git log -4 --oneline`：应包含代码锚点 `ecc0068` 以及前序 `14caa2b`、`f7ada89`。
4. 完整阅读本文件和 [`ThunderRaster-P3-RX78-60-percent.md`](ThunderRaster-P3-RX78-60-percent.md)。只有需要追溯已接受/否决内核实验时，再查 [`RX78-60-percent-performance-plan.md`](RX78-60-percent-performance-plan.md) 的 E3–E9 和相应 `docs/assets/`。
5. 确认设备当前运行的是关闭 band probe 的 RX-78 60% 专用固件。启动行必须含 `renderer=ThunderRaster rx60_only=1 band_probe=0`，RX 会话必须稳定为 `fast_path_all=31`。
6. 若以上任一项不成立，先恢复零点，不开始候选。

## 2. 当前任务，不再重新决策

短期任务是 P3-1：为纯黑 RX-78 60% 路径实现一个**有界、可完整回退**的 8 行双核片内工作带原型，验证能否同时降低 PSRAM raster 与独立 blit 的整帧成本。

首版结构已经锁定：

- 保留每帧约 1,919 条唯一的 12 B indexed command，不复制成逐带命令。
- 复用现有 band-index scratch，按 8 行建立连续的 16-bit command-index 列表；254 行共 32 带。
- CPU0 顺序处理偶数带，CPU1 顺序处理奇数带。带之间像素行不重叠，带内命令顺序必须与当前 pass-0、pass-1 稳定顺序完全一致。
- 每核持有一套 8×254 RGB565 color 和 Q13 depth。color/depth 分开申请，每块 4,064 B；双核四块合计 16,256 B。
- 候选路径不再需要全尺寸片内 occupancy；应以显式资源规划换回约 8.1 KiB，而不是在当前 E9 余量上盲加 16 KiB。任一必需块失败，整帧回退 E9，禁止半启用。
- 每个带完成后按现有最近邻映射写入最终 424×424 framebuffer 对应的互不重叠目标行，再复用工作带。两个核心 join 后才允许 present。
- 首版允许跨带图元在其成员带内重新执行精确 triangle/quad setup，但只回放该带成员，不得重扫完整模型；若 setup 重复吃掉收益，依据真机门槛停止或进入有数据支持的持续状态设计。

不要在首版同时加入动态调度、二维 tile、LOD、材质系统、namespace 重命名或公共 API 重构。P3-1 是一个被编译开关隔离的性能原型，不是借机整理代码。

### 当前任务板

- [x] 建立独立分支、RX-78 60% 专用固件和同分支零点。
- [x] P3-0：完成 8/12/16 行容量、重复和双核负载探针。
- [ ] P3-1A：加入默认关闭的原型开关和全有/全无资源组，连续三次稳定分配。
- [ ] P3-1B：生成唯一命令流与稳定 8 行索引；`bin_us ≤0.5 ms`。
- [ ] P3-1C：完成片内 8 行精确光栅和直接 framebuffer 合成，补齐边界主机测试。
- [ ] P3-1D：接入双核奇偶带、join、阶段计时和 E9 完整回退。
- [ ] 通过 ASan/UBSan、1,408 组 framebuffer、256 组 Q13/occupancy 和新增行带边界门禁。
- [ ] 真机三轮 `<47.5 ms`：决定继续调优或按停损回退。
- [ ] 真机三轮 `<45.455 ms / >22 FPS`：P3-1 才可签署完成。
- [ ] 恢复完整产品构建，复核阶段 1–3、100%、Museum 65%、退出重建和按键行为。
- [ ] 只有上述全部完成后进入 P3-2 公共框架提炼。

每个勾选项必须在 `docs/assets/` 有可定位的命令、日志、数值和结论；实现存在但没有证据，不得勾选。

## 3. 已锁定零点

| 项目 | 数值 |
| --- | ---: |
| E9 长期基线 | 50.266 ms / 19.89 FPS |
| P3 分支零点 | 50.301 ms / 19.880 FPS |
| 关闭探针恢复复测 | 50.328 ms / 19.869 FPS |
| 22 FPS 门槛 | <45.455 ms |
| 首版继续门槛 | <47.500 ms |
| 尚需净省（相对 E9） | 约 4.81 ms |
| framebuffer clear | 约 4.42 ms |
| depth/raster begin | 约 2.05 ms |
| panel prepare | 约 6.40 ms |
| CPU0 / CPU1 raster | 约 28.82 / 28.55 ms |
| blit | 约 5.30 ms |

零点证据：[`assets/thunderraster-p3-rx60-baseline/README.md`](assets/thunderraster-p3-rx60-baseline/README.md)。P3-0 证据：[`assets/thunderraster-p3-band-probe/README.md`](assets/thunderraster-p3-band-probe/README.md)。设备当前无探针固件 BIN 为 `0x460dc0`，SHA-256 为 `88c89b3d240cb6f86ad7c6028ef830601394b4428f1cf27d95ae85b65129c126`。

不得把以下数据与零点混算：Museum 65% 房间场景、100% RX-78、阶段 1–3、开启 `STOPWATCH_RX_BAND_PROBE` 后约 56.15 ms 的诊断帧时，以及旧 32 行全模型重放条带。

## 4. P3-0 已回答的问题

| 行带 | 平均成员 | 跨带重复 | 双核 color+depth | 双核扫描行差 | 结论 |
| ---: | ---: | ---: | ---: | ---: | --- |
| 8 | 3,787.7 | 97.3% | 15.88 KiB | 4.19% | 首版唯一安全规格 |
| 12 | 3,202.7 | 66.9% | 23.81 KiB | 4.73% | 会挤压当前快路径 |
| 16 | 2,839.0 | 47.9% | 31.75 KiB | 2.78% | 当前片内预算不成立 |

8 行复制成 12 B 命令会达到约 44.4 KiB，所以只能存 16-bit 成员索引。平均 3,788 个索引约 7.4 KiB，小于现有 `2 × Mesh::capacity` band-index scratch。P3-0 的统计本身约增加 5.62 ms，只能用于容量判断；运行性能候选时必须关闭它。

## 5. P3-1 实现顺序

严格按以下顺序推进，一个检查点只解决一个风险：

### A. 隔离与资源规划

- 新增独立编译开关，例如 `STOPWATCH_RX_MICROBAND_PROTOTYPE`，默认 `OFF`，并要求 `STOPWATCH_BENCHMARK_RX60_ONLY=ON`。
- 候选只在 60% trusted RX-78 indexed path 命中；其他比例、Museum 产品路径和通用 Soft3D 必须直接走 E9。
- 在 `MuseumRenderer::open()` 建立全有或全无的资源组：四个 4,064 B 工作块、既有 projected SoA、ready、唯一 indexed commands 和 band indices。记录每块是否片内、最终 free/largest block；失败立即释放候选块并保留 E9。
- 候选启用时新增单独 fast-path bit；预期 E9 为 `31`，P3-1 可用时才增加该 bit。不要复用旧位让日志失真。

检查点：连续三次重建 renderer 都必须走同一路径；不得出现首轮命中、后两轮回退。建议 internal free 保持至少约 14 KiB、单块分配需求不超过 4,064 B；worker 栈高水位不得低于当前安全区间。若资源规划只能靠偶发分配成功，立即停止。

### B. 唯一命令与稳定分箱

- 先按现有 pass-0、pass-1 顺序生成唯一命令流。
- 第一遍只从 projected SoA 的四个 y 值求 `firstBand/lastBand` 并累计 32 个 count；做 prefix sum，检查总 membership 不超过 band-index capacity。
- 第二遍按相同命令升序重新求 band 范围并把 command index 追加到各带。这样不保存临时 bounds、不重扫模型，也不会超过 scratch。
- 任一命令容量、membership、band 或资源 guard 失败，整帧走 E9；不得截断、降画质或部分绘制。

检查点：新增 `bin_us`，三轮平均必须 `≤0.5 ms`。固定姿态下每个带的索引必须严格递增；总 membership 应落在 P3-0 的约 3,788 区间，明显偏离先查边界公式和路径。

### C. 精确 8 行目标与直接合成

- 从现有 `trustedSolidFace`/`solidQuadRowsExact` 提取“显式 target + 全局 clip row”的最小能力，不复制另一套大像素内核。
- Q13 depth 表达式、`d < oldDepth`、等深覆盖、RGB565、ABC 后 ACD、`.001/.00001` 边界和命令顺序保持不变。
- band-local depth 每次复用前清零；空像素 color 不需要读取。目标索引必须把全局 y 正确映射到 0–7 本地行。
- 直接合成必须复用当前最近邻映射规则。一个目标行只能由一个源带/核心写入；保留 dirty bounds 与双 framebuffer 生命周期。

检查点：先在主机加入跨 7/8、15/16 行边界、极薄 quad、退化 triangle、等深 ABC/ACD 和最后不足 8 行的用例，再跑完整门禁。不要以最终颜色相同替代 Q13 depth/occupancy 一致。

### D. 双核奇偶带与真机 A/B

- CPU1 常驻 worker 处理奇数带，CPU0 处理偶数带；每核串行复用自己的工作区。
- 记录 `bin_us`、偶/奇带 clear、raster、composite、join、完整 interval，以及 internal free/largest 和 worker stack。
- 两核只写不重叠 framebuffer 行；在下一帧 framebuffer clear、DMA/present 或释放资源前必须 join。
- 不要先做工作窃取。P3-0 的扫描行差只有 4.19%；只有真机显示一核稳定成为关键路径，才单独设计重新分配。

检查点：同一固件三轮稳定，候选完整 interval `<47.5 ms` 才继续工程化；最终接受要求三轮稳定 `<45.455 ms / >22 FPS`。

## 6. 正确性与性能硬门槛

任一项失败，候选不得接受：

- `SANITIZE=1 bash tools/test_soft3d.sh`
- `SANITIZE=1 bash tools/test_gundam_museum_perf.sh`
- 60/65/90/100 共 1,408 组 framebuffer 一致。
- 256 组 skew-quad 的 RGB565、每个 Q13 depth 和 occupancy 逐点一致。
- 新增 8 行边界与尾带测试通过。
- 三轮 `fast_path`、内存和命令/membership 容量稳定；失败路径可完整回退 E9。
- 真机比较必须同设备、同轨迹、同开关、同统计口径；先看完整 interval，再解释局部阶段。
- 候选 BIN/IRAM、internal free/largest、worker stack、原始串口日志和 SHA-256 全部记录。

首版若 `≥47.5 ms`，或 bin `>0.5 ms`，先确认没有误走回退、重复全模型扫描、probe 未关闭或分配失效；排除这些错误后仍不达标，就记录否定并回退，不继续堆叠优化。局部 raster 变快但 framebuffer composite、DMA 争用或整帧变慢，同样否决。

## 7. 明确禁止重开的路线

- 旧 `424×32` 条带：每带重扫/回放全部面板，65%/100% 分别慢 12.0%/8.6%。
- E7 逐像素递增重心：颜色门禁表面通过，但严格 Q13 depth 不一致。
- 普通三角机械复制 quad 行指针优化：60% 退化到 50.925 ms。
- `px += 1`、六组 delta 扩大 setup、10 B 非对齐命令、复制像素内核模板：均已因正确性、寄存器 spill、代码布局或片内分配退化否决。
- 自由 front-to-back 重排、粗深度、改变等深覆盖、降低采样率、简化 RX-78 几何。
- PSRAM 直连 SPI/GDMA：可靠传输版本慢于 bounce，80 MHz 快速结果伴随 TX underflow 和花屏。
- 为 ThunderRaster 改 namespace/目录/ABI：当前没有性能收益，只增加回归面。

若新硬件事实改变否决前提，必须先写清“哪个前提改变”，不能仅以代码形式不同重做同一实验。

## 8. 构建、烧录与采样命令

主机门禁：

```sh
SANITIZE=1 bash tools/test_soft3d.sh
SANITIZE=1 bash tools/test_gundam_museum_perf.sh
```

RX-78 60% 快速固件基线配置：

```sh
source /Users/xudanyang/Documents/stopwatch/esp-idf/export.sh
idf.py -B build-thunderraster-rx60 \
  -DSTOPWATCH_BENCHMARK_AUTORUN=ON \
  -DSTOPWATCH_BENCHMARK_RX60_ONLY=ON \
  -DSTOPWATCH_RX_BAND_PROBE=OFF \
  -DSTOPWATCH_COLLECT_TOPOLOGY_STATS=ON build
```

P3-1 实现后只额外开启原型开关。烧录和监听：

```sh
idf.py -B build-thunderraster-rx60 -p /dev/cu.usbmodem83301 app-flash
idf.py -B build-thunderraster-rx60 -p /dev/cu.usbmodem83301 monitor
```

串口每次必须确认：只出现 stage 3、render percent 60、三轮完成、probe 关闭、预期 fast-path bit 稳定。采样后退出 monitor，计算三轮均值，不只引用 `fps_x10`。

## 9. 每轮实验的提交与证据

每个候选建立独立 `docs/assets/thunderraster-p3-<实验名>/`，至少包含：

- `README.md`：假设、唯一变量、基线/候选表、结论和回退状态。
- 筛选后的原始串口日志。
- commit、构建开关、BIN 大小、SHA-256、设备和时间。
- 主机命令/退出码、framebuffer/depth/occupancy 结果。
- 三轮 interval 与分阶段数据、内存/栈和 fast-path flags。

提交纪律：一个可解释候选一个实现提交；真机接受后再更新规范。若否决，先回退实现，再以文档提交保留负结果。不要把多个候选叠在同一固件后才测，也不要把未验证猜想写成 ThunderRaster 已有能力。除非用户明确要求，不推送远端。

## 10. 完成定义与后续边界

P3-1 只有同时满足严格像素门禁、稳定资源命中和三轮 `<45.455 ms` 才标记完成。达到 `<47.5 ms` 但未过 22 FPS 只能标记“原型成立，继续 P3-1 调优”，不能对外宣称达标。

P3-1 完成后才进入 P3-2：把资源预算、band scheduler、fallback 和能力查询提炼成 ThunderRaster 公共机制，并在阶段 1–3、Museum 65% 和至少一个非 RX-78 资产上重新签署。RX-78 固定尺寸、相机和拓扑不得泄漏为公共 API 假设。
