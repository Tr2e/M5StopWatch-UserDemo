# RX-78 Benchmark 60% E4：按实际尺寸配置 occupancy

日期：2026-09-24

设备：StopWatch ESP32-S3，`/dev/cu.usbmodem83301`

配置：`STOPWATCH_BENCHMARK_AUTORUN=ON`、`STOPWATCH_COLLECT_TOPOLOGY_STATS=ON`，每次启动连续三轮 100%→60%。

## 判断

60% 只需 `(254×254+7)/8 = 8,065 B` occupancy，旧路径却为可选片内副本申请 22,472 B。按 renderer 打开时已知的采样比例申请实际容量后，释放约 14 KiB，2,112 条 12 B 索引命令块得以稳定进入片内 RAM，并直接生成上/下带有序流。完整 424 occupancy 仍保留为 PSRAM 正确性回退，普通 Museum 默认按 100% 配置。

| 项目 | E3 | E4 | 变化 |
| --- | ---: | ---: | ---: |
| 60% 三轮平均帧间隔 | 52.027 ms | 51.149 ms | -0.878 ms / -1.69% |
| 60% FPS | 19.22 | 19.55 | +1.72% |
| panel prepare | 7.094–7.113 ms | 6.500–6.504 ms | 约 -0.60 ms |
| main raster | 29.579–29.598 ms | 29.320–29.333 ms | 约 -0.26 ms |
| worker raster | 29.594–29.614 ms | 29.412–29.417 ms | 约 -0.19 ms |
| 60% fast path | 7 | 31 | projected + ready + band + command + direct |

三轮 100% 为 `86.601 / 86.601 / 86.639 ms`，路径仍为 7；阶段 1–3 保持既有区间。相对 E3 之前的稳定失配基线 `55.456 ms / 18.03 FPS`，E4 累计降低 4.307 ms（7.77%）。

## 门禁

- `SANITIZE=1 bash tools/test_soft3d.sh`：通过。
- `SANITIZE=1 bash tools/test_gundam_museum_perf.sh`：通过；60/65/90/100 共 1,408 组 framebuffer 逐像素一致。
- ESP-IDF 5.5.4 诊断构建通过；BIN `0x460550`，分区余 `0x8fab0`（11%）。
- 60% 的 6 次 RX 会话均 `commands=true`、`fast_path_all=31`；worker 创建后 internal free 约 24.6 KiB，最大连续块约 7.5 KiB。

原始筛选串口记录：[`sized-occupancy-serial.log`](sized-occupancy-serial.log)。
