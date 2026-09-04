# Vector Canyon Fighter：G4-P4 外设驱动静态评审

> 日期：2026-09-04  
> 目标：按官方协议完成 Joystick2 与 Dual Button 生产适配器，并在不接通外设的条件下验证软件边界。  
> 非目标：不宣称实体接线已通过、不启用外设输入、不烧录未验证的外设配置。

## 官方协议基线

- Joystick2（U024-V2）：Grove 5V/GND/SDA/SCL，默认 7-bit I2C 地址 `0x63`；`0x50` 返回 X/Y 有符号偏移，每轴为小端序 16 bit；`0xFE` 为固件版本。
- Dual Button（U025）：Grove 黄线为红键、白线为蓝键；官方示例确认两路均为低电平按下。
- ESP32-S3：GPIO3 属于 strapping 引脚，因此线束文档增加“开机/复位/烧录时不按住红键”的约束。

参考：

- [M5Stack Unit Joystick2 产品文档](https://docs.m5stack.com/en/unit/Unit-JoyStick2)
- [M5Stack Joystick2 官方驱动](https://github.com/m5stack/M5Unit-Joystick2)
- [M5Stack Dual Button 产品文档](https://docs.m5stack.com/en/unit/dual_button)
- [M5Stack Dual Button 官方示例](https://github.com/m5stack/M5Stack/blob/master/examples/Unit/DUAL_BUTTON/DUAL_BUTTON.ino)
- [Espressif ESP32-S3 GPIO 文档](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/gpio.html)

## 实现切片

- `Joystick2AxisSource` 在硬件 I2C1 上使用 GPIO10/11、100 kHz，与内部 I2C0（GPIO47/48）隔离。
- 读取在独立 FreeRTOS 任务中以 50 Hz 进行；I2C 失败不会阻塞 UI/仿真线程。
- 首次读取 `0xFE` 成功后才标记设备身份成立；`0x50` 连续失败或样本陈旧会输出无效中性轴。
- 启动静止校准 700 ms，检测校准期间摇杆移动；之后应用死区、渐进曲线及低通滤波。
- `DualButtonActionSource` 对 GPIO3/4 做 20 ms 去抖、500 ms 长按判定，保持与表身 K1/K2 相同的短按/长按动作语义。
- `CompositeInputProvider` 保存 Dual Button 产生的离散油门调整，避免 Joystick2 无油门轴时下一帧把速度恢复为默认值。
- 工厂开关已接通完整外设路径，但默认固定为 `VECTOR_CANYON_USE_EXTERNAL_INPUT=0`。

## 静态门禁

- [x] 官方寄存器地址、端序、按钮电平与项目常量一致。
- [x] 轴/动作驱动只实现既有 Provider 接口；App、FlightModel、Renderer 零修改。
- [x] I2C1 与内部 I2C0 分离，且沿用工程现有 driver-ng `i2c_bus` 组件。
- [x] I2C 读取位于后台任务，UI 线程只读无锁快照。
- [x] 摇杆地址/固件身份未确认或数据超时后，不输出陈旧操纵量。
- [x] GPIO 抖动、短按、长按及长按释放不误触短按均有主机测试。
- [x] 离散油门步进跨帧保持有回归测试。
- [x] Clang C++17 零警告测试通过。
- [x] ESP-IDF 5.5.4 完整构建通过，固件 `0x47a830` B。
- [x] 默认输入仍为 IMU + 表身按键；本阶段没有访问 GPIO3/4 或外部 I2C1。

## 未通过/待实机项

- [ ] 通电前线束导通、绝缘、5V 和按键信号电压检查。
- [ ] 实机读取地址 `0x63`、固件版本及 XY 实际范围。
- [ ] 确认摇杆上下/左右极性；必要时只改 AxisSource 内的轴反转常量。
- [ ] 实测 Dual Button 两键、Boost 长按、输入延迟、帧率和外设功耗。
- [ ] 验证掉电重连；被动 Dual Button 无法自动识别拔线，必须人工按键自检。

## 结论

P4 软件适配器通过静态评审，且没有改变当前实机行为。外设启用门禁保持关闭；下一步是 P5 通电前电气验收及受控实机探测，不能仅凭编译成功越过该门禁。
