# 参考模型任务

在本仓库新建或精修参考驱动的 3D 展示模型（如四驱车、高达、机械模型）时，读取并使用 `skills/reference-model-workshop/SKILL.md`。完整制作按其 `references/pipeline.md` 分节点制作、检查和记录；局部修正从受影响节点进入。普通代码任务不必加载该流程。

节点自检自主进行，用户视觉验收独立记录，不以编译或测试通过替代。各子目录的 `AGENTS.md` 仍按其适用范围执行。

# ThunderRaster RX-78 性能任务

当任务涉及 ThunderRaster、RX-78 60% benchmark、微带/行带光栅或继续当前 3D 性能优化时，动代码前必须按顺序完整阅读：

1. `docs/ThunderRaster-RX78-P3-交接与执行清单.md`
2. `docs/ThunderRaster-P3-RX78-60-percent.md`
3. `docs/RX78-60-percent-performance-plan.md` 的 E3–E9
4. `docs/ESP32-S3-高性能3D渲染框架提炼与模型接入规范.md`

当前实现分支是 `perf/thunderraster-rx60-p3`，代码锚点是 `ecc0068`。先核对分支、构建开关、真机路径标志和同分支基线，再开始候选；不得把 Museum 65%、100%、阶段 1–3 或旧 32 行条带数据混入 RX-78 60% P3 的性能结论。未通过交接清单中的像素、内存和真机端到端门槛，不得将候选写成已接受的框架能力。
