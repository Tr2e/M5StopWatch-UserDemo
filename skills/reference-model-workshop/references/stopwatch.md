# StopWatch 适配与历史来源

本页的相对路径都从 **`userdemo` 仓库根目录** 解析，不从已安装技能目录解析。先找到含 `main/apps/app_lets_and_go_racer` 的仓库；没有该仓库时通用方法与证据脚本仍可使用，但下列 C++ 资产不可凭空调用。运行命令前读取项目 `AGENTS.md` 和 `docs/Lets-And-Go-Racer-车型建模与优化规范.md`。

## 资产目录：保持生产单一来源

以下路径基于 `main/apps/app_lets_and_go_racer/`：

| 资产 | 生产源文件 | 复用方式 |
| --- | --- | --- |
| 参数部件库 | `model/car_mesh_builder.h` | `lets_and_go::mesh_parts::Builder`：折脊壳、薄轮罩、管件、凹口、轮毂、导轮；已有八车实际调用，不是未接入示例 |
| 面片与语义 | `model/car_display_mesh.h` | 点、RGB565、UV 边界、`CarPaint`、`CarPart`、轮动标记；四边形拆两三角形 |
| 八车结构样本 | `model/car_display_mesh.cpp` | 每车独立函数；学习部件布局和参数，不复制成所有车同一壳 |
| 程序涂装库 | `view/car_paint.h` | 底色、火焰、蛛网、灯窗和字样的 UV 采样；这是程序材质，不是外部 PNG/PBR 贴图库 |
| 实体栅格器 | `view/car_surface_raster.h` | 逆深度、透视 UV、近裁剪、tile；继续由生产 renderer 调用 |
| 几何验收 | `tools/lets_and_go_car_geometry_test.cpp`（仓库根） | 结构断言、轮胎包络、三档、镜像 UV 和容量 |
| 部件库验收 | `tools/lets_and_go_mesh_builder_test.cpp`（仓库根） | 独立包含头文件，容量／镜像／口腔深度／管体基向量 |
| 生产证据 | `tools/lets_and_go_renderer_test.cpp`（仓库根） | 真实状态机、八车、四视角、三档、玩家／对手、缓存与遮挡 |

不再克隆涂装和栅格器形成“工具包里的第二份实现”。跨引擎迁移优先转写结构与材质卡；若搬用 C++，一并处理所依赖的类型、枚举、坐标和许可。

### 部件 API 契约

`MeshWriter` 使用调用方提供的有效 `CarPanel` 连续缓冲区，容量不足设置 `overflowed`，不能忽略。空输出用 `{nullptr, 0}`。不把大模型返回值放到小设备任务栈上；主机测试的局部数组不是设备内存布局建议。

`Builder.segments` 取 24／18／12 对应 High／Medium／Low；`chine`／`cowl` 需至少两行、有限坐标、递增 z 且总跨度非零。半径、深度和层数使用有效正值。这是受信任的代码创作接口，不是接受任意外部模型文件的安全解析器。

- `ChineStation {z,width,sill,edge,deck}`：车体截面；`CowlStation {z,inner,outer,edge,crown}`：半侧轮罩截面。`cowl(side,...)` 的 `side=±1` 保留对应 UV，薄唇深度独立设置。
- `box` 只有顶面和四侧，不是闭合六面盒；需要底面时明确补上，不误用作密封零件。
- `tube` 为直圆管，无端盖，短到零长度时不输出；弯管以相接路径段构成，端部进入接件。
- `duct` 的开口向 +z，内壁退向 −z；侧向口需要显式轴变换／独立构造及对应测深探针。`shell=0` 表示沿用唇色，不是黑色壳体值。
- `wheel`、`roller` 内含本项目固定尺寸（模型单位，不是毫米）和底盘约定；不能直接当任意车辆轮组。动画只标记轮辐；检查具体版本的轮盖和轮辐数。

## 当前局部约束

模型 +z 向车头、+y 向上、+x 向车库正视图右侧（车辆左侧）；比赛只经 `carPointInTrackBasis` 反射模型 x。不要翻转操控、物理位置或 UV 去抵消错误。

High ≤ 2,048 面、Medium ≤ 1,536、Low ≤ 1,024；八车应从同源实体生成。打开期车库／比赛缓存基准 591,128／481,592 B，常驻 renderer 3,000／3,232 B；改动后重新测量，不保证永恒不变。车库只缓存当前选择、比赛只缓存实际最多四台。

车库左右换车、上下换视角；侧视图到位才转轮，斜前视轻微自动摆动；比赛正常轮动。新增 ID 追加；目前 8 台恰好占满 `uint8_t` 对手掩码，**第九台不能仅追加目录**，须先迁移容量与存档并测试。控制、性能参数、音频和赛道不属于模型外观调整范围。

几何测试每三角形采 66 点、轮胎穿入容差 0.003 模型单位；保留限制的含义，不能声称解析无相交。大型面片缓存按游戏打开／关闭分配释放，避免逐帧分配和任务栈大对象。

## 命令与八车证据清单

```sh
SANITIZE=1 bash tools/test_lets_and_go.sh
SANITIZE=1 bash tools/render_lets_and_go.sh /tmp/lets-go-model-frames
python3 skills/reference-model-workshop/scripts/check_evidence.py \
  --manifest skills/reference-model-workshop/assets/stopwatch-evidence.json \
  --frames /tmp/lets-go-model-frames --output /tmp/lets-go-model-evidence
source ../esp-idf/export.sh
idf.py build
```

选择尚不存在的证据输出目录。要比较基线，加 `--baseline`；将未改车型帧设为 `expect_unchanged: true`，目标车型保留 false／省略并人工解释变化。清单是当前八车配置，新增车须同时补清单和生产场景，不能认为脚本自动发现了所有车型。

每台有 top／side／front／rear／opposite、medium／low、select／showcase、race_player／race_opponent 共 11 帧。前斜视来自实际 select/showcase，反斜视来自 opposite。清单不替代 renderer 内 8×9×3×6 视角／过渡／LOD 检查与极近裁剪压力测试。生成拼图后必须打开查看；先确认展示页实际确认了目标车型，而不是只移动光标。

## 演进档案：保留有效决策，不复活过时方案

源记录在仓库 `docs/`：

| 阶段／文件名 | 当时发现 | 沉淀为当前方法 |
| --- | --- | --- |
| `Lets-And-Go-Racer-R2-车辆几何评审.md`、`R3-车库评审.md`（相同前缀） | 7 截面通用线壳、参考尺寸／版本误读、白线低对比 | 锁版本、有限容量显式溢出、选中缓存、屏幕尺度可读性；线壳不是现生产模型 |
| `Lets-And-Go-Racer-R9-性能与反馈评审.md` | LOD／缓存／反馈与性能约束 | 辨识度优先于细分，区分设备指标与主机验证 |
| `Lets-And-Go-Racer-R12-二次视觉审查.md` | 遮挡顺序、轮动跳相、栈占用和预览可信度 | 真实生产渲染、空间遮挡、距离累计轮相、就地有界输出 |
| `Lets-And-Go-Racer-R14-实车模型重建.md` | 通用船形、Sonic 尾翼和 Brocken 颜色误读 | 独立部件、参考核对、标签与存在性检查；当时画家排序／线框比赛已被 R16 替代 |
| `Lets-And-Go-Racer-R16-实体赛车与比赛细化.md` | 线框不够还原、悬浮贴花、近景错挡 | 同源实体 + UV 程序涂装 + 每像素逆深度；多 tile 等价、按需缓存 |
| `Lets-And-Go-Racer-R17-Magnum腰线与结构复核.md` | 腰线鼓胀、轮罩吞通道／穿胎、比赛贴花镜像 | 先剪影拓扑后细分；窄腰独立轮罩、薄唇、坐标与 UV 一起验 |
| `Lets-And-Go-Racer-R18-其余三车结构复核.md` | Sonic 翼肋、Neo 分窗／轮组、Brocken 管架／马达结构 | 逐台两轮；全附件包络与比赛包围盒；具体版本独立细节 |
| `Lets-And-Go-Racer-R21-交互视角与侧视轮动.md` | 四视角过渡与车库动画约束 | 固定视角 + 过渡 + 三档检查，静态截图不代替动画测试 |
| `Lets-And-Go-Racer-R23-新增四台赛车.md` | 八车掩码、存档与单人后四车缓存失效 | 扩候选不扩所有缓存；检查最高 ID 和参与者变化 |
| `Lets-And-Go-Racer-R24-新增四车逐台结构优化.md` | 四车细节仍近似、假开口、展示图测成同车、PNG 假差异 | 真实口腔／开槽；身份核验；合法近对手；解码像素回归 |

### 八个结构样本的索引

- Magnum：宽肩后先收腰、开放后轮罩通道、硬斜风挡；R17 记录的 .305 肩／.16 腰是模型半宽，不是实测尺寸。
- Sonic：前连接翼中部下弯、尾部阶梯翼肋、贴面肋纹。
- Neo：中央折脊、分窗宽前座舱、后掠分叉翼、盘／盖轮毂与侧导轮。
- Brocken：座舱／马达／前罩三段、圆管与鳍片、六辐旋臂；不加不存在的高尾翼。
- Cobra：收腰、双椭圆口腔、长灯窗、分段金属连接件。
- Spider：硬边金色玻璃与黑窗框、曲线蛛网、独立三层翼、侧导轮。
- Stinger：高刀脊、四个有内壁的喇叭口、弯管和实体中鳍。
- Diospada：倾斜侧口内壁、拱肩翼真实空槽、短槽灯窗；从背面确认未封口。

这些是方法示例与私人游戏资产，不是官方 CAD、官方授权可售资源或精度认证。没有参考的背面和内部仍要标记未知。
