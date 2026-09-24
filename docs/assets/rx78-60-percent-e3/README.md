# RX-78 Benchmark 60% E3：精确 SoA 投影缓存

日期：2026-09-24

设备：StopWatch ESP32-S3，`/dev/cu.usbmodem83301`

配置：`STOPWATCH_BENCHMARK_AUTORUN=ON`、`STOPWATCH_COLLECT_TOPOLOGY_STATS=ON`，每次启动连续三轮 100%→60%。

## 判断

RX-78 的 2,674 个投影点需要 32,088 B，稳定片内最大连续块只有 31,744 B。单块 AoS 会随会话堆形态随机命中；双块 AoS 虽稳定，但每次取点的分段判断拖慢热光栅。最终方案保留精确 float，把 x/y/z 拆成三个同长度片内数组，三块全部成功才启用，失败时整体释放并走原回退。

| 方案 | 60% 三轮平均帧间隔 | 60% FPS | 100% 三轮平均帧间隔 | 100% FPS | 投影缓存命中 |
| --- | ---: | ---: | ---: | ---: | --- |
| 单块 AoS 稳定失配基线 | 55.456 ms | 18.03 | 90.999 ms | 10.99 | 后两轮失配 |
| 双块 AoS | 52.769 ms | 18.95 | 87.304 ms | 11.45 | 6/6 |
| 三数组 SoA | 52.170 ms | 19.17 | 86.728 ms | 11.53 | 6/6 |
| SoA + 行带只读 y（接受） | 52.027 ms | 19.22 | 86.614 ms | 11.55 | 6/6 |

最终 SoA 相对稳定失配基线：60% 减少 3.429 ms（6.18%），100% 减少约 4.385 ms（4.82%）。相对双块 AoS，60% 减少约 0.742 ms。三轮最终候选的 RX 分解稳定在 panel prepare `7.094–7.113 ms`、main raster `29.579–29.598 ms`、worker raster `29.594–29.614 ms`。

## 正确性与构建

- `SANITIZE=1 bash tools/test_soft3d.sh`：通过。
- `SANITIZE=1 bash tools/test_gundam_museum_perf.sh`：通过；60/65/90/100 共 1,408 组 framebuffer 逐像素一致。
- ESP-IDF 5.5.4 诊断构建通过；最终诊断 BIN 大小 `0x4604e0`，最小应用分区余 `0x8fb20`（11%）。
- 阶段 1–3 保持约：100% `44.7 / 28.5 / 36.5 FPS`，60% `72.8 / 51.3–51.4 / 60.8 FPS`。

原始筛选串口记录：[`segmented-cache-serial.log`](segmented-cache-serial.log)、[`soa-cache-serial.log`](soa-cache-serial.log)、[`soa-yonly-serial.log`](soa-yonly-serial.log)。日志保留每轮结果、阶段分解、RX 分解、内存路径与完成标记；启动期外设噪声已过滤。
