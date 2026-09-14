---
name: reference-model-workshop
description: Create, refine and review reference-based real-time hard-surface 3D assets such as miniature race cars, Gundam kits, aircraft and mechanical models. Use for staged modeling pipelines, reference and runner checks, proportions, UV/livery, LOD and production-render evidence. Not for machine-learning models, logo design or concept-image-only work.
---

# Reference Model Workshop

把“版本 → 参考 → 结构 → 灰模 → 辨识点 → 精修 → 生产验证 → 用户验收”作为完整资产闭环。来源包括八台迷你四驱车、HG RX-78、牛高达返工，以及用户已基本认可的 SD RX-78 v5；复用制作和检查方法，不复制不同型号的尺寸。

## 执行入口

1. 检查项目约定、既有模型／渲染器及工作区；明确本次目标、版本和不变项。修改已有资产前保存生产渲染基线。仅审查请求不授权实现修改。
2. 新建或精修模型均先阅读 [分节点 Pipeline](references/pipeline.md)，其中已直接规定 RX-78 与牛高达返工所得的校准动作及交付前视觉复核。新模型创建 `assets/model-card.md` 工作副本并逐节点留证；局部修正从受影响节点进入，交付前复核仍须执行，不机械重做未受影响的内容。新增／结构修改时阅读 [结构方法](references/geometry.md)；材质／渲染修改时阅读 [材质与显示](references/materials.md)。同时涉及两者时两份都读。
3. 开工前完整阅读 [审查与证据](references/review.md)。每台按结构、显示两轮审查：发现 → 修复 → 重测；通过本台门禁后再处理下一台，不用“CR passed”代替记录。
4. StopWatch 四驱车任务阅读 [项目适配与来源](references/stopwatch.md) 及该仓库的车型规范；Gundam Museum 任务阅读 [高达适配](references/gundam.md)。新建或精修 SD 高达必须执行 Pipeline 的“SD 高达执行路径”，按制作卡逐项记录比例、头脸体积、胸肩厚度、站姿和装备相交检查。后续 SD 模型以 RX-78 v5 的完成度与检查闭环为基准，具体形状仍取本款参考。其他引擎只沿用通用方法，自行确定坐标、预算、格式、测试和工具。
5. 使用既有生产构建器、材质和渲染通路。源代码项目交付可编辑部件及回归；DCC 项目交付源场景、材质、导出物及对应引擎证据。不要为套用本技能擅自更换引擎或渲染架构。
6. 将目标视角列入 `assets/evidence-manifest.json` 的工作副本，用 `scripts/check_evidence.py` 验证证据并生成原尺寸拼图。实际打开查看拼图，结合参考人工判定结构和涂装；脚本通过只说明它检查的项目通过。

## 交付与边界

- 输出结构／材质卡、可编辑资产、两轮审查记录、实际运行画面、测试与预算、尚未验证项。手工参数模型不等于官方 CAD 或实测复刻。
- 节点自检由代理持续执行；不把每个节点变成许可请求。轮廓和关键辨识点有明显偏差时先修正，再堆细节。只有用户明确反馈才记录用户视觉验收；编译、证据脚本和渲染测试不能代替它。
- 粗模仅作内部制作基线，除非用户主动要求阶段审阅。未通过 Pipeline 的交付前视觉复核，不交付为完成品或候选版；已知脸部、比例、装配与可见几何缺陷须继续修正，不能列作“局限”后结束任务。
- 模型的剪影、腰线、开口和支撑应由几何成立，不能靠黑贴纸或堆面数掩盖；低档仍保留辨识点。
- 参考照片和品牌车型不因私人使用而获得公开再分发许可。默认保存来源记录，不把参考照片打包进可分发工具包；保留已有代码许可，公开发布另核权利。
- 本技能不授权 push、烧录、公开发布或重写无关功能。按当前用户要求决定提交；无设备不得把主机测试写成真机帧率验收。
