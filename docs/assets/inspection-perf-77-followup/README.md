# 77% 后续优化对照（2026-09-10）

本轮固定 77%（271×222），保留 High 几何、程序涂装和 150ms 后原生高清恢复。

- 将每个像素重复执行的 `light / 255.f` 移至三角形外层，保持相同浮点除法与结果。`triangle-before.asm` / `triangle-after.asm` 可见 Xtensa 软件除法调用点由 6 处变为 5 处，内循环中的调用消失。
- 连续欣赏同一车型时复用原 canvas 上的标题、按钮和页脚，清除车辆／阴影／名称区域后重画；页面／车型改变及重开首帧完整绘制，不新增 framebuffer。
- 正常 App 增加最近 64 个已绘制帧的平均绘制、呈现、合计 P95／最大值日志；切换车型或分辨率重置，空闲不记作慢帧。这不是输入端到端延迟。

## 证据与口径

`host-tests.log`：光照提取与统计窗口加入后的完整 ASan/UBSan；`renderer-tests.log`：静态 UI 复用加入后的生产渲染 ASan/UBSan，8 车型 × 80 姿态 × 77%／80%／100% 比较复用与完整绘制；340 张生产图与修改前基线逐字节相同。

`kernel-only-device.log` 为单独光照优化的首轮真机测试；`combined-device.log` 为合并 UI 复用后的真机测试。每轮 8 车型 × 77%／100% × 25 帧，首帧仅预热，交替先后顺序比较相同姿态。每帧比较整个 canvas 的 CRC，实际屏幕全刷／局刷也交替顺序测量。参考实现是本轮修改前的 77% 版本，已包含共享顶点缓存和局部送显。

逐车型 CSV 中 `before_p95`／`after_p95` 是 **绘制阶段** P95。前后平均绘制＋呈现均加同一 `partial_us`，不能把全屏传输当作前版开销重复计算收益。固定姿态对照未运行真人触屏／摇杆采样或比赛音频，不等于实操 FPS、输入延迟或长拖稳定性验收。

## 复现临时固件

将本目录 4 个 `phase3_*` 源文件复制到 `main/apps/app_lets_and_go_racer/view/`；在 HAL 初始化之后、应用安装之前调用 `phase3DeviceBenchmark()`，为 benchmark/reference 两个 `.cpp` 设置与生产 garage 一致的 `-O2 -fno-builtin-memset`，执行 ESP-IDF `reconfigure build`。测试完成后删除临时源与调用、重新构建并恢复正常固件。临时对照需要两个车库 renderer，因此不是常驻内存测量。

设备必须核验为 StopWatch 303A:1001、序列号 44:1B:F6:C1:8A:00（本轮端口 83401），仅写 0x20000 应用分区。PaperColor 83301 不参与。
