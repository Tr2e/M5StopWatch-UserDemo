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

## 集中调参

优先修改 `main/apps/app_lets_and_go_racer/lets_and_go_config.h`：

- 页面时长与 30 FPS 帧间隔。
- 赛道基础高度、半径和 3.8 m 立交抬升。
- 首版 AI 效率与随机波动（R13 已验证四款车均可通过增压与避让翻盘）。
- High/Medium/Low 自动细节降级和恢复阈值。
- 倒计时、GO、Boost、撞墙、最后一圈与冲线的音调/振动参数。

赛车动力学在 `model/racer_model.cpp`，AI 在 `model/rival_ai.cpp`，摇杆死区与硬件滤波沿用 `app_vector_canyon_fighter/input` 的共享驱动。修改这些参数后必须重新运行 `tools/test_lets_and_go.sh` 和 ESP-IDF 构建。

## 真机验收清单

以下项目在当前无设备环境中均为“待验证”，不得视为已通过：

- [ ] Joystick2 中点、X 轴极性、死区、满量程和 I2C 连续 30 分钟稳定性。
- [ ] Dual Button Red/Blue 极性、短按、长按、组合 800 ms 与返回 Launcher。
- [ ] 四辆车在 466×466 圆屏上的辨识度，文字和 HUD 无边缘裁切。
- [ ] 车库 High LOD、四车立交交叉的平均 FPS 不低于 30，压力时不低于 20。
- [ ] 触发细节降级后 HUD、玩家车、双侧护栏仍清晰；负载恢复后画质不抖动。
- [ ] 输入失联自动暂停，恢复后需要玩家主动继续且不会复用旧转向。
- [ ] 音效/振动系统开关生效；连续贴墙不会周期性狂震。
- [ ] 三圈、名次、最佳圈、同种子重赛与 NVS 断电保存正确。
- [ ] 连续运行 30 分钟无看门狗、明显堆下降或异常温升。
- [ ] 米白全屏的功耗、AMOLED 温升、拖影与可接受亮度。

关闭 App 后串口会输出车库/比赛各自的渲染帧数、峰值耗时、最终细节档和切换次数。真机验收时保存这两行日志，和 `docs/assets/lets-and-go-reference-race.json` 一并归档。

## 桌面复验

最新跑道视觉见 [R15 跑道视觉优化](Lets-And-Go-Racer-R15-跑道视觉优化.md) 和 `docs/assets/lets-and-go-r15-after.png`。桥底与侧面参与遮挡，比赛缓存比 R14 增加 18,440 bytes；真机优先测桥下四车场景的帧耗时、空闲堆与栈水位。

```sh
tools/test_lets_and_go.sh
SANITIZE=1 tools/test_lets_and_go.sh
bash tools/render_lets_and_go.sh /tmp/lets-go-review
python3 tools/lets_and_go_contact_sheet.py /tmp/lets-go-review /tmp/lets-go-review/contact.png
```

当前四车外形和官方实物对照见 [R14 实车模型重建](Lets-And-Go-Racer-R14-实车模型重建.md)；道路视觉见 R12，可玩性见 R13。28 套回归继续覆盖生产渲染与 384 场策略比赛。最新对比图为 `docs/assets/lets-and-go-r14-before.png` 与 `lets-and-go-r14-after.png`。R14 增加约 40 KB 渲染实例缓存，真机需重点复验空闲堆与三档帧耗时。

基准生成器是 `tools/lets_and_go_reference_generator.cpp`；固定种子 `0x12345678` 的 JSON 已更新为 v2，包含共同终点的插值冲线时间。静态 SVG 是 R10 历史设计稿，不代表当前生产渲染器。
