# 三机眼窝逐台优化

用户授权“依次推进优化”。顺序沙扎比 → ν → RX-78，每台完成两轮读图、主机测试、目标构建及提交后再改下一台。本轮不烧录。保留用户认可的比例、姿势、装备和大眼风格；问题基线见 [专项检查](Gundam-Museum-三机眼部专项检查-20260915.md)，基线提交 `2822bb7`。

## 1. 沙扎比 SDEX017

参考沿用专项检查已实际打开的万代同款正/斜/背商品图，另实际查看 [同款实物头部正斜近照](https://gunplapocchi.com/wp-content/uploads/2020/12/SDEX-SAZABI-11.jpg)（[原作者评测](https://gunplapocchi.com/sdex-sazabi/)）。实物贴纸说明颜色和轮廓，不将贴纸叠层照搬成三维镜片。图像支持斜窗中央容纳单眼、两端变窄；下述单位均为建模参数，不是官方尺寸。

- CR1：旧黑带等宽 `.037`，单眼凸于眉缘。改为中央高 `.085`、中段 `.0525`、末端 `.020` 的曲面斜窗；窗口后壁退 `.070`，单眼前端从 `.577` 退到 `.490`，后端 `.465` 插入后座，中心高度 `2.452`，镜片前半径 `.035`。保留三红角和额头摄像窗的独立身份。
- CR1 返修：第一版下眼框叠到旧面具上缘，正斜图出现重复折线；接边后非共面四边形又产生交替三角明暗。改成沿眼窗边界汇入中央脊的三角扇，下面具用左右两个平面薄壳，共享 `(0,2.37,.58)` 与两侧挂点。原颈柱、面具后梁及背板保留，未靠前移眼睛遮丑。中间版先看 `r1/r3` 正斜侧，最终看 `v4` 全套。
- CR2：实际查看最终正/斜/侧、另一侧斜视、俯/仰眼区、头颈装配图；全部11张原生诊断图与48帧完整装备转台。核对单眼位于上下边界内、侧面不再凸出眉檐、下眼框到尖面具连续、头颈支撑、身体装备与圆屏留白。100%/65%均可辨单眼；正侧视逐渐被眉甲遮挡属于嵌入后的正常遮挡。此次没有宣称真机视觉/FPS通过。
- 技术：ASan/UBSan 864组通过；眼窗中段射线、单眼中心及十个周界采样均检查完整头部最前面；单眼前移、黑窗前移受控负例被检出。既有头/面具断连负例、枪盾/炮阵/拳壳零异常相交通过。最终4059面（旧4057），容量4096、工作区1274184字节不变；内部/埋面/局部刷新差异0，剔除边缘累计23、单例最大4，完整装备圆屏/视口越界0。

最终证据：`docs/assets/gundam-eye-refine-20260915/sazabi/`，包含前后对照、原生图、结构图、相机、测试结果和48帧GIF。中间 CR1 图为临时诊断，最终档案只收当前生产资产输出。

证据清单检查通过；`sazabi.cpp` SHA-256：`b8f9c36279f40a7080d0bd5eeb8549031b654933ca61e6156061a67e641c3dec`。最终 `idf.py build` 通过，应用大小 `0x3d31e0`，分区余量23%。

## 重现命令

每次采用新的临时目录；MODEL依次为sazabi、nu、rx78，只有已完成章节对应的证据算完成。

```sh
bash tools/render_gundam_reference.sh STUDY MODEL
SANITIZE=1 bash tools/test_gundam_museum.sh NATIVE MODEL
python3 tools/package_gundam_evidence.py NATIVE EVIDENCE --model MODEL --studies STUDY --baseline docs/assets/gundam-eye-audit-20260915/MODEL
python3 skills/reference-model-workshop/scripts/check_evidence.py --manifest EVIDENCE/manifest.json --frames EVIDENCE --output NEW_CHECK_DIR
```

目标构建使用项目 ESP-IDF 环境的 `idf.py build`；设备验收与烧录另行授权。
