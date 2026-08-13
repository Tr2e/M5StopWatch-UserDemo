# Jarvis 3D 线条 UI 与 ESP-NOW 交互计划

## 1. 文档目的

本文档记录 Stopwatch 上 Jarvis 科幻线条 UI 的初步范围、交互方案和实施计划。

当前分支基于：`feat/typhoon`

当前开发分支：`feat/jarvis`

相关硬件结构项目：`/Users/xudanyang/Documents/HUD/`

该 HUD 项目是 StickS3 HUD v4.2 的 OpenSCAD 光学结构和镜匣设计，负责物理光路、镜片安装和支架约束；本文档负责 Stopwatch 固件中的显示和输入交互。两者暂不直接共享代码。

## 2. 目标

先在 Stopwatch 本机完成可调试的 3D 线框界面，再接入 StickC Plus/Joystic 通过 ESP-NOW 发送的视角数据。

目标视觉方向：

- 黑色背景、青色/蓝绿色主线条和少量橙色或红色警示色。
- 中央具有透视感的 3D 核心、环形扫描线和分层 HUD。
- 通过透视投影、线条粗细、亮度衰减、旋转和局部动画产生轻量 Iron Man/Jarvis 风格的立体感。
- 优先保持线条清晰、帧率稳定和圆屏安全区内可读，不追求复杂模型或真实 3D 引擎。

## 3. 当前工程约束

- 设备为 M5Stack StopWatch，显示逻辑尺寸为 466×466，实际可视区域为圆形。
- 工程使用 Mooncake App、LVGL 和 `smooth_ui_toolkit`。
- 现有 App 通过 `main/main.cpp` 的 `installApp()` 注册。
- `main/CMakeLists.txt` 使用 `file(GLOB_RECURSE ...)` 自动收集普通 App 源码和资源。
- A/B 按键由 `input::KeyManager` 统一管理：A 上一项、B 下一项、A+B 长按返回 Launcher。
- 触摸由 LVGL 输入设备提供，触摸回调中不能执行网络、文件或长时间阻塞操作。
- ESP32-S3 已包含 ESP-NOW 能力，但当前 Jarvis 不应立即改变全局 Wi-Fi 初始化流程。
- 当前最小 App 分区约 4.94 MiB，基线固件仍有约 26% 空间；不适合引入大型 3D/AI/语音模型资源。

## 4. 第一阶段功能范围

第一阶段只做本机可交互 Demo，不依赖 StickC Plus：

1. 新建 `app_jarvis` 并注册到 Launcher。
2. 使用 Canvas/M5GFX 绘制线框 UI，保留现有 LVGL App 的生命周期管理。
3. 触摸拖动控制 yaw/pitch 视角。
4. 点击或轻触指定区域切换视角模式。
5. A/B 切换视角或线框场景。
6. A+B 长按返回 Launcher。
7. 显示调试信息：当前 yaw、pitch、roll、zoom、视角模式和输入来源。
8. 预留 RemoteInput 接口，但第一阶段由触摸和按键产生输入。

第一阶段暂不做：

- 语音识别、语音合成和云端 Jarvis 对话。
- 设备端运行大模型。
- 长时间唤醒词监听。
- 复杂模型加载、纹理和大量动画资源。

## 5. 推荐技术方案

### 5.1 App 与渲染

建议目录：

```text
main/apps/app_jarvis/
├── app_jarvis.h/.cpp       # Mooncake 生命周期、输入路由、资源释放
├── model/
│   ├── view_state.h/.cpp   # yaw/pitch/roll/zoom/view_mode
│   └── wire_scene.h/.cpp   # 线框节点和场景定义
├── view/
│   ├── jarvis_renderer.h/.cpp
│   └── jarvis_overlay.h/.cpp
├── input/
│   ├── jarvis_input.h/.cpp # 统一输入事件
│   └── touch_mapper.h/.cpp
└── remote/
    ├── remote_input.h/.cpp # ESP-NOW 接口和数据校验
    └── espnow_protocol.h
```

渲染优先使用 `M5GFX` 的 `LGFX_Sprite`/Canvas：

- 线框绘制不与 LVGL 控件逐条混合。
- Canvas 在 App 打开时接管显示，关闭时恢复 LVGL。
- 视角状态和渲染状态由 App 自己持有，避免使用跨 App 的静态 UI 状态。
- 所有后台数据只通过轻量快照传给渲染主循环。

### 5.2 轻量 3D 线框

第一版只需要点、线段和三角面框：

```text
世界坐标点
    ↓ yaw/pitch/roll 旋转
相机空间
    ↓ 简单透视投影
屏幕坐标
    ↓ 圆形安全区裁剪 + 线条绘制
466×466 Canvas
```

建议初始场景：

- 中央 3D 能量核心：立方体、六边形或多层菱形。
- 外环：两到三层不同速度的圆环/椭圆环。
- 连接线：从核心向环带或边缘 HUD 节点延伸。
- 扫描线：低频旋转或脉冲，不使用大面积填充。
- 状态节点：`JARVIS`、`VIEW 01`、`LINK`、`LOCAL` 等短文本。

圆屏布局建议使用中心 `(233, 233)` 和安全半径约 210–218 px；关键文本和控制提示不贴近四角。

### 5.3 输入抽象

所有输入最终转换为同一种事件：

```cpp
enum class JarvisInputSource : uint8_t {
    Touch,
    Buttons,
    EspNow,
};

struct JarvisViewInput {
    int16_t yaw_delta = 0;
    int16_t pitch_delta = 0;
    int16_t roll_delta = 0;
    int16_t zoom_delta = 0;
    uint8_t buttons = 0;
    uint8_t view_mode = 0;
    uint16_t sequence = 0;
    JarvisInputSource source = JarvisInputSource::Touch;
};
```

触摸阶段：

- 手指水平位移映射到 yaw。
- 手指垂直位移映射到 pitch。
- 点击中心或菜单区域触发视角切换。
- 释放触摸后停止增量，不产生漂移。

后续 ESP-NOW 阶段只替换输入来源，不改变 ViewState 或 Renderer。

## 6. ESP-NOW 接口预留

建议先实现协议解析和本机模拟入口，暂不要求硬件发送端立即完成。

推荐使用固定小端二进制包：

```text
magic       2 bytes
version     1 byte
type        1 byte
sequence    2 bytes
yaw_delta   2 bytes
pitch_delta 2 bytes
roll_delta  2 bytes
zoom_delta  2 bytes
buttons     1 byte
view_mode   1 byte
crc16       2 bytes
```

接收端必须具备：

- magic、版本、长度和 CRC 校验。
- sequence 去重。
- 过期包丢弃。
- 最后一次有效数据超时后停止继续施加旋转增量。
- ESP-NOW 回调只复制数据或投递队列，不直接操作 LVGL/Canvas。
- App 退出时断开接收回调或停止对应 worker，避免影响其它 App。

初期可将 `remote_input` 设计为：

```cpp
class JarvisRemoteInput {
public:
    bool begin(uint8_t channel);
    void stop();
    bool poll(JarvisViewInput& output);
};
```

实际 ESP-NOW 初始化方式需要结合 Stopwatch 当前 `WifiService` 的信道和连接状态验证，不能直接复制其它项目中会重复初始化 Wi-Fi 的实现。

## 7. StickC Plus / JoyC 联调阶段

本机触控交互稳定后，再制作 StickC Plus 发送端：

1. JoyC 读取摇杆轴值和按键状态。
2. 通过死区、低通滤波和最大速率限制减少视角抖动。
3. 摇杆 X/Y 发送 yaw/pitch 增量。
4. 额外按键发送视角切换、重置或 zoom 命令。
5. 以固定周期发送数据，并递增 sequence。
6. Stopwatch 端显示 `REMOTE`、RSSI/最近接收时间等调试信息。

联调顺序：

- 先发送固定测试包，验证协议解析。
- 再发送摇杆原始值，验证方向和死区。
- 再接入视角速度曲线。
- 最后加入视角切换和断链恢复。

## 8. 分阶段实施计划

### Phase 0：文档与接口冻结

- 当前文档进入 `feat/jarvis`。
- 确认本机触摸手势、视角模式和颜色方向。
- 冻结 `JarvisViewInput`、`ViewState` 和 ESP-NOW 包格式。

### Phase 1：本机线框 Demo

- 创建 `app_jarvis` 骨架和 Launcher 图标。
- 实现 Canvas 生命周期切换。
- 实现透视投影、旋转矩阵和圆屏安全区。
- 绘制中心核心、环带、扫描线和最小 HUD。

### Phase 2：触摸与按键交互

- 加入触摸拖动 yaw/pitch。
- 加入点击切换视角。
- 加入 A/B 和 A+B 返回约定。
- 加入本机调试 overlay。

### Phase 3：RemoteInput 本地模拟

- 实现协议编码/解码、CRC、sequence 和超时。
- 增加本机模拟数据入口。
- 在不接硬件的情况下验证远程输入不会破坏触摸交互。

### Phase 4：ESP-NOW 接收端

- 在 Stopwatch 中接入 ESP-NOW 接收 worker/队列。
- 确认 Wi-Fi 信道、初始化时机和退出清理。
- 验证固定包、错误包、重复包和断链。

### Phase 5：StickC Plus 发送端

- 读取 JoyC。
- 实现死区、滤波、速率限制和按键映射。
- 进行方向、延迟、丢包和断链测试。

## 9. 验收标准

本机交互完成的最低标准：

- App 可从 Launcher 打开和关闭。
- 触摸拖动视角方向正确，停止拖动后不漂移。
- 点击可切换至少两个视角。
- A/B 可执行同等的视角切换操作。
- A+B 长按能稳定回到 Launcher。
- 线框在圆屏安全区内完整显示，边缘无关键文字裁切。
- 运行期间无明显闪烁、卡顿或 LVGL/Canvas 冲突。
- 连续进入/退出 App 多次后无残留任务或崩溃。

ESP-NOW 联调完成的最低标准：

- 固定测试包可正确改变 yaw/pitch。
- 重复包不会重复累加。
- 错误包被拒绝。
- 发送端停止后视角不会持续漂移。
- 返回 Launcher 后 ESP-NOW 不影响其它 App。

## 10. 构建与验证

构建命令：

```bash
cd /Users/xudanyang/Documents/stopwatch/userdemo
source ../esp-idf/export.sh && idf.py build
```

烧录命令：

```bash
source ../esp-idf/export.sh && idf.py -p /dev/cu.usbmodem101 flash
```

第一阶段只要求构建和本机/模拟输入验证；StickC Plus 联调完成后再进行实际烧录测试。
