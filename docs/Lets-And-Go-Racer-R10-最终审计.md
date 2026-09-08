# R10 最终固件门禁与交接审计

## 完成结论

R0～R10 的无真机开发范围已完成。游戏从 Launcher 进入后具备外设检查、校准、官方车型选择、0～3 对手、无中隔板立交赛道、末位发车、三圈追逐、暂停、结算、重赛、回车库和退出的完整闭环。真机相关项目保持为明确门禁，未被桌面结果替代。

## 阶段提交

| 阶段 | 提交 | 交付 |
| --- | --- | --- |
| R0 | `3a1a9a5` | 完整计划与基线 |
| R1 | `7c7b6a9` | App 骨架与状态机 |
| R2 | `0b1ccf0` | 四辆官方车线框与双星图标 |
| R3 | `dd306dd` | 车库与对手选择 |
| R4 | `98b370c` | SKY LOOP 01 立交与投影 |
| R5 | `fb69876` | 玩家动力学与外设适配 |
| R6 | `faa210a` | AI 与确定性三圈比赛 |
| R7 | `6cd5a49` | 手绘比赛画面与 HUD |
| R8 | `7fe6be2` | 全流程、结算与 NVS |
| R9 | `b39935d` | 自适应性能、反馈与故障处理 |
| R10 | 本文所在提交 | 最终门禁与交接 |

## 需求逐项证据

| 需求 | 状态 | 证据 |
| --- | --- | --- |
| 英文名称与红蓝双星图标 | 完成 | Launcher 注册 `Let's & Go!!`，独立 RGB565 图标资产 |
| 官方热门车型 | 完成 | Cyclone Magnum、Hurricane Sonic、Neo Tridagger ZMC、Brocken Gigant；固定 High/Race LOD |
| 选中放大与动效 | 完成 | 轮胎旋转、底盘振动、逐风线、冲出展示 |
| 0～3 对手/单人 | 完成 | 对手掩码与独立 DONE；Red 可移除当前已选对手 |
| 单条无中隔板立交 | 完成 | SKY LOOP 01，3.8 m 高差，只有左右外护栏 |
| Dual Button + Joystick | 完成 | 复用 Vector Run Joystick2/Dual Button 驱动，失联 fail-neutral |
| 末位随机发车与追逐 | 完成 | 有界固定种子起速，玩家末位且前 2.5 秒 90% 电机效率 |
| 三圈/名次/最佳圈 | 完成 | 60 Hz 固定步长、圈数和结束顺序；NVS 保存最佳圈 |
| 手绘伪 3D 与 HUD | 完成 | 米白纸面、固定铅笔纹、追尾镜头、轮胎、速度线、小地图 |
| 性能和反馈 | 完成 | 车库/比赛独立预算，自动三级细节，受系统开关控制的音效/震动 |

## 自审 A：正确性与契约

发现并修正：

1. 统一门禁起初只覆盖了三个 Vector Run 复用契约；最终扩展为全部 15 个现存 `vector_canyon_*` 测试和 Launcher 外设导航回归。
2. 固定种子 JSON 最初使用 `finishers` 字段，但玩家冲线时对手可能尚未完赛；改为 `standingsAtPlayerFinish`，避免消费者误读。
3. 对手页 Red 短按未落实“移除”语义；新增纯控制层 `cancelRival` 和回归测试，已选时原地移除，未选或 DONE 时返回。
4. 基准生成器直接调用生产车型几何、赛道和比赛控制器；两次生成的 SVG 与 JSON SHA-256 分别完全相同。

## 自审 B：性能、内存与交接

发现并修正：

1. 真机敏感参数曾分散在多个实现文件；新增 `lets_and_go_config.h`，集中 UI 时序、立交尺寸、渲染阈值与反馈强度。
2. 为 `RaceController`、`RaceSnapshot`、`RacerInput`、`RacerState`、`RaceRenderer`、`GarageRenderer` 和预算控制器设置编译期常驻大小上限。
3. 游戏每帧路径使用定长数组，不创建车型照片、视频帧或纹理位图；动态对象仅在 App 打开时创建输入管理器，并在关闭时释放。
4. 静态 SVG 因本地文件浏览安全策略未进行浏览器截图验收；已通过 XML 解析、结构断言和重复生成哈希门禁，最终观感列入真机清单。

## 自动验证

- `tools/test_lets_and_go.sh`：10 个游戏测试、15 个 Vector Run 测试、1 个 Launcher 回归全部通过。
- `SANITIZE=1 tools/test_lets_and_go.sh`：同一组测试在 ASan/UBSan 下全部通过。
- 长时间门禁：64 组确定性场景，每组最多模拟 180 秒，覆盖四车、0～3 对手、失联和非法时间输入。
- 基准产物：SVG 可被 XML 解析；JSON 结构有效；连续两次生成 SHA-256 一致。
- ESP-IDF 独立构建目录清洁编译：通过，无 Let's & Go!! 新增警告；输出中的 AlarmClock、Launcher、Typhoon 与 HAL 警告为已记录基线。

## 固件容量

| 指标 | R0 基线 | R10 最终 | 变化 |
| --- | ---: | ---: | ---: |
| 固件镜像 | `0x47d1a0` | `0x497d00` | `0x1ab60` = 106.84 KiB |
| 最小 App 分区 | `0x4f0000` | `0x4f0000` | 不变 |
| 剩余空间 | `0x72e60` | `0x58300` | 7% 剩余 |

106.84 KiB 增量低于 180 KiB 预警线，也低于 250 KiB 硬上限。

## 交接产物

- 静态基准图：`docs/assets/lets-and-go-reference.svg`
- 固定种子摘要：`docs/assets/lets-and-go-reference-race.json`
- 可重复生成器：`tools/lets_and_go_reference_generator.cpp`
- 一键主机门禁：`tools/test_lets_and_go.sh`
- 烧录、操作、调参与真机门禁：`docs/Lets-And-Go-Racer-使用与真机验收.md`

真机待验证项包括外设电气与手感、实际 FPS/栈水位、圆屏观感、音振强度、AMOLED 功耗和温升。只有完成交接清单后，才能把“无真机代码完成”升级为“设备验收通过”。
