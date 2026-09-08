# Let's & Go!! Racer 使用与真机验收

## 构建与烧录

项目根目录为 `userdemo`，目标为 ESP32-S3。首次构建使用：

```sh
source ../esp-idf/export.sh
idf.py build
idf.py -p <SERIAL_PORT> flash monitor
```

Launcher 中的应用名称为 `Let's & Go!!`，图标是红蓝双星。车型名称、配色和轮廓只按设备所有者私人使用场景保留，不应直接用于公开发行。

## 外设与操作

需要 Joystick2 作为转向轴、Dual Button 作为 Red/Blue 动作输入。进入 App 后先完成连接检查与摇杆校准；比赛中 Joystick2 连续失联 600 ms 会安全减速并自动暂停。Dual Button 是被动 GPIO，软件无法可靠识别实体拔线。

| 操作 | 选择页面 | 比赛中 |
| --- | --- | --- |
| Joystick X | 左右切换，保持后连发 | 连续转向 |
| Blue 短按 | 确认；对手页添加/切换 | 无 |
| Blue 按住 | 无 | Boost |
| Red 短按 | 取消；对手已选时移除 | 刹车 |
| Red 长按 | 由页面返回语义处理 | 暂停/继续 |
| Red + Blue 800 ms | 退出 App | 退出 App |

比赛为自动油门，玩家固定末位发车，完成三圈后进入结算。`RETRY` 使用相同车辆、对手、赛道和随机种子；`GARAGE` 回到选车并清空对手；`EXIT` 返回 Launcher。

## 音乐与音效

R20 新增原创 8-bit BGM 与 14 类音效。车库、比赛、最后一圈、结算使用不同编曲／节奏，暂停停伴奏，恢复接续，退出释放音频。BGM／音效共同遵守进入 App 时读取的系统 SFX 开关与扬声器音量，振动开关独立。见 [R20 实现与试听](Lets-And-Go-Racer-R20-8bit音乐与音效.md)。当前增加音频波形与并发生命周期回归，总计 30 套。

## 集中调参

优先修改 `main/apps/app_lets_and_go_racer/lets_and_go_config.h`：

- 页面时长与 30 FPS 帧间隔。
- 赛道基础高度、半径和 3.8 m 立交抬升。
- 首版 AI 效率与随机波动（R13 已验证四款车均可通过增压与避让翻盘）。
- High/Medium/Low 自动细节降级和恢复阈值。
- 倒计时、GO、Boost、撞墙、最后一圈与冲线的振动参数；音乐／音效参数在 `audio/chip_synth.cpp`。

赛车动力学在 `model/racer_model.cpp`，AI 在 `model/rival_ai.cpp`，摇杆死区与硬件滤波沿用 `app_vector_canyon_fighter/input` 的共享驱动。修改这些参数后必须重新运行 `tools/test_lets_and_go.sh` 和 ESP-IDF 构建。

## 真机验收清单

以下项目在当前无设备环境中均为“待验证”，不得视为已通过：

- [ ] Joystick2 中点、X 轴极性、死区、满量程和 I2C 连续 30 分钟稳定性。
- [ ] Dual Button Red/Blue 极性、短按、长按、组合 800 ms 与返回 Launcher。
- [ ] 四辆车在 466×466 圆屏上的辨识度，文字和 HUD 无边缘裁切。
- [ ] R16 四款实体车的轮罩、座舱、尾翼与轮毂清晰，比赛转弯/上下坡无角度跳变，近距离车辆无方形裁断。
- [ ] R17 Magnum 宽肩收腰、座舱侧通道、独立轮罩在三档均保留，前轮罩不穿胎；选车和比赛材质方向一致（从车后观察文字允许自然倒向）。
- [ ] R18 Sonic 前后连接翼、Neo 中央尖脊/铜色分窗/侧导轮、Brocken 马达区/管架/六辐轮毂在三档均可辨识，转动时附件无裁断或穿胎。
- [ ] 车库 High LOD、四车立交交叉的平均 FPS 不低于 30，压力时不低于 20。
- [ ] 触发细节降级后 HUD、玩家车、双侧护栏仍清晰；负载恢复后画质不抖动。
- [ ] 输入失联自动暂停，恢复后需要玩家主动继续且不会复用旧转向。
- [ ] 音效/振动系统开关生效；连续贴墙不会周期性狂震。
- [ ] 音量 0 无 BGM／音效；暂停、恢复和重赛无重复提示；退出不留循环音乐。
- [ ] 四车桥下连续播放无明显断音，检查小喇叭响度、任务栈水位与系统闹钟优先／结束后恢复音乐。
- [ ] 三圈、名次、最佳圈、同种子重赛与 NVS 断电保存正确。
- [ ] 连续运行 30 分钟无看门狗、明显堆下降或异常温升。
- [ ] 反复进入/退出游戏后 PSRAM 空闲量恢复；约 925 KiB 的打开期曲面缓存分配成功，记录剩余 PSRAM、内部堆和栈水位。
- [ ] 米白全屏的功耗、AMOLED 温升、拖影与可接受亮度。

关闭 App 后串口会输出车库/比赛各自的渲染帧数、峰值耗时、最终细节档和切换次数。真机验收时保存这两行日志，和 `docs/assets/lets-and-go-reference-race.json` 一并归档。

## 桌面复验

第一辆车以 [R17 Magnum 腰线与结构复核](Lets-And-Go-Racer-R17-Magnum腰线与结构复核.md) 为准。剩余三车以 [R18 其余三车结构复核](Lets-And-Go-Racer-R18-其余三车结构复核.md) 为准，最新对比为 `docs/assets/lets-and-go-r18-before.png` 与 `lets-and-go-r18-after.png`，俯视/侧视为 `lets-and-go-r18-structures.png`。R16 图片仅保留为历史。

实体渲染实现背景见 [R16 实体赛车与比赛细化](Lets-And-Go-Racer-R16-实体赛车与比赛细化.md)，车型已由 R17–R18 更新。当前跑道见 [R19 田宫赛道与深色远景](Lets-And-Go-Racer-R19-田宫赛道与深色远景.md)：红蓝白塑料模块、实体外墙、深色远景及同源缩略图；路面、双侧墙和桥底共同参与深度绘制和车辆遮挡。优先测桥下四车、红蓝弯道贴墙、三档画质、暗部可见度，以及退出后的缓存释放；新扫描行绘制的设备 FPS 尚未验证。

```sh
tools/test_lets_and_go.sh
SANITIZE=1 tools/test_lets_and_go.sh
bash tools/render_lets_and_go.sh /tmp/lets-go-review
python3 tools/lets_and_go_contact_sheet.py /tmp/lets-go-review /tmp/lets-go-review/contact.png
```

四款车均为参照实物的手工近似模型，不是官方 CAD 或扫描资产。30 套回归覆盖生产渲染与 384 场策略比赛，包含实体深度、近裁剪、透视 UV 分块一致性、缓存生命周期，以及 R17–R18 的结构/附件间隙和包围盒检查。R19 增加赛道深度绘制顺序、小地图边界与深色背景回归，R20 增加音频波形与并发生命周期回归。桌面量得车库/比赛渲染实例合计 6,216 bytes，打开期渲染缓存合计 1,072,712 bytes；音频另有 76 B 合成器、1,024 B HAL 样本块与少量同步状态。这些不代表真机空闲内存或达标 FPS。

基准生成器是 `tools/lets_and_go_reference_generator.cpp`；固定种子 `0x12345678` 的 JSON 已更新为 v2，包含共同终点的插值冲线时间。静态 SVG 是 R10 历史设计稿，不代表当前生产渲染器。
