# ESP32-S3 比赛渲染受控对照

主报告：`../../ESP32-S3-贴图与软件渲染优化实证方案.md`。

- `race_device_benchmark.cpp`：同一 renderer 轮换四种组合，8 车型 × 2 赛道 × 25 固定位置，去掉首帧后每组 24 帧，包含全屏传输。
- `baseline/`：本轮开始前关键源码；不参与正常编译。
- `device.log`、`device.csv`、`device.json`：原始日志、逐车型逐赛道逐方案统计、正确加权的总均值。`python3 summarize.py device.log` 可重新生成统计；单组 P95 在 CSV，不平均为全局 P95。
- `uv-host-tests.log`：实验阶段生产 renderer ASan/UBSan 和 32 个比赛状态的 UV 像素对照。
- `frame-comparison.json`：对前一版 356 张生产场景的对照。
- `host-tests.log`、`target-build.log`：最终完整回归与正常固件构建。
- `firmware.json`、`normal-flash.log`、`normal-boot.log`：最终固件、写入和启动核验。

复现：将临时 benchmark 源复制至生产 view 目录，在 HAL 初始化后、App 安装前调用 `raceDeviceBenchmark()`，给该文件设置 `-O2 -fno-builtin-memset`，执行 ESP-IDF 重新配置和构建。复现四组合时，还需使用 `experimental/` 保存的 renderer 与 raster 源，它们包含 UV 实验开关；正式固件移除了 UV 分支，只保留复制开关用于确定性对照／兼容回退。用户界面不提供这些实现选项。测试完移除入口和源文件、重新构建，确认正常 ELF 没有 `raceDeviceBenchmark` 符号。

复制快路径逐组检查整屏 CRC；UV 变换检查变化帧与像素数。背景与 HUD 保持原样，玩家原生分辨率不降低。保存的 PSRAM 整屏比较缓冲只用于临时测试，计时之外的检查仍可能影响缓存，轮换顺序只能降低这种偏差。正常固件没有新增 framebuffer。

设备仅核验为 303A:1001、44:1B:F6:C1:8A:00 的 StopWatch 83401，写入 0x20000 应用分区；PaperColor 83301 不参与。若重新连接后枚举改变，重新确认身份，不硬猜端口。
