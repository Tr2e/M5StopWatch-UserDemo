# Vector Canyon Fighter：G4-H6 暂停交互闭环评审

> 日期：2026-09-05
>
> 范围：让既有 Pause 契约、应用状态和 HUD 形成可达闭环。地形、战机、相机、飞行参数、碰撞和 H1–H5 HUD 布局冻结。

## 审计结论

`FlightAction::Pause`、`FlightModel::togglePaused()` 和暂停 HUD 已经存在，但表身按键与 Dual Button 都不会产生 Pause 动作，导致暂停状态只在测试中可达。G1 规定的“暂停、冻结背景、继续”因此尚未形成真实交互闭环。

## 交互决策

- K1 单击继续降低巡航速度，K2 单击继续提高巡航速度；K2 按住仍为 Boost。
- K1 长按继续按上下文切换沉浸模式或撞击后重置。
- K1+K2 短按切换 Pause/Resume；动作在第二枚按键按下时立即锁存，不依赖两枚按键恰好同时释放。
- 识别为双键 chord 后，两枚按键随后错开的释放事件均被吞掉，禁止意外改变巡航速度。
- K1+K2 长按仍由系统 `KeyManager` 返回 Launcher；不得被应用重定义。
- 表身按键和未来 Dual Button 共用同一个 `TwoButtonFlightActionMapper`，保证接入外设后无需修改 App 或 FlightModel。

## 视觉决策

- 暂停后保留冻结的峡谷、战机和外围仪表作为上下文。
- 中央状态仍使用 H5 的低亮角框，不使用 caution/impact 色。
- 主状态为 `PAUSED`，下方增加低亮 `K1+K2 TAP`，只解释恢复动作，不添加新装饰。
- 暂停层优先于普通 terrain warning；解除暂停后恢复实时风险显示。

## 验收门禁

- [x] 单击 K1/K2 仍分别只产生 ThrottleDown/ThrottleUp。
- [x] 双键先后按下只产生一次 Pause，不要求同帧按下或同帧释放。
- [x] chord 后错开的两个 click 不泄漏为油门变化。
- [x] K1 长按的 Reset/ToggleImmersive 和 K2 held Boost 语义未改变。
- [x] IMU + 表身按键与 Joystick2 + Dual Button 使用同一动作映射器。
- [x] 输入契约、外设去抖、组合输入、FlightModel 与 HUD 五组主机测试零警告通过。
- [x] ESP-IDF 5.5.4 完整构建通过；固件 `0x47bf40` B，分区余量 `0x740c0` B（9%）。
- [x] 固件 SHA-256：`284922c9273d6262b36889da329410aabc557332e51d09b992715ab36a454a28`。
- [x] 固件四个分区烧录并通过 Hash 校验，RTS 硬复位完成。
- [x] 2026-09-05 候选固件进入真机验证后，用户未报告暂停或既有按键冲突并继续提出下一独立速度切片；H6 阶段接受。

## 退出条件

Pause 在当前表身按键和未来 Dual Button 上具有相同、可靠且不会误改油门的入口；玩家能够从冻结画面直接读出恢复动作，系统退出手势保持不变。
