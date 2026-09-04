# Vector Canyon Fighter：G4-P3 外设组合层评审

> 日期：2026-09-04  
> 目标：在通电接入外设前，建立可独立组合摇杆轴和按键动作的生产接口。  
> 非目标：不访问 GPIO/I2C，不切换默认输入，不假定电气连接已完成。

## 实现

- `FlightAxisProvider` 独立输出规范化 steer、pitch、throttle，以及轴设备状态和校准能力。
- `FlightActionProvider` 独立输出 pressed/held 动作及按键设备状态。
- `CompositeInputProvider` 将二者合成为既有 `FlightInput/InputStatus`，App、FlightModel 与 Renderer 无需修改。
- 动作源失联时清空全部动作并报告 `Degraded`，健康轴仍可操纵；轴源失联时输出无效中性轴并报告 `Disconnected/Fault`。
- 校准请求只路由至轴源；关闭顺序为动作源后轴源，便于后续先停止 GPIO 事件再释放 I2C。

## 门禁

- [x] Fake Joystick2 + Fake Dual Button 能独立报告来源并正确组合。
- [x] 按键掉线不使摇杆轴失效，且不会保留 Boost/边沿动作。
- [x] 摇杆掉线强制 steer/pitch 为 0、`valid=false`。
- [x] Provider 对越界轴执行最终 `[-1,1] / [0,1]` 钳位。
- [x] 校准只传给 AxisProvider。
- [x] Clang C++17 零警告测试通过。
- [x] ESP-IDF 5.5.4 完整构建通过。
- [x] 默认工厂仍使用 IMU；固件大小保持 `0x47a830` B，无真机行为变化，无需重复烧录。

## 阶段结论

P3 已消除最终双设备接入时需要改写游戏核心的风险。下一阶段可以分别实现 Joystick2 I2C 轴源和 Dual Button GPIO 动作源，但在接线检查完成前不得切换默认工厂或为外设供电。
