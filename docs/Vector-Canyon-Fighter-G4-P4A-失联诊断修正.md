# Vector Canyon Fighter：G4-P4A 失联诊断修正

> 日期：2026-09-04  
> 范围：修正外设静态适配评审后发现的异常状态提示，不启用外设。

## 问题

App 在等待输入就绪时统一进入校准画面。若 Joystick2 未响应，Provider 正确返回 `Disconnected/Fault`，但旧画面仍显示 `CALIBRATING` 和倒计时，会把失联错误描述成正在校准。

## 修正

- `Disconnected/Fault` 显示 `INPUT OFFLINE`。
- Joystick2 路径明确显示 `CHECK PORT.A / ADDR 0x63`；其他轴源显示 `SENSOR NOT READY`。
- 失联时不绘制虚假的校准进度或倒计时，并保留 K1+K2 退出提示。
- `Calibrating` 状态继续使用既有居中布局、进度条和静止提示。

## 门禁

- [x] 仅改变输入失联/故障画面；正常游戏、姿态、地形和 HUD 不变。
- [x] 不改变 Provider 状态，不用 UI 文案掩盖硬件错误。
- [x] 默认外设开关保持关闭。
- [x] ESP-IDF 完整构建通过。

## 结论

通过。诊断反馈与输入状态契约重新一致，可以进入实体线束电气门禁。
