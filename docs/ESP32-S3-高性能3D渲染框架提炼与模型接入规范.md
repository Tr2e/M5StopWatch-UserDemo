# ESP32-S3 高性能 3D 渲染框架提炼与模型接入规范

> 状态：F0–F4 首轮实现与同机真机标定完成；透明/PBR/加权蒙皮仍为显式非目标
> 更新：2026-09-23  
> 压力样本：Gundam Museum RX-78，65% 内部采样，2,736 面板  
> 当前真机基线：draw `66.572 ms / 15.021 FPS`，含 present 完整周期 `66.673 ms / 14.998 FPS`

> F0 基线已于 2026-09-23 在当前主机与同一台 ESP32-S3 上重现；原始命令、哈希和阶段数据见
> [`assets/soft3d-framework-20260923/F0-baseline.md`](assets/soft3d-framework-20260923/F0-baseline.md)。
> F1–F4 的最终主机门禁、GLB 编译器能力、三场景真机 P95、内存/栈和退出重建数据见
> [`assets/soft3d-framework-20260923/F4-validation.md`](assets/soft3d-framework-20260923/F4-validation.md)。

## 1. 结论

**首轮框架已可用：固态静态/刚性模型可由 GLB + TOML 离线编译成不可变 `ModelAsset`，通过统一场景入口进入同一光栅器；错误资产在主机门禁阻断，性能等级由真机签署。**

当前边界是刻意收窄而非静默降级：透明、纹理/PBR program、morph target 和四权重 skinning 会被编译器拒绝。底层像素内核继续复用 Racer 中已经证明的实现，公共 `soft3d::SurfaceRaster` 是稳定入口；待通用材质 ABI 扩展时再把最后的实现文件物理迁入 common，不为目录纯度重写已验证数学。

RX-78 是一个合适的上界压力样本：面数高、细小结构多、双面件多、屏幕覆盖大，还带有全屏背景、缩放合成和面板传输。它已经稳定接近 15 FPS，说明底层技术路线可用；但不应把 15 FPS 当作所有游戏的上限。更少的可见图元、更小的屏幕覆盖、更少的 overdraw、更简单的背景和无缩放合成的场景，应该获得更高帧率。

这个判断不等于承诺“低面数必然 30/60 FPS”。ESP32-S3 的帧时还受清屏、像素覆盖、PSRAM 带宽、合成和面板传输影响；面数只是其中一个维度。框架应以实际工作量和端到端帧时选路，不以模型名称或单一面数做推断。

## 2. 已经被真机证明的框架能力

下列能力可以从 Museum / Racer 中抽出，不依赖高达题材。

### 2.1 图元与命令流

- 三角形和共面四边形都作为一等图元；不在光栅热路径重复猜测拓扑。
- 共享顶点只投影一次，被剔除的面不触发无效投影。
- 纯色材质使用 12 B 索引命令，顶点投影结果与图元命令分离。
- 已知不变量（三角/四边形、近裁剪安全、行带已选、稀疏记录必开）编码进命令或在批次边界选择模板内核，不留在每像素分支中。
- 法线、anchor、投影顶点和命令流按访问频率拆分，避免每个阶段读取完整大对象。

### 2.2 软件光栅

- RGB565 颜色 + 16-bit Q13 逆深度，具备近裁剪和等深覆盖的确定语义。
- 纯色 scanline span 快路径，保守定位边界后跳过已确定在三角形内部的覆盖判定。
- 四边形在面板级共享颜色、存储目标和深度不变量，避免两个子三角形重复解析。
- 通用 UV/程序材质路径保留，但不让它给纯色快路径付费。
- 热函数可单独 O3/IRAM 化，是实测后的布局决策，不是整编译单元盲目 O3。

### 2.3 内存层级

- PSRAM 存放大型顺序数据；内部 RAM 优先保留给小型、高频、随机读改的状态。
- 已验证的片内优先级：占用位图、投影顶点、ready/pass 标记、有序行带索引、一个光栅分区的深度/颜色。
- 优先分配失败必须回退到正确的 PSRAM/单核路径，不能把“某台设备申请成功”当成永久前提。
- 热路径不做每帧堆分配；大对象不放任务栈。

### 2.4 双核与帧流水

- 把光栅目标分成不重叠水平行带，两核保持各自行范围，不改变行带内的原始图元顺序。
- worker 是常驻任务，不每帧创建/销毁。
- 使用有序行带索引减少两核重复扫描，但索引必须在片内 RAM 或直接生成路径中，否则索引写入会吃掉收益。
- 清 framebuffer、清深度、剔除、命令准备、背景、光栅和面板 DMA 按缓冲所有权建立时序图。只并行互不覆盖的目标，在第一个消费者前 join。
- 评估并行只看端到端关键路径。某个子任务因 PSRAM 争用变慢，仍可能让整帧更快；反之亦然。

### 2.5 清理、合成与送显

- 用模型占用位图稀疏清理上一帧深度，同一份位图也供稀疏合成使用。
- 缩放内部渲染使用占用像素驱动最近邻放大，不扫描整张深度/颜色。
- 原生 RGB565、rotation、clip、stride 都通过 guard 时才直写 framebuffer；否则回退显示 API。
- 双 framebuffer + 异步面板提交能隐藏部分传输，但必须把下一帧对 PSRAM 的争用计入。
- PSRAM 直连 GPSPI/GDMA 在当前硬件上会 TX underflow；完整重传后又比 4 行 SRAM bounce 慢，不是当前默认送显路径。

### 2.6 正确性与性能方法

- 固定姿态序列比较完整 framebuffer hash，不只看一张截图或局部包围盒。
- 分段记录 draw、present、clear、cull、prepare、CPU0/CPU1 raster、composite 和 panel DMA。
- 主机回归、ASan/UBSan、真机交错 A/B、多次独立重启和人眼面板验收是不同门禁，不互相替代。
- 不保留“代码看起来更快”但整帧无可重复收益的候选。寄存器压力、指令 cache、PSRAM 争用和编译器特化都可能反转源码层面的直觉。

## 3. 不应进入通用框架的 RX-78 特例

- Gundam `Part`、`Pose`、装备、展柜网格、Museum UI 和 424 固定尺寸。
- 用 `CarPanel.wheel` 代表骨骼 ID 的历史复用。
- `MuseumRenderer` 中大量为 A/B 留下的 `set...FastPath()` 开关。框架应使用少量 profile/能力位，不把每次实验都永久变成 API。
- RX-78 专用容量、近裁剪安全假设、相机距离和上下半屏切分点。
- 为保持特定画面 hash 的面顺序细节。通用资产仍需确定性顺序，但顺序应由资产包显式指定。
- 用 C++ 程序原语手工生成某个角色的方式。它可作为可选 authoring backend，不能是新模型的唯一入口。

## 4. 建议的公共框架分层

```text
game / museum / viewer
        │  Scene, Camera, ModelInstance, HUD
        ▼
scene runtime
        │  transform / skeleton / visibility / LOD
        ▼
render frontend
        │  shared projection / cull / ordered compact commands
        ▼
render scheduler
        │  frame graph / memory policy / single-or-dual-core bands
        ▼
raster backend
        │  solid fast path | general material path
        ▼
compositor + display backend
           native RGB565 / scaled sparse composite / async 4-row bounce
```

建议目录：

```text
main/apps/common/soft3d/
├── asset/                 model_asset / model_instance / material
├── frontend/              projection_cache / visibility / command_builder
├── raster/                surface_raster / solid_kernel / material_kernel
├── runtime/               render_profile / scheduler / scratch / stats
└── display/               framebuffer_compositor / async_panel_presenter

tools/soft3d_asset_compiler/     compile / validate / reference render / benchmark
```

初次抽取不应重写数学内核。先保持已验证的像素语义，把类名、资产类型、配置和应用依赖解耦；再用 Museum 和 Racer 同时回归证明抽取没有造成性能和画面回退。

## 5. 核心资产契约

每个模型编译成不可变 `ModelAsset`，每个场景对象只保存轻量 `ModelInstance`。

```cpp
struct ModelAssetView {
    Span<const PackedPosition> positions;
    Span<const PackedNormal> normals;
    Span<const Primitive> primitives;
    Span<const Material> materials;
    Span<const uint16_t> orderedPrimitiveIndices;
    Span<const LodRange> lods;
    Span<const Bone> skeleton;          // 静态模型为空
    Bounds bounds;
    AssetFlags flags;
};

struct ModelInstance {
    const ModelAssetView* asset;
    Transform world;
    uint16_t lod;
    uint16_t visibilityMask;
    Span<const BoneTransform> pose;     // 静态模型为空
};
```

资产包必须显式携带：

- 坐标契约：右手系，`+Y` 向上，正面方向和单位缩放在 manifest 中声明，编译后统一。
- 原点、支撑面/地面高度、静态包围球和 AABB。
- 共享顶点索引，三角/共面四边形拓扑，材质 ID，单/双面属性。
- 确定性图元顺序。编译器可以合并相邻共面三角形，但不可随意改变共边和等深覆盖语义。
- 多级 LOD 的独立范围、距离/屏幕尺寸门槛和每级预算。LOD 是离线资产，不在 ESP32 运行时简化网格。
- 可选骨架和绑定。MVP 先支持“一个顶点/面隶属一根骨骼”的刚性分件；四权重 skinning 必须单独测量后才能成为默认。
- 编译期容量、预计常驻字节、最大投影 scratch、材质通道分布和预计快路径命中率。

## 6. 材质和渲染能力分级

不要让所有模型都走最通用、最贵的路径。

| 等级 | 能力 | 默认路径 | 适用场景 |
| --- | --- | --- | --- |
| S0 | RGB565 纯色，烘焙或面级光照 | 12 B indexed solid command | 高帧率游戏角色、场景道具、机械模型 |
| S1 | 纯色 + 双面/透视近裁剪/骨架分件 | solid fast path + guarded fallback | 动画角色、展示模型 |
| M0 | 程序 UV/小图集 | general material path | 赛车涂装、低分辨率细节 |
| M1 | 多材质、高频透视纹理 | 仅经专项预算后启用 | 少量英雄物件 |

对一般快节奏游戏，S0/S1 应是默认；细节优先用颜色分块、剪影和几何层次表达。只有确实产生屏幕价值的区域才进入通用 UV 内核。

## 7. 性能不能只用面数分级

每帧成本更接近：

```text
frame = fixed scene/display cost
      + transformed unique vertices
      + culled/tested primitives
      + tested pixels and overdraw
      + material cost per written pixel
      + composite/present cost
```

RX-78 当前样本为 2,736 面板，65% 时平均约 41,468 次像素覆盖，最终覆盖约 13,566 像素，约 26.3% 覆盖被深度拒绝，每个最终像素平均被写约 2.255 次。这是压力数据，不是新模型必须填满的预算。

| 目标 | 完整帧预算 | 适用策略 |
| --- | ---: | --- |
| 30 FPS | 33.33 ms | 简单背景、S0/S1、少量可见图元；简单场景可单核，避免 worker 固定开销 |
| 24 FPS | 41.67 ms | 中等角色/道具数，视工作量启用双核行带 |
| 20 FPS | 50.00 ms | 较复杂场景，严格控制 overdraw、材质和背景成本 |
| 15 FPS | 66.67 ms | RX-78 级压力场景，不应作为普通新场景的设计目标 |

上表是框架调度预算，不是尚未实测的模型数量承诺。资产编译器只报静态风险；主机固定场景和真机运行才能签署帧率等级。

## 8. 新模型如何做到“开箱即用”

### 8.1 作者交付物

静态模型只需：

```text
models/<name>/
├── model.glb
└── model.toml
```

有骨架模型另加 `animations/*.glb`。`model.toml` 最小字段：

```toml
name = "crate_bot"
up = "+Y"
front = "+Z"
unit_scale = 1.0
pivot = "ground_center"
default_material = "solid"
two_sided = false
target_profile = "30fps"

[lod0]
max_screen_radius = 9999

[lod1]
max_screen_radius = 80
```

首个资产编译器建议支持 glTF 2.0/GLB 子集：静态网格、顶点色/纯色材质、单层 UV、索引三角形、节点变换和刚性骨骼绑定。不在 MVP 中承诺 PBR、透明排序、morph target、四权重蒙皮或任意 shader。

### 8.2 离线编译

```bash
python3 tools/soft3d_asset_compiler/compile_model.py \
  models/crate_bot/model.glb \
  --manifest models/crate_bot/model.toml \
  --out main/assets/soft3d/crate_bot
```

编译器必须自动完成：

1. 统一坐标、单位、原点和三角形绕序。
2. 顶点去重，建立紧凑共享索引。
3. 计算法线/anchor/包围体，标记双面件和近裁剪风险。
4. 在不改变原对角线和顺序的前提下，合并符合条件的共面三角形为四边形。
5. 按材质能力拆分 solid/general 命令范围，但保留显式渲染顺序。
6. 生成 LOD 范围、容量报告、预计常驻/临时内存和风险警告。
7. 输出可直接链接的 `model_asset.h/.cpp` 或只读二进制 blob + descriptor。
8. 生成标准测试场景和资产报告，不要求业务 App 手写测试视角。

### 8.3 静态门禁

以下问题直接阻断编译：

- NaN/Inf、过多退化三角形、索引越界、容量溢出。
- 未声明透明材质、不支持的 skinning 或 shader。
- LOD 缺失材质/骨骼引用，包围体不包含全部顶点。
- 资产声称 S0 但包含 UV/逐像素材质。

以下为预算警告，不应悄悄修改模型：

- 屏幕尺寸下不可辨识的大量微小三角形。
- 过高的双面比例、材质切换数和重叠层数。
- 可见面占比低，埋藏面或内部结构过多。
- 图元很少但几乎全屏覆盖；这类资产像素成本仍可能很高。

### 8.4 接入 App

资产编译后，业务代码只做三件事：

```cpp
auto model = soft3d::assets::crateBot();
scene.add({.asset=&model, .world=spawnTransform});
renderer.render(scene, camera, RenderProfile::Game30);
```

静态模型不应复制 renderer，不应新建一份光栅器，不应手写共享顶点 cache。动画模型只额外提供 `pose`/骨架变换。游戏逻辑、物理、输入和 HUD 不进入模型资产 API。

### 8.5 自动验证

每个新模型自动获得：

- 标准正/斜/侧/背、近裁剪、屏外、大小屏幕占比渲染。
- 固定种子连续相机姿态的 framebuffer hash。
- 单/双核、内部 RAM 成功/失败回退、原生/缩放合成对照。
- 顶点数、图元数、剔除数、tested/written pixels、overdraw、快路径命中率和峰值 scratch。
- 真机 30/24/20/15 FPS 预算签署：平均、P95、最大帧、内部 RAM/栈水位和退出回收。

“开箱即用”的定义是：模型通过离线编译和主机门禁后，可不修改光栅器地进入标准 viewer/游戏场景；真机性能等级仍必须实测签署。

## 9. 模型作者的高性能规则

1. 优先减少不可辨识的微小图元，不是简单追求低面数。屏幕上能影响剪影、开口和深度层次的面优先保留。
2. 相邻共面、同材质、同覆盖语义的两三角形保持可识别的共享对角线，便于编译器打包四边形。
3. 删除永久埋藏面；可动件、开口内壁和可从侧后方看到的面不得误删。
4. 双面只用于真正的薄片/可见内壁，不作为修补错误法线的默认开关。
5. 快速游戏默认使用纯色或面级烘焙光照。微小表面纹样不要用大量几何实现，也不要让整个模型为少量 UV 细节进入通用内核。
6. 为不同屏幕尺寸手工或离线生成 LOD，不依赖运行时临时减面。
7. 场景同样需要预算：全屏网格、大面积地面、粒子和多层透明可能比模型本身更贵。

## 10. 运行时自动选路

框架不应无条件开启 RX-78 的全部机制。建议 `RenderProfile` 只提供稳定的决策：

- 可见图元/像素工作量低于实测阈值：单核 solid 路径，避免事件和 worker 同步成本。
- 工作量较大且行带平衡：常驻双核 worker + 两条有序行带命令流。
- 内部分辨率等于输出：原生 framebuffer composite；不做缩放批次。
- 内部分辨率小于输出：占用像素驱动的稀疏放大。这是 profile 选项，不是框架强制降质。
- 纯色比例足够高：索引 solid 命令流；其他面稳定回退通用材质路径。
- 没有原生 framebuffer、存储布局或 clip guard 不成立：使用正确的显示 API，不强行直写。

阈值必须由一组小/中/大基准场景在同一台设备上标定，不把 RX-78 的分带参数写死为通用答案。

## 11. 框架化实施顺序

### F0：冻结正确性和压力基线

- 保留 RX-78 canonical hash、6,720 组主机回归和当前 15 FPS 真机基线。
- 加入至少两个通用基准：简单静态场景和中等动画场景。
- 为三个基准统一记录图元、像素、内存、draw/present 和 P95。

### F1：只抽取已证明的 runtime

- 把 `CarSurfaceRaster` 与 Racer 命名/材质解耦为 `soft3d::SurfaceRaster`。
- 把 `MuseumProjectionCache` 抽成无 Gundam 类型依赖的共享顶点前端。
- 抽出紧凑 solid 命令、行带 worker、scratch 策略、合成和统一指标。
- Museum 和 Racer 切换到公共层，不改变画面和真机性能。

### F2：定义 `ModelAsset` 并做手工转换器

- 用一个简单静态模型和一个刚性骨架模型证明资产 API。
- 不先追求完整 glTF，先确定运行时格式的字节数、对齐、顺序和回退语义。

### F3：离线资产编译器

- GLB + TOML 输入，生成可链接资产、报告和测试场景。
- 自动合并安全四边形、计算包围体、材质分类、容量和风险。
- 新模型不修改 renderer 即通过标准 viewer。

### F4：实测选路与性能等级

- 在真机上测小/中/大场景，标定单/双核、native/scaled、solid/general 的切换阈值。
- 给资产和场景输出实测的 30/24/20/15 FPS 等级，不用面数预测代替设备结果。

## 12. 暂停与未进入当前框架的研究

`一次分箱 + 持续边状态的片内微带/微瓦片` 是未来研究项，不是 F0–F4 的前置条件，不应阻塞现有框架抽取。

它的潜在价值是将上半屏深度/颜色工作集保留在片内 RAM，同时消除旧微带实验的重复 setup 和图元回放。但要达到 18 FPS，RX-78 完整周期必须从 `66.673 ms` 降到 `55.556 ms`，净省 `11.117 ms`；旧 20 行微带只使 composite 改善约 `1.11 ms`，却因重复 setup 显著变慢。因此不将 18 FPS 作为该架构的承诺。

未来只在以下条件下重开：

- 使用进入/退出事件或有序 bitset 一次分箱，不为每带复制完整成员列表。
- 跨带图元持续边/setup 状态，不重复除法、投影和边初始化。
- 维持确定性面顺序、Q13 深度和等深覆盖语义。
- 水平微带优先于二维微瓦片；当前全屏网格使 dirty-tile 送显不具备收益基础。
- 有界原型首版必须进入 `62 ms`以内，且 panel/bin 新增成本不高于 `0.3 ms`、主核光栅改善至少 `4 ms`、composite 改善至少 `1 ms`；否则停止完整工程化。

## 13. 成功定义

框架提炼完成不以“把文件移到 common”为标准，而以以下结果为标准：

1. Museum RX-78 画面 hash 与真机帧时不回退。
2. Racer 的通用材质、遮挡和近裁剪路径不回退。
3. 一个全新静态模型只通过 GLB + manifest + 注册即进入标准 viewer。
4. 一个非高达的刚性骨架角色不修改光栅器即能动画。
5. 小/中/大基准场景有真机帧率、P95、内存和退出回收数据，可根据工作量自动选择单/双核路径。
6. 新模型的错误在离线编译或主机门禁中暴露，不在设备上用花屏、栈溢出或性能崩塌来发现。
