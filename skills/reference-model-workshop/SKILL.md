---
name: reference-model-workshop
description: Create, refine, texture and review reference-based real-time hard-surface 3D assets such as miniature race cars, aircraft and mechanical models. Use for structural fidelity, silhouettes, UV/livery, LOD and production-render evidence. Not for machine-learning models, logo design or concept-image-only work.
---

# Reference Model Workshop

把“参考版本 → 结构卡 → 独立部件 → 表面材质 → 两轮审查 → 生产证据”作为一个完整资产闭环。来源是八台迷你四驱车从通用线框到实体模型的多轮修正；复用决策方法，不把特定车型、品牌、设备限制当作普遍规则。

## 执行入口

1. 检查项目约定、既有模型／渲染器及工作区；明确本次目标、版本和不变项。修改已有资产前保存生产渲染基线。仅审查请求不授权实现修改。
2. 创建或更新 `assets/model-card.md` 的工作副本。新增／结构修改时先完整阅读 [结构方法](references/geometry.md)；材质／渲染修改时先完整阅读 [材质与显示](references/materials.md)。同时涉及两者时两份都读。
3. 开工前完整阅读 [审查与证据](references/review.md)。每台按结构、显示两轮审查：发现 → 修复 → 重测；通过本台门禁后再处理下一台，不用“CR passed”代替记录。
4. 在 StopWatch / Let's & Go!! 仓库中还需完整阅读 [项目适配与来源](references/stopwatch.md)，再读取该仓库的车型规范。其他引擎只沿用通用方法，自行确定坐标、预算、格式、测试和工具。
5. 使用既有生产构建器、材质和渲染通路。源代码项目交付可编辑部件及回归；DCC 项目交付源场景、材质、导出物及对应引擎证据。不要为套用本技能擅自更换引擎或渲染架构。
6. 将目标视角列入 `assets/evidence-manifest.json` 的工作副本，用 `scripts/check_evidence.py` 验证证据并生成原尺寸拼图。实际打开查看拼图，结合参考人工判定结构和涂装；脚本通过只说明它检查的项目通过。

## 交付与边界

- 输出结构／材质卡、可编辑资产、两轮审查记录、实际运行画面、测试与预算、尚未验证项。手工参数模型不等于官方 CAD 或实测复刻。
- 模型的剪影、腰线、开口和支撑应由几何成立，不能靠黑贴纸或堆面数掩盖；低档仍保留辨识点。
- 参考照片和品牌车型不因私人使用而获得公开再分发许可。默认保存来源记录，不把参考照片打包进可分发工具包；保留已有代码许可，公开发布另核权利。
- 本技能不授权 push、烧录、公开发布或重写无关功能。按当前用户要求决定提交；无设备不得把主机测试写成真机帧率验收。
