# RX-78 Benchmark 60% E6：复用片内工作区生成 pass 索引流

日期：2026-09-24

设备：StopWatch ESP32-S3，`/dev/cu.usbmodem83301`

配置：`STOPWATCH_BENCHMARK_AUTORUN=ON`、`STOPWATCH_COLLECT_TOPOLOGY_STATS=ON`，每次启动连续三轮 100%→60%。

## 判断

直接上下行带命令路径在完成面分类后，旧实现仍按 pass 0 / pass 1 把 2,736 个面完整扫描两遍。E6 复用该路径本帧不再使用的片内 band-index 工作区，单次扫描生成两个稳定索引流，再按原有“pass 0 升序、pass 1 升序”访问实际可见面。面分类、图元顺序、投影、命令格式与光栅均不变；普通命令回退会重新覆盖同一工作区。

| 项目 | E5 | E6 | 变化 |
| --- | ---: | ---: | ---: |
| 60% 三轮平均帧间隔 | 50.948 ms | 50.841 ms | -0.108 ms / -0.21% |
| 60% FPS | 19.63 | 19.67 | +0.21% |
| 60% panel prepare | 6.488–6.496 ms | 6.402–6.420 ms | 平均约 -0.080 ms |
| 100% 三轮平均帧间隔 | 85.672 ms | 85.682 ms | +0.009 ms / 等价 |
| 诊断 BIN | 0x460b90 | 0x460b80 | -16 B |

60% 三轮为 `50.832 / 50.830 / 50.860 ms`，100% 三轮为 `85.693 / 85.683 / 85.669 ms`。六次 RX 会话保持稳定分配与 `fast_path_all=31/7`；60% 最终 internal free 约 23.1 KiB，与 E5 等价。阶段 1–3 保持既有区间。

收益很小，但三轮同向、来源与 panel-prepare 分段吻合，且没有增加持久内存或固件体积，因此保留。通用结论是：当稳定排序键已经计算、且有生命周期互斥的 scratch 时，可用稳定索引流代替对大资产的多 pass 全量重扫；不得因此新增一份常驻工作区，也不得改变顺序。

## 门禁

- `SANITIZE=1 bash tools/test_soft3d.sh`：通过。
- `SANITIZE=1 bash tools/test_gundam_museum_perf.sh`：通过；60/65/90/100 共 1,408 组 framebuffer 逐像素一致。
- ESP-IDF 5.5.4 诊断构建通过；BIN `0x460b80`，分区余 `0x8f480`（11%）。
- 三轮 100%→60% 真机路径稳定，未出现 cache/command allocation miss。

原始筛选串口记录：[`pass-stream-serial.log`](pass-stream-serial.log)。

