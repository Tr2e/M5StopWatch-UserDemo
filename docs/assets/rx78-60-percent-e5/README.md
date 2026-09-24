# RX-78 Benchmark 60% E5：按扫描行合并 occupancy 写入

日期：2026-09-24

设备：StopWatch ESP32-S3，`/dev/cu.usbmodem83301`

配置：`STOPWATCH_BENCHMARK_AUTORUN=ON`、`STOPWATCH_COLLECT_TOPOLOGY_STATS=ON`，每次启动连续三轮 100%→60%。

## 判断

RX-78 的 prepared solid quad 会让两个子三角形逐像素更新同一份 occupancy。旧路径对每个通过深度测试的像素执行一次位图读改写；候选在单条扫描行内保留当前 occupancy byte，把同一 byte 的多个 bit（包括两个子三角形）合并后再写回。颜色、深度、覆盖规则和子三角形顺序不变；只批处理辅助元数据写入。

| 项目 | E4 | E5 | 变化 |
| --- | ---: | ---: | ---: |
| 60% 三轮平均帧间隔 | 51.149 ms | 50.948 ms | -0.201 ms / -0.39% |
| 60% FPS | 19.55 | 19.63 | +0.40% |
| 100% 三轮平均帧间隔 | 86.614 ms | 85.672 ms | -0.941 ms / -1.09% |
| 60% main raster | 29.320–29.333 ms | 29.208–29.228 ms | 约 -0.11 ms |
| 60% worker raster | 29.412–29.417 ms | 29.124–29.151 ms | 约 -0.28 ms |
| 60% fast path | 31 | 31 | 不变 |

60% 三轮为 `50.947 / 50.933 / 50.965 ms`，100% 三轮为 `85.654 / 85.693 / 85.670 ms`。六次 RX 会话的投影、occupancy 和命令路径均稳定命中；60% 保持 `commands=true`、`fast_path_all=31`，100% 保持 `fast_path_all=7`。阶段 1–3 保持既有区间。

该优化增加约 1.5 KiB 片内代码占用，使 60% worker 创建后的 internal free 从约 24.6 KiB 降至 23.1 KiB，最小观测最大连续块为 7.5 KiB。收益可重复但不大，因此结论是“在 prepared sparse solid quad 的高像素覆盖路径中保留”；不能无条件复制到简单图元或片内预算更紧张的固件，必须同时签署端到端收益和堆余量。

## 门禁

- `SANITIZE=1 bash tools/test_soft3d.sh`：通过。
- `SANITIZE=1 bash tools/test_gundam_museum_perf.sh`：通过；60/65/90/100 共 1,408 组 framebuffer 逐像素一致。
- ESP-IDF 5.5.4 诊断构建通过；BIN `0x460b90`，分区余 `0x8f470`（11%）。
- 三轮 100%→60% 真机路径稳定，未出现 cache/command allocation miss。

原始筛选串口记录：[`occupancy-row-cache-serial.log`](occupancy-row-cache-serial.log)。

