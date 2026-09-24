# RX-78 Benchmark 60% E9：quad 扫描边不变量

日期：2026-09-24

设备：StopWatch ESP32-S3，`/dev/cu.usbmodem83301`

配置：`STOPWATCH_BENCHMARK_AUTORUN=ON`、`STOPWATCH_COLLECT_TOPOLOGY_STATS=ON`，每次启动连续三轮 100%→60%。

## 判断

E9 保持原重心、覆盖与 Q13 depth 表达式，只把 solid-quad 每条扫描行反复计算的三条边 `minY/maxY` 与水平边分类提升到图元 setup；同时把 occupancy 指针明确为与 color/depth 不重叠的独立存储。像素顺序、深度比较、颜色和 occupancy 结果不变。

| 项目 | E8 | E9 | 变化 |
| --- | ---: | ---: | ---: |
| 60% 三轮平均帧间隔 | 50.705 ms | 50.266 ms | -0.439 ms / -0.87% |
| 60% FPS | 19.72 | 19.89 | +0.88% |
| 100% 三轮平均帧间隔 | 85.326 ms | 84.305 ms | -1.021 ms / -1.20% |
| 60% main raster | 29.142–29.153 ms | 28.797–28.810 ms | 约 -0.35 ms |
| 60% worker raster | 28.957–28.987 ms | 28.511–28.517 ms | 约 -0.46 ms |
| 诊断 BIN | 0x460b20 | 0x460d90 | +624 B |
| 60% 最终 internal free | 23,091 B | 22,595 B | -496 B |

60% 三轮为 `50.276 / 50.261 / 50.260 ms`，100% 三轮为 `84.298 / 84.307 / 84.309 ms`。六次 RX 会话保持 `projected/occupied/commands=true`（100% command 仍按既有容量回退），60% `fast_path_all=31`，100% `fast_path_all=7`。

## 门禁

- `SANITIZE=1 bash tools/test_soft3d.sh`：通过。
- `SANITIZE=1 bash tools/test_gundam_museum_perf.sh`：通过；60/65/90/100 共 1,408 组最终 framebuffer 一致。
- 256 组 deterministic skew-quad：RGB565、每个 Q13 depth 和 occupancy 逐点一致。
- ESP-IDF 5.5.4 诊断构建通过；BIN `0x460d90`，应用分区余 `0x8f270`（11%）。
- 阶段 1–3 的 100% 三轮平均帧间隔为 `22.279 / 34.979 / 27.310 ms`；60% 为 `13.684 / 19.417 / 16.384 ms`，无有意义回退。

## 同轮拒绝项

- 把 E8 的行指针方案机械复制到普通三角：60% 退化到平均 `50.925 ms`；普通三角/短 span 数量不足以摊销逐行准备，已撤回。
- `px += 1` 递增：最终颜色门禁不足，严格 Q13 depth 对比失败，未烧录。
- 继续预存六个 `q/r` 坐标差和深度差：主机微基准约快 7–9%，但 Xtensa 发生寄存器 spill/IRAM 克隆布局变化，真机 60% 退化到平均 `55.425 ms`、100% 退化到 `93.282 ms`，已撤回。
- 把 `.001` 边界容差也折叠进已有 `edgeMinY/edgeMaxY`：60% 三轮平均退化到 `50.378 ms`（E9 为 `50.266 ms`），尽管 100% 小幅改善到 `84.108 ms` 且片内余量增加约 512 B，仍按 60% 主目标撤回。

原始筛选串口记录：[`edge-invariants-serial.log`](edge-invariants-serial.log)。

## 正式烧录

提交 `39813fa` 后以 `STOPWATCH_BENCHMARK_AUTORUN=OFF`、`STOPWATCH_COLLECT_TOPOLOGY_STATS=ON` 重建并烧录。正式 BIN 为 `0x4c4bb0`（5,000,112 B），分区余 `0x2b450`，SHA-256 `c0acb93eba3b04b839924a424b2650f2ae0cec1d19b91b7eba102f12299e0e9f`。写入 Hash 校验通过；串口确认 `V0.5-317-g39813fa` 进入 Launcher，12 秒内 0 条 benchmark stage result。
