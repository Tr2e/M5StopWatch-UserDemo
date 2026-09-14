# StopWatch · Gundam Museum 适配

以下路径从仓库根目录解析。先读适用的 `AGENTS.md`。当前制作规则是 [Pipeline](pipeline.md) 的通用 G0–G7 加“SD 高达执行路径”；不要将历史模型卡当作最新指令。

## 当前基准与产品行为

2026-09-14，**SD RX-78 v5** 是后续 SD 模型的制作基准。用户在 v4 后反馈“这次结果挺好的”，v5 单展品/新站姿完成后反馈“好”，并要求后续按照 RX-78 模式制作。复用完成度、分件/关节方法和自检闭环；具体比例、造型、姿势和装备从本款资料重新确定。

RX-78 已转为用户正面比例基线的 SD 资产，结构参照同构型 SDCS 资料；用户原图精确出处未确认。SDEX v1 的比例、单顶点针尖及旧三页浏览已失效。v2 被退回扁头和等宽天线，v3 仅头部获局部肯定，v4 身体和装配修复后获认可；不能跳过 v4/v5 经验。

Nu 已独立重建为 **SD BB 战士 #387**（`docs/Gundam-Museum-SD-Nu-制作卡.md`），完成主机、目标构建和烧录启动检查，用户现已反馈“牛高达做的很好”；它与 RX-78 v5 共同验证 SD 制作流程；原 HGUC #086 为历史资产。Strike 已按 **SDEX #002 空战型强袭**独立重建，制作记录见 `docs/Gundam-Museum-SD-Strike-制作卡.md`；HGCE #171 为历史资产，SD 强袭的用户视觉验收单独记录。增加下一台 SD 要独立建模、独立建卡，不只改名字/尺寸。当前浏览每台一个完整装备展品，仅保留屏幕两侧切换箭头与手势拖动；移除标题、名称、AUTO、RESET 和提示文字，松手保持角度；无装备及头部特写保留作离线诊断，用户另有要求时再调整产品合同。

## 新模型必做

1. 建立本款制作卡，明确每张图的比例、结构、姿态、配色或仅问题提示角色。实际查看同构型多角度成品、头脸近照、说明书/板件和装备背面，标明推断与未知。
2. 按 Pipeline 的 SD 路径，依次检查比例、完整身体体积、头脸/天线、胸肩四肢细节、站姿与装备装配；每小步先自检再继续。不要复制 RX-78 的比例阈值、关节角度、插接白名单坐标。
3. 分别给出比例、侧面体积、装备接地、生产显示四项结论；相交检测与渲染比较是不同门禁。读图、技术通过和用户认可分别留档。
4. 模型的 ID 必须同时进入 builder 路由、控制器、渲染缓存、UI 与测试/证据路由。新增模型不能只改枚举或输出文件名。共享修改覆盖全部受影响展品。

## 可复用实现

- `main/apps/app_gundam_museum/model/rx78.h/.cpp`：可编辑 C++ 部件、截面、开口与回接壁，`armFrame/forearmFrame/handFrame` 及手枪共用坐标；不是官方 CAD。
- `model/nu_gundam.*`、`model/strike_gundam.*`：现有独立资产。`ModelId` 进入缓存身份，防止切换后仍显示旧模型。
- `view/museum_renderer.*`：生产 `CarSurfaceRaster`，原生 468×466，内部上限 424×424。产品拖动65%、静止100%；离线回归另测90%。这些比例是内部绘制比例，不是三档几何 LOD。每个展品固定包络，禁止随角度自动缩放掩盖姿态/尺寸错误。
- `controller/museum_controller.h`：当前三展品循环，无装备/头部页不进入导航；移除自动旋转和确认键复位，旧按钮热区可用于拖动；长按 A 或退出和弦仍可返回 Launcher。
- `tools/sd_rx78_geometry_test.h`：脚底共面与接触面积、头脸深度、天线多站截取。全部语义点和区间需按下一款重建。
- `tools/sd_rx78_equipment_test.h`：实际三角面非共面穿越，有限接口正常插接。不是完整实体碰撞求解器；共面/完全包含/动态包络须按任务补查。v3 的 442 对错误相交作为已验证负例，不能换成“新图看着没问题”。
- `tools/sd_nu_geometry_test.h`：BB387 独立比例、头脸/天线/足底探针，枪盾/火箭筒/炮阵与身体相交，六枚炮片之间的相交，以及主动移错步枪的负例。`NuAssembly` 仅按需提供六枚炮的建模面范围，无常驻模型缓存；接口白名单不可移植到下一款。
- `tools/gundam_reference_render.cpp`：同源资产和光栅器的 640×640 结构视图，头脸正斜侧、身体局部与枪盾组合。过滤部件的诊断近景要标注，并保留完整装备旋转。

## 执行命令与边界

以下是**已接入的 RX-78** 示例；每轮选择新目录，不覆盖历史。目前脚本模型参数仅支持 `rx78/nu/strike`，下一型号要先实现自己的路由、相机和检查，不能把 `rx78` 换成任意字符串就认为支持了新模型。

```sh
SANITIZE=1 bash tools/test_gundam_museum.sh /tmp/gundam-new-native rx78
bash tools/render_gundam_reference.sh /tmp/gundam-new-study rx78
python3 tools/package_gundam_evidence.py /tmp/gundam-new-native /tmp/gundam-new-evidence --model rx78 --studies /tmp/gundam-new-study --baseline docs/assets/gundam-sd-rx78-v5
python3 skills/reference-model-workshop/scripts/check_evidence.py --manifest /tmp/gundam-new-evidence/manifest.json --frames /tmp/gundam-new-evidence --output /tmp/gundam-new-review
```

证据检查仅验证文件/清单/解码等完整性，必须实际看最终图。`tools/package_model_alignment.py` 可按记录的等比配准拼参考/旧/新图；参考图私有保存，不当可再分发素材。

RX-78 v5：3284 panels，12 个颈底盖真实补回比较，工作区 710712 B。864 组 = 1 姿态 × 3 绘制比例 × 2 装备诊断状态 × 2 范围 × 3 俯仰 × 24 方位；产品只有一个展品页不意味着删掉诊断覆盖。v5 共享控制器更新时三模型各跑 864 组，合计 2592，不是 RX-78 有三个姿态。新模型按实际状态计算，不照抄数量。

ESP-IDF 使用项目现有环境与组件目录构建。烧录遵循当前授权，先核对设备而非硬编码上次串口；记录实际写入二进制/ELF 摘要及启动，结束后释放监视串口。没有测量的设备 FPS、内存余量和视觉体验不可由主机图/工作区大小代替。

## 历史与溯源入口

- `docs/Gundam-Museum-SD-RX78-Pipeline复盘.md`：v1–v5 全过程、23 类问题、修复与节点映射，供解释原因；实际执行规则已进入主 Pipeline。
- `docs/Gundam-Museum-SD-RX78-用户图比例验收基线.md`：原正面图锚点和当时的差异；其中旧模型状态是历史数据。
- `docs/Gundam-Museum-SD-RX78-V3-头壳与天线返修.md`、`V4-身体体积与装备装配.md`、`V5-单展品与站姿.md`（均带相同 `Gundam-Museum-SD-RX78-` 前缀）：各轮原始证据和反馈。
- `docs/assets/gundam-sd-rx78-v5`：最终原生图、结构图、48 帧转台、源哈希、测试和固件记录。
- `docs/Gundam-Museum-RX78-Pipeline复盘.md`、`docs/Gundam-Museum-Nu-Gundam-R2-RX78经验继承与校准.md`：早期 HG 经验。HG 的旧比例、动作、面数与用户认可不能当成 SD 参数或验收。
