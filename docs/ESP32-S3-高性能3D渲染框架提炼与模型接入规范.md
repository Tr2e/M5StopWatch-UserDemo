# ESP32-S3 高性能 3D 渲染框架提炼与模型接入规范

> 状态：F0–F4 的 S0/S1 纯色静态/刚性范围已完成首轮实现与同机标定；通用材质、自动 LOD、通用双核 scheduler、动画导入、透明/PBR/加权蒙皮仍未形成通用稳定 API
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

### 1.1 当前实现边界（接入前必读）

- 公共 `ModelAsset + renderSceneCachedIndexed` 已验证的产品范围是 S0/S1 纯色静态与单骨刚性分件，当前是单核即时提交。
- 12 B indexed command、双核行带、split color/depth 已在 RX-78 专用后端实测，但还不是任意 `ModelAsset` 自动获得的公共 scheduler 能力。
- `RenderProfile` 当前提供纯函数选路政策，不是一个会自动配置、渲染和送显的完整 renderer。调用者必须真实接通所选路径。
- `LodRange.maximumScreenRadius` 是资产元数据；当前 runtime 只消费业务层显式设定的 `ModelInstance.lod`，尚无公共自动 LOD 选择器。
- `visibilityMask==0` 在标准入口表示不渲染；非零位的 camera/layer 匹配尚未抽象，不得把它宣称为完整分层可见性系统。
- Racer 保留的程序 UV 材质是已验证的旧后端能力，但当前通用 `ModelAsset` 没有 UV 数据 ABI，GLB 编译器也会拒绝纹理/PBR；新 App 不能把 M0/M1 当成已接入能力。

## 2. 已经被真机证明的框架能力

下列能力可以从 Museum / Racer 中抽出，不依赖高达题材。

### 2.1 图元与命令流

- 三角形和共面四边形都作为一等图元；不在光栅热路径重复猜测拓扑。
- 共享顶点在每实例中最多变换、投影一次。RX-78 专用前端先分类面再惰性投影，可避免纯被剔除面的投影；公共即时入口按图元遍历，可能在背面判断前投影其首次出现的顶点，但不会重复投影。
- 纯色材质使用 12 B 索引命令，顶点投影结果与图元命令分离。
- 已知不变量（三角/四边形、近裁剪安全、行带已选、稀疏记录必开）编码进命令或在批次边界选择模板内核，不留在每像素分支中。
- 法线、anchor、投影顶点和命令流按访问频率拆分，避免每个阶段读取完整大对象。
- 需要稳定的多 pass 顺序时，若排序键已计算且有生命周期互斥的 scratch，可单次资产扫描生成各 pass 的稳定索引流，再只访问有效项；不得为了省一次短扫描新增常驻缓冲，回退路径必须能安全重用或覆盖 scratch。

### 2.2 软件光栅

- RGB565 颜色 + 16-bit Q13 逆深度，具备近裁剪和等深覆盖的确定语义。
- 纯色 scanline span 快路径，保守定位边界后跳过已确定在三角形内部的覆盖判定。
- 四边形在面板级共享颜色、存储目标和深度不变量，避免两个子三角形重复解析。
- 必须记录 occupancy 等逐像素位图元数据时，可在单条扫描行内缓存当前 byte，将连续像素及 quad 两个子三角形的 bit 合并后写回；不得跨越会改变顺序语义的边界。该特化会增加代码体积，只在高覆盖路径经 framebuffer 一致性、真机端到端收益和片内余量三项签署后启用。
- 通用 UV/程序材质路径保留，但不让它给纯色快路径付费。
- 热函数可单独 O3/IRAM 化，是实测后的布局决策，不是整编译单元盲目 O3。

### 2.3 内存层级

- PSRAM 存放大型顺序数据；内部 RAM 优先保留给小型、高频、随机读改的状态。
- 已验证的片内优先级：占用位图、投影顶点、ready/pass 标记、有序行带索引、一个光栅分区的深度/颜色。
- 优先分配失败必须回退到正确的 PSRAM/单核路径，不能把“某台设备申请成功”当成永久前提。
- 预算必须同时检查总空闲与最大连续块，并以多次进入/退出后的稳定堆形态签署。总空闲足够不代表单个大块能分配；不得用首轮偶然命中承诺性能等级。
- 对可独立寻址、精度不变的热数据，可用 SoA 把一个超出最大连续块的 AoS 工作集拆成多个有界块。启用必须是 all-or-nothing，任一块失败就释放整组并走正确回退；消费热循环不得为每个元素重复做分段归属判断。
- 可选片内位图、行表和索引工作区必须按已知的实际采样尺寸申请，不按编译期最大画布无条件占满。业务若能在 renderer 打开前确定 profile，应把尺寸传给资源规划；完整最大容量只保留在正确性回退存储中。释放的预算是否交给下一优先级缓存，仍需验证分配命中、最终堆余量和端到端收益。
- 热路径不做每帧堆分配；大对象不放任务栈。

### 2.4 双核与帧流水

- 把光栅目标分成不重叠水平行带，两核保持各自行范围，不改变行带内的原始图元顺序。
- worker 是常驻任务，不每帧创建/销毁。
- 使用有序行带索引减少两核重复扫描，但索引必须在片内 RAM 或直接生成路径中，否则索引写入会吃掉收益。
- 清 framebuffer、清深度、剔除、命令准备、背景、光栅和面板 DMA 按缓冲所有权建立时序图。只并行互不覆盖的目标，在第一个消费者前 join。
- 评估并行只看端到端关键路径。某个子任务因 PSRAM 争用变慢，仍可能让整帧更快；反之亦然。

### 2.5 清理、合成与送显

- 用模型占用位图稀疏清理上一帧深度，同一份位图也供稀疏合成使用。
- 稀疏场景无论 1:1 还是缩放输出，都应测试 occupancy 驱动合成；1:1 不等于必须扫描完整 depth。密集场景才优先测试 deferred occupancy 或完整顺序扫描。
- 缩放内部渲染使用占用像素驱动最近邻放大，不扫描整张深度/颜色。
- 原生 RGB565、rotation、clip、stride 都通过 guard 时才直写 framebuffer；否则回退显示 API。
- 双 framebuffer + 异步面板提交能隐藏部分传输，但必须把下一帧对 PSRAM 的争用计入。
- 局部提交必须为每个 render framebuffer 分别保存上一帧脏区；复用某个 framebuffer 时清理其旧覆盖，并提交“旧覆盖 ∪ 当前覆盖”。两个 buffer 必须各完整初始化一次。
- 当前覆盖优先在图元 setup/光栅提交时顺手维护保守 bounds；不要为了少清几个像素在帧尾扫描整张 occupancy。若矩形过松，再依据实测升级为少量水平带或 tile，不先引入复杂脏区系统。
- 双核光栅不得让两个核心竞争更新同一组普通 bounds。若 occupancy 合成本来就必须扫描有效行，可由每个合成行带顺手生成局部 bounds，join 后 O(1) 合并；这不属于额外的帧尾全图扫描。若合成不读 occupancy，则仍优先使用 setup 阶段保守 bounds。
- 最近邻稀疏放大的映射缓存键必须同时包含 source width/height 与 output width/height。不得把某个固定比例（例如 65% 的 276×276）生成的表复用于 60% 的 254×254 或原生 424×424。
- 稀疏合成所需的整行 scratch 不得放在已经很深的渲染调用栈。主核与 worker 各持有独立、预分配的 source-index/color 行缓存；禁止通过单纯增大任务栈掩盖可迁移的临时大数组。
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
| M0 | 程序 UV/小图集 | Racer 旧后端已验证；通用 `ModelAsset` ABI 未接入 | 不得由新 App 直接宣称支持 |
| M1 | 多材质、高频透视纹理 | 未实现，仅作后续预算级别 | 当前禁止接入产品路径 |

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

### 7.1 刚性分件的标准优化：共享顶点缓存 + `world × pose` 预合成

刚性分件不能只因为使用了共享索引，就假设运行时已经减少了顶点计算。旧的参考入口按图元遍历，每个面角都会重新读取索引并执行 `bone`、`world` 两级仿射变换；共享索引只减少资产存储，没有自动减少 CPU 计算。对于一个 64 共享顶点、48 四边形、8 骨骼的 rigid chain：

```text
旧路径：48 × 4 = 192 次面角访问 × 2 次矩阵应用 = 384 次点矩阵应用
新路径：64 个唯一顶点 × 1 次合成矩阵应用 + 最多 8 次矩阵合成
```

三角形数、图元顺序、材质和像素光栅工作量都不变；优化只消除前端重复计算。当前公共实现位于 `soft3d::RigidRenderScratch`、`renderIndexedModelAssetCached` 和 `renderSceneCachedIndexed`；旧的 `renderRigidModelAssetCached` / `renderSceneCachedRigid` 仅作为兼容别名。标准路径采用以下固定流程：

1. 每个刚性实例开始时，仅清理有效位和计数器，不分配堆内存。
2. 对本帧实际使用的刚性分件，按 `world × pose[rigidPart]` 的实际变换顺序惰性合成 3×4 仿射矩阵，每分件最多一次。`pose` 必须由业务层提供最终的模型空间矩阵；当前 runtime 不会沿 `Bone.parent` 自动求层级世界矩阵。
3. 以 `position index + rigidPart` 为缓存身份；唯一顶点首次出现时执行一次合成矩阵变换和一次投影，后续相邻面直接复用 camera-space 与 screen-space 结果。无 pose 的静态实例也走同一 indexed cache。
4. 如果同一 position index 被不同 rigidPart 非法或低效地复用，冲突面角回退参考变换，不允许缓存改变画面语义。
5. 无 pose 的静态实例仍使用同一 indexed cache，只跳过 pose 合成；只有顶点容量不足或 pose 容量超过骨骼容量时才自动回退 `renderModelAsset`。容量错误只能损失性能，不能造成越界或错误画面。

业务 App 不手写缓存算法，只在长期场景工作区中放置一次 scratch，并调用标准场景入口：

```cpp
struct SceneWorkspace {
    // 容量取本场景“最大单个刚性资产”，不是所有实例之和；实例间串行复用。
    soft3d::RigidRenderScratch<MaxRigidVertices, MaxRigidBones> rigidScratch;
};

soft3d::renderSceneCachedIndexed(raster, camera, scene, workspace.rigidScratch);
```

scratch 不得放在帧循环任务栈中，也不得逐帧构造。当前 `RigidRenderScratch<64,8>` 的主机 ABI 实测为 2,196 B（约 2.15 KiB），包含 camera/projected 两组顶点、刚性分件键、有效位、8 个 3×4 合成矩阵和计数器；`maximum_projection_bytes` 只表示单组投影点，不是整个 rigid scratch。多渲染线程必须各自持有 scratch，不能并发共享。离线资产编译器以 `(position, rigidPart)` 为刚性顶点去重键，并在资产报告的 `memory.rigid_scratch` 中写出最大顶点/骨骼容量和估算字节数。该字节数由 ABI 测试约束；类布局变化时必须同步公式和文档。

这项优化不能替代像素预算。模型放大后，tested pixels、overdraw 和深度拒绝仍可能主导帧时；因此真机结论必须同时记录顶点变换、图元和像素指标，不能把最终 FPS 的全部变化归因于骨骼计算。

### 7.2 `TOTAL/DRAW TRI` 与 `TOTAL/DRAW QUAD` 的统一语义

所有 App 必须使用同一组统计定义，不能由结果页自行推算：

- `TOTAL TRI`：当前实例和当前 LOD 包含的全部三角形；四边形计为 2，三角形计为 1。
- `DRAW TRI`：通过 CPU 侧单面背面判断、近裁剪可见性和屏幕边界检查后，实际提交给光栅器的三角形。
- `TOTAL QUAD`：当前实例和 LOD 中保持为原生 quad 拓扑的四边形数；已离线拆成两个 triangle 的面不计入。
- `DRAW QUAD`：通过同一组提交前检查后，以 quad 拓扑实际提交给光栅器的四边形数。
- `DRAW TRI` 不是最终肉眼可见三角形。被其他表面通过 Z-buffer 遮住的三角形已经提交，仍计入 DRAW；最终可见性应另看 written pixels、depth rejected pixels 和 overdraw。

TRI 是跨拓扑可比的主负载指标，QUAD 只是辅助指标，用于观察离线四边形打包和 solid-quad 快速路径的可利用规模。QUAD 多不等于像素成本低，不能代替 TRI、tested pixels 或 overdraw 做性能结论。

禁止把 `DRAW TRI/QUAD` 直接赋值为 TOTAL，也禁止在结果页用 `visiblePrimitives × 2` 或“三角形等价数 - 图元数”反推，因为 LOD 可以混合 triangle/quad，且后续路径可能引入其他拓扑。公共 `FrameWorkload` 必须直接累计 `totalTriangles`、`submittedTriangles`、`totalQuads`、`submittedQuads`、`culledPrimitives` 和 `offscreenPrimitives`，业务层只读取这些统计。

统计也必须遵守热路径预算。带显式 `PrimitiveTopology` 的公共资产在已经读取的 topology 上顺手累计；没有显式拓扑字段的历史 panel 资产必须在模型建立/重建时只判定一次，并缓存静态 TOTAL 和逐面拓扑位图。禁止为了结果页每帧扫描全模型，也禁止每次提交用浮点坐标相等比较重新猜 triangle/quad。2026-09-23 的 3D Benchmark 同机三轮编译开/关 A/B 中，位图化后的统计最坏增加 `0.394%` 帧间隔，RX-78 为 `0.088%`；若新后端明显超过该量级，必须先修正统计实现，不能用展示需求掩盖性能回退。

标准前端按以下顺序处理每个图元：

1. 根据拓扑先累计 TOTAL。
2. 取得或缓存 camera-space 顶点。
3. 对完全位于近裁剪面前方的单面图元，用变换后的顶点绕序计算 facing；明确背向才剔除。双面图元禁止做背面剔除。
4. 与近裁剪面相交的图元不做激进背面剔除，继续沿用参考裁剪，避免相机穿越时破面。
5. 以近裁剪后的投影边界判断是否完全在 viewport 外；只有完整屏外才能剔除。
6. 通过后复用同一份 prepared panel 进入光栅器，并按原始 topology 累计 DRAW。

背面判断必须基于 camera-space 顶点重新计算，不可直接使用模型空间 normal；这样才能正确覆盖实例旋转、刚性骨骼姿态和统一缩放。prepared panel 的投影与边界结果必须直接供光栅阶段复用，不能为了统计再额外投影一遍。

这一规则已经接入 `renderModelAsset`、`renderIndexedModelAssetCached`、`renderScene` 和 `renderSceneCachedIndexed`。容量不足的刚性回退与缓存路径必须产生相同的 TOTAL/DRAW；简单封闭模型通常应满足 `0 < DRAW < TOTAL`。若两者长期相等，应优先检查业务层是否硬编码、模型是否误标双面、绕序是否错误或前端是否绕过标准入口。

对外 FPS 必须说明时间边界。连续交互 App 建议按相邻已提交帧的完成时刻计算，包含帧间调度和普通循环开销；若只报告 draw throughput，必须明确标为 draw FPS，不能冒充实际帧率。资源建立、阶段切换、日志和结果页应位于采样窗口外，预热后再计数。计时读取本身仍需设备微基准或编译开关 A/B；当前 ESP32-S3 的单次 `esp_timer_get_time()` 实测约 `0.949 µs`，每帧一次相对 29.3 ms 帧间隔约 `0.003%`。

## 8. 新模型如何做到“开箱即用”

### 8.1 作者交付物

静态模型的当前编译器输入为：

```text
models/<name>/
├── model.glb
└── model.toml
```

刚性分件不从 `animations/*.glb` 导入动画。它使用同一 GLB 中的节点几何，由 `model.toml` 的 `rigid_nodes` 标出分件；运行时 pose 由 App 生成并且必须是最终模型空间矩阵。当前没有动画曲线导入、层级 pose 求值或加权 skinning。`model.toml` 最小字段：

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
```

这是单 LOD 最小示例。若要多 LOD，必须先在输入图元序列中准备好各级几何，再为每个 `[lodN]` 显式写 `first_primitive / primitive_count / max_screen_radius`。只增加一个阈值表不会创造减面结果，而且 runtime 仍需业务层设置 `ModelInstance.lod`。

当前编译器支持 glTF 2.0/GLB 的受限子集：索引三角网格、节点静态变换、baseColor 纯色和由 manifest 列出的刚性节点。它明确拒绝透明、纹理/PBR 程序、sparse accessor、morph target 和加权 skinning。“拒绝”不得在对外说明中写成“已支持但较慢”。

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
4. 只在对角线、材质、刚性分件、双面语义与共面条件全部相容时，把 `ABC + ACD` 合并为四边形。
5. 当前只输出 solid 材质，保留确定性图元顺序；不会生成 general material 命令。
6. 按 manifest 写出 LOD 范围和阈值元数据，但不生成、简化或自动选择 LOD 几何；同时生成容量、常驻内存、rigid scratch 估算和风险警告。
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

当前没有 `renderer.render(scene, profile)` 这样的一体化公共 API。资产编译后，业务代码必须显式持有光栅器与工作区，并调用已验证的入口：

```cpp
soft3d::SurfaceRaster<MaxWidth, MaxHeight> raster;
SceneWorkspace workspace; // 长期常驻，不在帧栈上
soft3d::ModelInstance instance;
instance.asset = &soft3d::assets::crate_bot::model();
instance.world = spawnTransform;
instance.lod = selectedLod; // 业务层显式选择
std::array<soft3d::ModelInstance, 1> instances{{instance}};
soft3d::renderSceneCachedIndexed(
    raster, camera, {{instances.data(), instances.size()}}, workspace.rigidScratch);
```

静态模型不应复制 renderer，不应新建一份光栅器，不应手写共享顶点 cache。动画模型只额外提供 `pose`/骨架变换；场景工作区按资产报告配置 `RigidRenderScratch`，静态与刚性实例统一调用 `renderSceneCachedIndexed`，容量不足时由框架正确回退。游戏逻辑、物理、输入和 HUD 不进入模型资产 API。

### 8.5 自动验证

下列是模型发布所需的完整门禁，不等于当前单次 `compile_model.py` 命令会自动执行全部项目。当前 `tools/test_soft3d.sh` 覆盖资产 ABI、编译器正/反例、quad 打包、缓存/回退哈希和 ASan/UBSan；多视角、单/双核、内存失败注入、真机 P95 和人眼验收需由对应的集成/真机流程另行出具证据：

- 标准正/斜/侧/背、近裁剪、屏外、大小屏幕占比渲染。
- 固定种子连续相机姿态的 framebuffer hash。
- 单/双核、内部 RAM 成功/失败回退、原生/缩放合成对照。
- 顶点数、图元数、剔除数、tested/written pixels、overdraw、快路径命中率和峰值 scratch。
- 真机 30/24/20/15 FPS 预算签署：平均、P95、最大帧、内部 RAM/栈水位和退出回收。

刚性模型另有强制门禁：优化路径与参考路径 framebuffer hash 必须一致；统计值必须证明每实例变换次数等于实际使用的唯一 `(position, rigidPart)` 数，矩阵合成次数不超过实际使用骨骼数；TOTAL/DRAW 在参考、缓存和容量不足回退路径必须一致；AddressSanitizer/UndefinedBehaviorSanitizer 必须覆盖正常容量和容量不足回退。

所有封闭单面测试模型还必须断言 `0 < submittedTriangles < totalTriangles`，并保存启用提交前剔除后的 framebuffer hash。性能优化若改变 hash，只能先证明原结果违反单/双面资产契约，不能以“肉眼接近”为由更新基线。

“开箱即用”的定义是：模型通过离线编译和主机门禁后，可不修改光栅器地进入标准 viewer/游戏场景；真机性能等级仍必须实测签署。

## 9. 模型作者的高性能规则

1. 优先减少不可辨识的微小图元，不是简单追求低面数。屏幕上能影响剪影、开口和深度层次的面优先保留。
2. 相邻共面、同材质、同覆盖语义的两三角形保持可识别的共享对角线，便于编译器打包四边形。
3. 删除永久埋藏面；可动件、开口内壁和可从侧后方看到的面不得误删。
4. 双面只用于真正的薄片/可见内壁，不作为修补错误法线的默认开关。
5. 快速游戏默认使用纯色或面级烘焙光照。微小表面纹样不要用大量几何实现，也不要让整个模型为少量 UV 细节进入通用内核。
6. 为不同屏幕尺寸手工或离线生成 LOD，不依赖运行时临时减面。
7. 场景同样需要预算：全屏网格、大面积地面、粒子和多层透明可能比模型本身更贵。
8. 刚性资产必须按 `(position, rigidPart)` 去重；不要让一个 position index 跨骨骼复用，否则正确性回退会失去共享顶点缓存收益。
9. 静态与刚性实例优先走公共 `renderSceneCachedIndexed`，scratch 容量来自资产报告并常驻场景工作区；禁止在业务渲染循环中另写顶点/骨骼缓存或逐帧分配。
10. 封闭模型保持一致且朝外的绕序；确需从背面观察的薄片必须显式标记双面。错误绕序不得依靠关闭公共背面剔除来掩盖。
11. 刚性 pose 的默认构造值不是单位矩阵；App 必须显式填写对角线。`Bone.parent` 当前只是资产元数据，业务层先完成层级求值，再把最终模型空间 `pose[rigidPart]` 交给 runtime。

## 10. 运行时自动选路

框架不应无条件开启 RX-78 的全部机制。当前 `RenderProfile` 只提供稳定的决策函数，调用者仍需将结果连接到真实后端；不得仅调用 `select...()` 或打开 bool 就记录为“已命中快路径”。选路应满足：

- 可见图元/像素工作量低于实测阈值：单核 solid 路径，避免事件和 worker 同步成本。
- 工作量较大且行带平衡：常驻双核 worker + 两条有序行带命令流。
- 内部分辨率等于输出且覆盖稀疏：occupancy 驱动的原生 framebuffer composite；不要仅因 1:1 就扫描完整目标。
- 内部分辨率等于输出且覆盖密集：顺序原生 composite，并 A/B 光栅期记录与 deferred occupancy。
- 内部分辨率小于输出：占用像素驱动的稀疏放大。这是 profile 选项，不是框架强制降质。
- 纯色比例足够高：索引 solid 命令流；其他面稳定回退通用材质路径。
- 没有原生 framebuffer、存储布局或 clip guard 不成立：使用正确的显示 API，不强行直写。

阈值必须由一组小/中/大基准场景在同一台设备上标定，不把 RX-78 的分带参数写死为通用答案。

### 10.1 新 App 必须执行的高性能接入清单

下表是发布门禁，不是建议项。标为“条件必做”的步骤必须先检查条件并留下选路依据；不能因为没有开启 RX-78 的某个开关就判定遗漏，也不能跳过测量后凭感觉关闭。

| 环节 | 要求 | 执行级别 |
| --- | --- | --- |
| 资产 | 不可变 `ModelAsset`、共享索引、确定性顺序、triangle/quad 显式拓扑、正确朝外绕序 | 无条件必做 |
| 材质 | S0/S1 纯色资产必须进入 solid 路径，不得因少量装饰误走 general material | 无条件必做 |
| 帧生命周期 | 禁止逐帧建模、堆分配和大任务栈对象；实例、pose、cache、命令容量常驻工作区 | 无条件必做 |
| 顶点前端 | 每实例唯一 `(position, rigidPart)` 只做一次 camera transform；完全位于近平面前方的唯一顶点只投影一次 | 无条件必做 |
| 刚性前端 | `world × pose[rigidPart]` 每使用分件每帧最多合成一次；容量不足正确回退 | 有刚性 pose 时必做 |
| 可见性 | 单面背面、近裁剪可见性和完整屏外检查必须在提交前完成；输出真实 TOTAL/DRAW | 无条件必做 |
| 纯色图元 | prepared panel 必须 compact 到 solid triangle/quad 路径，使 span、trusted depth 和 quad 快路径真实命中 | 无条件必做 |
| 统计 | TRI/QUAD 在已解析 topology 上顺手累计；历史 panel 在模型重建时缓存 TOTAL 与逐面 topology 位图；禁止逐提交重新猜拓扑或为结果页每帧遍历资产；产品 FPS 禁止开逐像素计数 | 无条件必做 |
| 深度清理 | 有 occupancy 时优先测试稀疏深度清理；span clear 与逐 occupied pixel clear 必须按覆盖形状 A/B。occupancy 在光栅期记录还是 deferred 记录由覆盖密度与合成方式决定，不能按“原生/缩放”一刀切 | 条件必做 |
| 合成 | 以实际覆盖率选 sparse/dense；稀疏 1:1 使用 occupied native copy，低内部采样使用 occupied 最近邻放大，密集场景测试顺序全目标 composite | 条件必做 |
| framebuffer | RGB565、rotation=0、完整 clip、stride、行指针及 32-bit 对齐 guard 全部成立才直写；密集场景全屏清理可用成对 32-bit native fill，稀疏静态背景只清历史覆盖；任一 guard 失败回退显示 API | 条件必做 |
| 脏区正确性 | 双 framebuffer 分别记录历史覆盖，首次使用各完整初始化一次；每次清理该 buffer 的旧覆盖并提交 `old ∪ current`，阶段切换、空帧、移动和缩放均不得留残影 | 使用局部提交时必做 |
| 脏区成本 | 当前 bounds 在图元 setup 时顺手保守累计，读取 O(1)；禁止帧尾全 depth/occupancy 扫描后仍宣称为“稀疏”优化，除非端到端 A/B 证明更快 | 使用局部提交时必做 |
| 送显 | 直接渲染使用外层事务和异步双 framebuffer；稀疏场景提交有效脏区，密集场景允许全屏提交；退出时恢复全局显示状态 | 设备支持时必做 |
| 内存 | occupancy、实例、pose、投影/cache 等小型高频随机状态优先申请内部 RAM，失败保留 PSRAM/普通路径；按“每像素随机访问 > 每顶点/实例”排序申请，并记录实际成功项 | 条件必做 |
| 元数据写入 | 高频 solid/quad 路径若逐像素更新 occupancy，可测试按扫描行合并相邻 byte 写入；必须保持图元/深度语义、通过完整 framebuffer 对比，并同时记录代码体积与片内堆余量 | 条件必做 |
| FPS 计量 | 签署产品 FPS 使用无逐像素计数的生产 raster；相邻提交完成间隔须包含正常帧间调度，或明确标成 draw FPS；资源建立/日志/结果页移出采样窗；`MeasuredSurfaceRaster` 只用于独立诊断 | 无条件必做 |
| 正确性 | 参考/优化/容量回退 framebuffer hash 一致，ASan/UBSan、构建、真机烧录和人眼面板检查分别通过 | 无条件必做 |

以下三项是重负载选路，不得无条件复制 RX-78：

- **双核行带：** 可见图元或诊断像素量达到同机阈值，并且真机端到端 A/B（含同步和 present）确认更快时启用；worker 必须常驻。
- **12 B indexed command stream：** 需要缓存/跨核重放大量命令时启用；少量图元立即提交不应先构建整帧命令流。
- **split color/depth 内部行带：** 只有内部 RAM 预算、行带并行和 PSRAM 争用数据同时支持时启用；申请失败必须回退。

代码评审必须逐项记录“已命中 / 条件不成立 / 真机 A/B 拒绝”，不允许只写“沿用高性能规范”。其中任何无条件必做项缺失，都视为性能或正确性缺陷。

小型演示页的平均 `1 / mean(frame time)` 只适合展示。性能签署必须另外保存连续帧样本并报告 P95/最大帧时、物理面板传输、内存和栈水位。当前 3.4 秒演示阶段不能代替真机签署。

### 10.2 简单场景不得机械复制压力样本策略

RX-78 证明的是各个机制在特定工作量下可行，不是要求所有场景打开同一组策略。2026-09-23 的 3D Benchmark 真机排查给出以下可复用结论：

1. 先按屏幕覆盖、overdraw、背景变化和提交面积分类，再看 TRI。两个大立方体虽然只有 24 total TRI，像素填充仍可占约 8 ms；低面数不等于低像素成本。
2. 固定成本会掩盖模型差异。对阶段 1–3 每帧全清 480×480 framebuffer 约 `17.6 ms`，100% 再扫描 424×424 合成约 `15–16 ms`；这两项与可见图元数无关，是简单场景帧率异常接近复杂场景的首要信号。
3. 稀疏场景应把同一份 occupancy 同时用于深度清理和 native/scaled composite，并只提交历史覆盖与当前覆盖的并集。策略按最终 framebuffer 覆盖和背景变化分类，不按模型名称分类：带全屏动态空间线框的 Museum RX-78 保留全屏策略；纯黑背景的 RX-78 单体转台已证明可使用稀疏/脏区策略。阶段 1–3 最终 100% 由 `20.8 / 16.7 / 19.1` 提至 `44.7 / 28.7 / 36.5 FPS`，60% 由 `33.9 / 29.5 / 32.0` 提至 `72.9 / 51.9 / 60.7 FPS`。
4. 脏区元数据也必须稀疏。第一版每帧扫描 occupancy 求 bounds，改为图元 setup 阶段顺手累计保守 bounds 后，100% 进一步提高约 `4.4%–7.5%`，60% 提高约 `2.6%–3.6%`。
5. 不要因场景简单就关闭内核级优化。反向 A/B 中，关闭 span depth clear 使 100% 降低 `2.8%–7.2%`，关闭 solid-quad 使 100% 降低 `3.1%–4.3%`；共享顶点投影、刚性矩阵预合成、提交前剔除同样继续减少确定工作量。
6. 真正应拒绝无条件复制的是固定管理成本较高的机制：双核 worker 同步、完整 indexed command 构建、split plane，以及密集全屏清理/提交。它们只有在本场景端到端 A/B 为正时才接入。
7. 纯黑 RX-78 转台的三轮真机结果由 100% `112.335 ms / 8.9 FPS` 提至 `90.720 ms / 11.0 FPS`，60% 由 `64.438 ms / 15.5 FPS` 提至 `51.498 ms / 19.4 FPS`。100% 清黑/合成由 `12.952 / 22.443 ms` 降至 `4.389 / 10.203 ms`；60% 为 `11.337 / 5.223 ms` 到 `4.248 / 5.429 ms`。阶段计时有双核重叠，不得求和反推帧时。
8. 带 bounds 的稀疏 helper 不能无条件污染通用热循环。一次实现曾使阶段 1–3 回退约 5–10%；拆分为“无 bounds 通用版本”和“RX 持久 scratch + 行带局部 bounds 版本”后恢复。公共抽象只有在编译结果和真机端到端数据均不回退时才成立。
9. occupancy 的逐像素读改写可按扫描行和 byte 合并，但收益随覆盖量变化，且模板特化会占用片内代码。RX-78 E5 在逐像素结果一致下使 60% 帧时降低约 `0.39%`、100% 降低约 `1.09%`，同时减少约 1.5 KiB internal free；因此它是需同时看速度与内存余量的条件优化，不是无条件规范。
10. 多 pass 遍历可复用生命周期互斥的索引 scratch。RX-78 E6 保持原 pass 与面索引顺序，用单次分类结果生成稳定索引流，60% panel prepare 平均减少约 `0.080 ms`、端到端减少约 `0.21%`，且未增加固件或持久内存；收益有限，只有满足“现成 scratch、顺序可证明、回退可覆盖”时才值得采用。

评审记录必须至少回答四个问题：本帧有多少像素被覆盖；背景有多少区域变化；为优化新增了多少固定扫描/同步/命令构建；双缓冲旧内容由谁清除。只列 TRI 或只列 fast-path 开关不足以签署性能。

## 11. 框架化实施顺序（历史路线与未完成抽取）

下列 F0–F4 是实施路线，不代表每个目标已经全部变成 common API。当前已公共化资产契约、单核静态/刚性前端、光栅桥接、scratch 与选路函数；Museum 的共享投影、双核 worker、indexed command builder 和 split 存储仍带专用类型。这些项目在真正抽入 common 且通过 Museum/Racer/通用场景三方回归前，不得标记为“通用框架已完成”。

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
- 水平微带优先于二维微瓦片；Museum RX-78 的全屏动态网格使 dirty-tile 送显不具备收益基础。该结论只适用于这类动态全屏背景，不得外推到纯黑背景的 RX-78 单体或其他稀疏场景；Benchmark 已证明矩形局部提交有显著收益。
- 有界原型首版必须进入 `62 ms`以内，且 panel/bin 新增成本不高于 `0.3 ms`、主核光栅改善至少 `4 ms`、composite 改善至少 `1 ms`；否则停止完整工程化。

## 13. 成功定义

框架提炼完成不以“把文件移到 common”为标准，而以以下结果为标准：

1. Museum RX-78 画面 hash 与真机帧时不回退。
2. Racer 的通用材质、遮挡和近裁剪路径不回退。
3. 一个全新静态模型只通过 GLB + manifest + 注册即进入标准 viewer。
4. 一个非高达的刚性骨架角色不修改光栅器即能动画。
5. 小/中/大基准场景有真机帧率、P95、内存和退出回收数据，可根据工作量自动选择单/双核路径。
6. 新模型的错误在离线编译或主机门禁中暴露，不在设备上用花屏、栈溢出或性能崩塌来发现。
