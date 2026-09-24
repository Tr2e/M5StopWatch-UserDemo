# RX-78 Benchmark 60% E8：扫描行目标指针特化

日期：2026-09-24

设备：StopWatch ESP32-S3，`/dev/cu.usbmodem83301`

配置：`STOPWATCH_BENCHMARK_AUTORUN=ON`、`STOPWATCH_COLLECT_TOPOLOGY_STATS=ON`，每次启动连续三轮 100%→60%。

## 判断

solid-quad 旧热循环每个像素都从 target 结构解析 depth/color 基址并重复计算行与 split-band offset。E8 在扫描行入口一次生成不别名的 depth/color 行指针，像素循环只保留局部 x 和全局 occupancy index；重心、深度、覆盖、比较和写入顺序完全不变。

| 项目 | E6 | E8 | 变化 |
| --- | ---: | ---: | ---: |
| 60% 三轮平均帧间隔 | 50.841 ms | 50.705 ms | -0.136 ms / -0.27% |
| 60% FPS | 19.67 | 19.72 | +0.27% |
| 100% 三轮平均帧间隔 | 85.682 ms | 85.326 ms | -0.356 ms / -0.42% |
| 60% main raster | 29.215–29.239 ms | 29.142–29.153 ms | 约 -0.08 ms |
| 60% worker raster | 29.130–29.163 ms | 28.957–28.987 ms | 约 -0.17 ms |
| 诊断 BIN | 0x460b80 | 0x460b20 | -96 B |

60% 三轮为 `50.730 / 50.677 / 50.707 ms`，100% 三轮为 `85.337 / 85.295 / 85.345 ms`。六次 RX 会话保持稳定分配与 `fast_path_all=31/7`；片内余量与 E6 等价。

## 门禁

- `SANITIZE=1 bash tools/test_soft3d.sh`：通过。
- `SANITIZE=1 bash tools/test_gundam_museum_perf.sh`：通过；60/65/90/100 共 1,408 组最终 framebuffer 逐像素一致。
- 新增 256 组确定性 skew-quad：旧三角路径与 quad 特化的 RGB565、Q13 depth 和 occupancy 全部逐点一致。
- ESP-IDF 5.5.4 诊断构建通过；BIN `0x460b20`，分区余 `0x8f4e0`（11%）。
- 三轮 100%→60% 真机路径稳定，未出现 cache/command allocation miss。

原始筛选串口记录：[`row-targets-serial.log`](row-targets-serial.log)。

