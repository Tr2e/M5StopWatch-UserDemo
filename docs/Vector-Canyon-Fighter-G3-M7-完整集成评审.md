# Vector Canyon Fighter G3 · M7 完整集成评审

> 状态：Review 通过，用户已确认
>
> 日期：2026-09-02
>
> Feature flags：`EXPLICIT_TERRAIN=1`，`EXPLICIT_PREVIEW=0`，`EXPLICIT_EVENT_STREAM=0`，`EXPLICIT_TOP_DEBUG=0`

## 1. 本阶段边界

M7 把 M2–M6 已通过的显式峡谷恢复到完整游戏链路：

- 恢复 IMU 校准、表体倾斜输入和 A/B 按键；
- 恢复 FlightModel、线框战机、HUD 和完整配色；
- 碰撞直接消费显式峡谷的左、右独立边界；
- 前视边界只触发预警，只有当前位置越界才碰撞；
- 相机保持在峡谷路线中心，横移只作用于战机，避免地形和战机重复偏移；
- 输入创建收口到 `InputProvider` 工厂，未来 Joystick2 / Dual Button 只需替换 provider，不修改地形、飞行或碰撞层。

## 2. 实现 Review

- [x] 正常游戏路径使用 `ExplicitCanyonStream`，不再进入孤立预览日程；
- [x] Renderer 的 legacy / explicit 入口共用战机、HUD、校准页和告警绘制，无两份游戏 UI 逻辑；
- [x] explicit 消失点由远端 `FloorCenter` 投影得到，HUD 与弯道视线保持一致；
- [x] `CollisionModel` 同时输出 left / right / floor / warning clearance；
- [x] 左碰撞只取决于 `leftWidth + lateralOffset`，右碰撞只取决于 `rightWidth - lateralOffset`；
- [x] 4.8 世界单位、4 点前视只参与 warning，不提前锁死战机；
- [x] 中性高度 `0.5` 不会产生永久警报，低于舰体离地门槛才判定谷底碰撞；
- [x] explicit camera 只读 route frame，不读 `lateralOffset`；
- [x] 战机屏幕横移继续只读 `lateralOffset`；
- [x] 通用 `InputProvider -> FlightInput -> FlightModel` 合约未更改；
- [x] 新增 provider 选择点不需要 CMake 手动列源文件；
- [x] 每 5 秒记录完整游戏 FPS、render avg/max、boost、simulation clamp 和任务栈水位；
- [x] 碰撞日志显示世界 S、横移、左右宽度、左右净空、谷底净空和预警净空。

首轮真机日志发现两个诊断缺陷：碰撞锁定后每次主循环都输出一行 `<COLL>`，以及首个性能窗口包含 2.5 秒校准阶段。第二轮候选已改为碰撞上升沿立即记录、其后按固定周期记录，并在校准完成时重置性能窗口。首轮固件不允许通过。

第二轮日志确认上述两项修复有效，但完整游戏仍只有 `18.8…19.1 FPS`；render 稳定在 `42 ms`，剩余约 `10 ms` 位于同步 IMU I2C 读取。第三轮候选把 StopWatch IMU 采样收口到 provider 内的 Core 1 固定 30 Hz 任务，主游戏循环只消费最新加速度快照；任务创建失败时明确回退到原同步采样。该改动不改变 `InputProvider` 接口，未来 Grove provider 不受影响。

第三轮把性能提高到 `21.8…22.2 FPS`，但 IMU 任务栈水位在运行后降至 `16 B`，存在明确溢出风险，因此继续否决。第四轮把该任务栈提高到 5 KB，并在地形 draw call 前增加同侧屏外线段的严格 trivial reject：只剔除两端同时位于屏幕同一边外、数学上不可能穿过画布的直线，不删除任何屏内网格或改变 LOD。

第四轮连续运行后 IMU 任务栈水位稳定在约 `2040 B`，栈风险已消除。移除临时诊断后，最终候选连续运行实测 IMU 任务最低余量 `2036 B`、主任务最低余量 `4612 B`。临时分段计时在多个 120 帧窗口内得到：

| 完整帧阶段 | 平均耗时 |
| --- | ---: |
| 黑色整屏 framebuffer 清空 | `12.86…12.99 ms` |
| 显式峡谷投影与绘制 | `13.55…14.41 ms` |
| 线框战机与尾焰 | `1.12…1.14 ms` |
| 完整 HUD | `3.87…3.99 ms` |
| framebuffer 提交到 AMOLED | `12.20…12.36 ms` |

约 25 ms、57% 的帧时间是当前 M5GFX / AMOLED 整屏 framebuffer 的必需清空与提交，地形只占约 14 ms。M0 中 `>=24 FPS` 是 M6 纯地形性能门槛，已以 `25.5…26.4 FPS` 通过；M7 完整负载的审核结果为 `21.6…22.3 FPS`、无持续 clamp，不为追求数字删除已确认的峡谷密度、战机或 HUD。临时分段计时代码在最终固件中移除。

## 3. Host 测试与回归

最终真机确认后重新构建并运行 M7 专项以及 M2–M6 全部 host 回归：6 组测试均通过 `-Wall -Wextra -Werror -pedantic` 严格编译与运行；随后以 AddressSanitizer + UndefinedBehaviorSanitizer 重新编译、运行，6 组退出码均为 0，未报告越界或未定义行为。

| 检查 | 结果 |
| --- | ---: |
| 左侧事件宽度 L / R | `1.20214 / 2.18` |
| 右侧事件宽度 L / R | `2.18 / 1.18013` |
| 前视位置当前净空 | `0.3`，未碰撞 |
| 前视预警最小净空 | `-0.339802`，已预警 |
| 左右独立碰撞 | 通过 |
| 谷底碰撞 | 通过 |
| 中性高度无永久预警 | 通过 |
| 输入 provider 多态合约 | 通过 |
| 战机横移不修改地形缓存 | 通过 |
| 显式相机不重复应用横移 | 通过 |
| M2 固定种子事件哈希 | `17445385684075537385`，未变 |
| M6 流式最大 slice 间距误差 | `0.00182676`，未变 |

## 4. 构建与烧录

- [x] ESP32-S3 编译、链接和分区检查通过；
- [x] 新增 `explicit_canyon_collision.cpp` 已出现在目标 `compile_commands.json`；
- 固件大小：`0x465630` B；
- 分区余量：`0x8a9d0` B（11%）；
- BIN SHA-256：`4f5d11fc5bf2548176c68623ac33436a6eb4b18ee0f8824ac5090df8a22fc008`；
- [x] bootloader、app、partition table 和 OTA data 全部写入并通过 Hash 校验；
- [x] 烧录后硬复位，系统启动、PSRAM 检查和 IMU 初始化正常；
- [x] 已采集完整游戏性能、IMU 栈、输入、预警、碰撞、重开及应用重入日志。

## 5. 真机 Review Checklist

- [x] 进入 Vector Run 后显示水平校准页，约 2.5 秒后自动进入游戏；
- [x] 显式峡谷、线框战机和完整 HUD 同时正常显示；
- [x] 左右倾斜时战机横移并横滚，峡谷不随战机做第二次偏移；
- [x] 前后倾斜可改变俯仰与高度；
- [x] A 短按降低油门，B 短按提高油门，B 长按 boost 有响应；
- [x] 左侧悬崖突出只收紧左侧可行空间，右侧同理；
- [x] 接近一侧时先有警告，越过当前边界时才碰撞；
- [x] 碰撞后战机停止，长按 A 返回校准并可重开；
- [x] 连续运行超过 60 秒，无异常长线、闪白、卡死或重启；
- [x] 完整负载 FPS 稳定于实测 `21.6…22.5`；simulation clamp 始终为 0；
- [x] 最终候选 render max 为 `46…49 ms`，无异常长帧；
- [x] IMU 任务最低栈余量 `2036 B`、主任务最低余量 `4612 B`，无递减趋势；
- [x] 系统返回可退出应用；日志确认 provider 关闭后返回 Launcher；
- [x] 退出后重新进入 Vector Run，异步 IMU 任务成功重建，校准、画面和操控正常；
- [x] 用户于 2026-09-02 确认最终候选视觉与运行正常。

## 6. 提交门禁

M7 提交门禁已满足：严格 host 回归与 sanitizer、目标构建和烧录、超过 60 秒真机运行、输入与双侧碰撞、重开、退出/重入生命周期均通过；用户已确认最终画面与运行正常，可以提交。
