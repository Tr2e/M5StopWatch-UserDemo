# Vector Canyon Fighter：G3-P0 通用输入契约评审

> 日期：2026-09-04  
> 阶段目标：在优化 IMU 姿态之前，先冻结可直接承接 Joystick2 + Dual Button 的输入边界。  
> 非目标：本阶段不调整 IMU 灵敏度、飞行姿态、战机几何、相机、碰撞或配色。

## 1. 问题与决策

旧 `InputProvider` 已经隔离了大部分硬件访问，但仍以 IMU 为中心：所有 Provider 被迫暴露校准方法；连续轴、Boost 按住和暂停边沿混在同一组布尔字段；K1 沉浸模式由 App 直接读取 HAL；轴设备与按键设备也没有独立的来源和健康状态。

P0 将契约拆成三部分：

```text
FlightInput   = 规范化连续轴 + 动作快照 + 序列号
FlightActions = pressed 一次性边沿 + held 持续状态
InputStatus   = 轴来源 + 动作来源 + readiness + 连接/错误/校准状态
```

Joystick2 和 Dual Button 因此不再被假定为一个不可拆分的设备。未来可以组合 Joystick2 + Dual Button，也可以在单个设备失联时使用 Joystick2 + 表身按键或完整 IMU 降级。

## 2. 层级边界

- 硬件 Provider：允许读取 IMU、表身按键、I2C 或 GPIO，负责设备特有的校准、死区、曲线、去抖和健康检查。
- 应用控制层：在固定模拟步外消费一次性动作；决定暂停、重置、沉浸与重新校准。
- FlightModel：只读取规范化轴、油门和持续 Boost，不读取 HAL，也不消费按键边沿。
- Renderer：只读取 FlightState、InputStatus 和视觉显隐状态；不探测硬件。

系统级双键退出仍由公共 `KeyManager` 处理，不属于游戏操纵设备。

## 3. 已实现契约

- `FlightAxisSource`：`None / Imu / Joystick2`。
- `FlightActionSource`：`None / BodyButtons / DualButton`。
- `InputReadiness`：`Disconnected / Calibrating / Ready / Degraded / Fault`。
- `FlightAction`：Boost、Pause、Reset、ToggleImmersive、Recalibrate、ThrottleUp、ThrottleDown。
- `pressed` 可一次性清除而不影响 `held`；固定步模拟不能重复执行同一硬件边沿。
- `InputProvider::status(nowMs)` 取代面向 IMU 的独立校准查询。
- `InputProvider::requestCalibration(nowMs)` 对不支持校准的设备允许安全空操作。
- 当前 IMU Provider 保持既有轴映射、死区、滤波和油门数值不变，只迁移动作与状态表达。
- K1 长按由 Provider 输出上下文所需的 Reset/ToggleImmersive，App 根据当前是否撞击只消费一个结果。
- HUD 输入标签由 `InputStatus.axisSource` 生成，不再永久写死为 IMU。

## 4. 自动检查

- [x] `FlightInput` 不超过 24 B，`InputStatus` 不超过 24 B。
- [x] pressed 清除不会清除 held Boost。
- [x] Fake Provider 的一次边沿只出现于一个 sample。
- [x] Joystick2 轴与 Dual Button 动作可以独立报告来源和连接。
- [x] Provider 关闭后报告 Disconnected。
- [x] FlightModel 不会重复消费 Pause 等应用级边沿。
- [x] 规范化轴和持续 Boost 仍驱动原 FlightModel。
- [x] App 游戏逻辑不再直接读取 A/B 键；HAL 按键访问收敛到当前 Provider。
- [x] 既有显式峡谷、飞行和碰撞集成测试通过。
- [x] ESP-IDF 5.5.4 完整固件构建通过。
- [x] 固件大小 `0x479d50` B，最小应用分区余量 `0x762b0` B（9%）。
- [x] 固件 SHA-256：`6068e3143dfaf88c4f045d72b852d5140d1e86259ddc69fcc3b38beee9bbbc69`。
- [x] `/dev/cu.usbmodem83401` 四个分区写入并全部通过 Hash 校验，RTS 硬复位成功。
- [x] 串口启动日志确认 ESP32-S3、8 MB PSRAM、显示、IMU、按键和 Launcher 正常初始化。

## 5. 后续接入门禁

P1 的 IMU 三轴姿态解算只能修改 IMU Provider 内部的物理量到规范化意图映射；不能把角度、重力阈值或陀螺仪状态带入 FlightModel。

G4 接入外设时应新增 Joystick2 AxisSource、Dual Button ActionSource 和组合/降级 Router。飞行模型、战机投影和峡谷渲染不得因为外设型号发生条件分支。

## 6. 阶段结论

代码、主机/固件构建和烧录门禁已经通过。2026-09-04 用户完成真机 Review，校准、游戏进入、现有视觉和输入行为未发现回退；P0 可以提交并进入 P1 三轴 IMU 姿态解算。
