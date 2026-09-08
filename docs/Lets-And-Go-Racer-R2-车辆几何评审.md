# Let's & Go!! Racer R2：官方车辆几何与图标评审

## 官方参考基线

本轮以田宫官方产品资料冻结车名、尺寸、配色和主要轮廓：

- [Cyclone Magnum Premium](https://www.tamiya.com/japan/products/19440/index.html)：155×97×40 mm，紧凑前后轮罩、大型后翼、白底红蓝图形、绿色轮毂。
- [Hurricane Sonic Premium](https://www.tamiya.com/japan/products/19441/index.html)：155×97×40 mm，连接左右前轮罩的翼面、三段小翼片后翼、红绿图形、黄色轮毂。
- [Neo Tridagger ZMC](https://www.tamiya.com/japan/products/19409/index.html)：132×90×46 mm，低矮平面化车壳、前轮盖和火焰图形。
- [Brocken Gigant Premium](https://www.tamiya.com/english/products/19452/index.html)：156×97×38 mm，前置马达、夹持轮胎而非覆盖轮胎的翼子板、管状侧护架。

这些几何是面向 466×466 圆屏的实时手绘线框重建，不是通用 3D 模型或照片贴图。

## 交付内容

- 四车官方名称、短名、尺寸、RGB565 配色和性能预设。
- 统一 7 截面车壳坐标系，但四车拥有独立宽度/高度/车鼻轮廓。
- 固定容量、无每帧堆分配的 `CarWireframe`。
- Race LOD：普通车型 50 条线，Hurricane Sonic 56 条。
- Showcase LOD：普通车型 126 条线，Hurricane Sonic 138 条。
- 高细节模式包含 8 段轮胎环、导轮、尾翼支架和更密车壳线。
- 田宫红蓝双星 + `TAMIYA` 字标的 200×200 RGB565 Launcher 图标。
- 可重现的图标生成脚本和 PNG 预览。

## Self-review A：正确性与架构

### 发现与修正

1. Cyclone Magnum 初始数据误用了 150 mm 纪念版数值，与本轮选定的 Premium AR 车型不一致。已根据田宫 Item 19440 修正为 155×97×40 mm。
2. 初始 `addLine()` 在容量不足时只返回，可能静默丢失尾翼。已新增 `overflowed` 诊断位并在测试中强制为 false。
3. Hurricane Sonic 的三段翼使初版达到 144 条线且超出计划的 140 条审查上限。已将小尺寸导轮从 6 段环简化为 4 段菱形，保留特征同时将总数收敛至 138。
4. 车辆几何层不依赖 HAL、M5GFX 或 LVGL，后续动画只消费线框快照。

### 验证

- 严格 Host 编译：通过。
- ASan + UBSan：通过。
- 车名、精确尺寸、ID 唯一性、几何有限性、坐标边界、LOD 上下限和辨识特征测试：通过。
- R1 流程状态机回归：通过。

## Self-review B：性能、资产与集成

### 发现与修正

1. 四车几何在栈上返回定长快照，后续渲染器应缓存它而不是每条线使用动态容器。
2. 田宫 RGB565 图标固定增加 80,000 字节，仍在 R0 容量门禁内；没有引入车辆照片或大尺寸游戏贴图。
3. 图标生成前后 SHA-1 一致，PNG 和 C 数组在当前工具链上可重现。
4. App 已从临时 Vector Run 图标切换为 `icon_lets_and_go`，不留存临时资产。

### 验证

- ESP-IDF 固件编译：通过。
- Let's & Go!! 新增编译警告：0。
- 固件由 R1 `0x47d960` 增加至 `0x491200`，R2 增量 `0x138a0`（80,032）字节。
- App 分区剩余 `0x5ee00`（388,608）字节（8%）。

## R2 结论

R2 通过两轮自审、严格 Host 测试、Sanitizer、资产重现检查和完整固件编译，可阶段提交并进入 R3。
