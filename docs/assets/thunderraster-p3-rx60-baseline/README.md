# ThunderRaster P3 RX-78 60% 快速基线

日期：2026-09-24

设备：StopWatch ESP32-S3 rev 0.2，`/dev/cu.usbmodem83301`

分支：`perf/thunderraster-rx60-p3`

配置：`STOPWATCH_BENCHMARK_AUTORUN=ON`、`STOPWATCH_BENCHMARK_RX60_ONLY=ON`、`STOPWATCH_COLLECT_TOPOLOGY_STATS=ON`。启动后直接进入 RX-78，只测 60%，连续三轮各 6.8 s。

## 结论

| 项目 | pass 0 | pass 1 | pass 2 | 三轮均值 |
| --- | ---: | ---: | ---: | ---: |
| 帧间隔 | 50.277 ms | 50.309 ms | 50.316 ms | 50.301 ms |
| FPS | 19.890 | 19.877 | 19.875 | 19.880 |
| framebuffer clear | 4.419 ms | 4.430 ms | 4.431 ms | 4.427 ms |
| raster begin | 2.047 ms | 2.050 ms | 2.054 ms | 2.050 ms |
| render | 41.823 ms | 41.837 ms | 41.832 ms | 41.831 ms |
| blit | 5.298 ms | 5.297 ms | 5.303 ms | 5.299 ms |
| panel prepare | 6.398 ms | 6.412 ms | 6.396 ms | 6.402 ms |
| main raster | 28.829 ms | 28.815 ms | 28.809 ms | 28.818 ms |
| worker raster | 28.538 ms | 28.550 ms | 28.552 ms | 28.547 ms |

E9 合并基线为 `50.266 ms / 19.894 FPS`。本分支零点慢 `0.035 ms / 0.069%`，没有形成可辨识的系统性退化。三轮均为 130 帧、129 个采样间隔，`fast_path_all=31`；每轮总量 `TRI 4650 / QUAD 1914`，平均实际提交约 `TRI 3202–3203 / QUAD 1283`。

串口确认只出现 `stage=3 name=04 RX-78 FINAL`，未运行阶段 1–3 和 100% 回合。筛选后的原始记录见 [`rx60-baseline.log`](rx60-baseline.log)。

## 固件与门禁

- 诊断 BIN：`0x460da0`（4,591,008 B），应用分区余 `0x8f260`（11%）。
- BIN SHA-256：`7cf0669e9c6569e503ac120506328dbea87eceea830b9890e26b81f8d6d95db0`。
- `SANITIZE=1 bash tools/test_soft3d.sh`：通过。
- `SANITIZE=1 bash tools/test_gundam_museum_perf.sh`：通过；60/65/90/100 framebuffer 共 1,408 组一致，并覆盖严格 RGB565、Q13 depth、occupancy 门禁。
- 只写入 app 分区；写后 hash 校验通过。

后续 P3 实验必须以本记录为同分支 A/B 零点，同时继续引用 E9 作为跨分支长期基线。
