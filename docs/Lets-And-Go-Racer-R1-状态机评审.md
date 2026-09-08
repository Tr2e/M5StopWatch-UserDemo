# Let's & Go!! Racer R1：App 骨架与状态机评审

## 交付内容

- 新增 `AppLetsAndGoRacer`，Launcher 名称为 `Let's & Go!!`。
- 完成 Mooncake 打开、运行、退出和 LVGL/Canvas 切换骨架。
- 新增纯 C++ `GameFlow`，覆盖外设检查到结算的完整状态转移。
- 新增玩家车辆、0～3 对手和赛道选择数据契约。
- 新增 Host 状态机测试，包括完整比赛、单人、非法转移、返回和重赛。

R1 中图标暂时复用已有 `icon_vector_run`，只为保证 Launcher 始终获得有效图像指针；R2 必须以田宫红蓝双星图标替换。

## Self-review A：正确性与架构

### 发现与修正

1. `rivalMask` 使用 8 bit，初版缺少车辆数量的编译期上限。已增加 `kCarCount <= 8` 和最大对手数静态断言。
2. 玩家更换车辆时必须清除旧对手集，避免新玩家车辆同时出现在对手中。已在 `selectPlayerCar()` 和返回车库路径处理。
3. 非法阶段调用必须无副作用。测试已覆盖跳过校准、未倒计时开赛和未比赛结束等路径。
4. 模型/控制代码不包含 HAL、M5GFX 或 LVGL 依赖，Host 测试可独立编译。

### 验证

- 严格 Host 编译：通过（C++17，`-Wall -Wextra -Werror -pedantic`）。
- AddressSanitizer + UndefinedBehaviorSanitizer：通过。
- 状态机测试：通过。

## Self-review B：性能、视觉与失败路径

### 发现与修正

1. M5GFX 文本对齐应使用明确的 `textdatum_t::middle_center`，已替换未限定枚举名，避免固件编译环境的名称查找差异。
2. 骨架页只以 10 FPS 更新，不在尚无动画的阶段按 30 FPS 反复填充全屏。
3. A+B 返回主页约定保留；`ExitRequested` 在 App 调度层收敛为 `close()`，不由纯状态机操作硬件。
4. 重复打开时重置流程、帧时间和 KeyManager，关闭时恢复 LVGL 刷新。

### 验证

- ESP-IDF 完整固件编译：通过。
- 新增代码编译警告：0。
- 固件由 `0x47d1a0` 增加到 `0x47d960`，R1 增量 `0x7c0`（1984）字节。
- 分区剩余 `0x726a0` 字节（9%）。

## R1 结论

R1 通过两轮自审、Host 严格测试、Sanitizer 和完整固件编译，可阶段提交并进入 R2。
