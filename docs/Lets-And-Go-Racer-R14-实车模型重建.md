# R14：四款实车特征模型重建

回应“模型粗糙、外形尽量贴近真机”。只修改车辆外观、展示镜头和渲染预算，不改变 R13 的动力学、AI、输入、三圈规则与追逐平衡。

## 官方实物依据

通过浏览器实际查看田宫照片，沿用首版目录的具体套件版本。

| 车型 / 官方来源 | 重建特征 |
| --- | --- |
| [Cyclone Magnum Premium / 19440](https://www.tamiya.com/english/products/19440/index.html) | 白蓝尖鼻、独立前轮罩、黑色长座舱、后轮罩红色闪电、蓝色单尾翼、绿色五辐轮圈 |
| [Hurricane Sonic Premium / 19441](https://www.tamiya.com/english/products/19441/index.html) | 白红车壳、连接前轮罩的前翼、绿色侧缘、后轮罩短肋、红色单尾翼、金色轮圈 |
| [Neo Tridagger ZMC / 19409](https://www.tamiya.com/japan/products/19409/index.html) | 深炭灰棱面车壳、铜色座舱、前轮银色封盖、后轮红色轮圈、红黄火焰、高位尾翼与中央短肋 |
| [Brocken Gigant Premium / 19452](https://www.tamiya.com/japan/products/19452/index.html) | 红壳黑纹、蓝色风挡、前置电机散热肋、银色侧管、金色轮圈、尾部短立柱，无大型悬空尾翼 |

R2 的 Sonic“三层悬空尾翼”和 R12 的 Brocken 深灰主体属于旧版误读，本轮纠正。尺寸目录保留田宫数据；手工几何重在部件布局和辨识轮廓，不宣称官方 CAD 等比例复刻。小字贴纸、曲面连续性仍有简化。

## 第一轮自审：结构与视觉正确性

- 四车原来共用七段船形外壳，轮胎无厚度。重建底盘、保险杠、胎面/侧壁/轮圈、导轮、轮罩、座舱、进气口和翼架。
- 原模型把全部线条叠在色块上，远侧结构透出。改为部件面片排序、随面片绘制铅笔边缘，保留纸色背景和克制色块。
- 检查生产截图，补齐 loft 端部斜肩封口，缩窄整块底板，调整三分之四展示角度与放大倍率。
- 初始 512 面预算截断了 Brocken 末尾部件。改为固定 576 面并增加四车容量断言，不接受静默丢面。

## 第二轮自审：遮挡、失败路径与负载

- 风挡仍会因平均深度排序被车壳覆盖。为风挡和后轮罩贴花增加底面归属，随底面绘制，再由更近实体遮挡；测试父面索引合法性。
- 修正非法车型编号回退时几何是 Magnum、导轮却保留错误配色的问题。
- 仅轮圈辐条旋转，胎面轮廓和侧管不再被误判为车轮；测试旋转半径不变、车身不动、Tridagger 前轮封盖无辐条。
- 风线改在车体之前绘制，避免穿过放大的车身。
- High / Medium / Low 轮胎分别为 10 / 8 / 6 段；低档导轮减至 4 段，省略短装饰边线但保留完整色块。车型或档位改变才重建缓存，不在每帧分配或生成模型。
- 比赛按独立鼻锥、座舱、轮罩、导轮和尾部结构重建紧凑线框，保留桥面逆深度遮挡，不复用数百面的展示模型。

## 预算与验证

| 车型 | 展示 High 面片 | 比赛线段 | 工具用精细线框 |
| --- | ---: | ---: | ---: |
| Magnum | 479 | 108 | 148 |
| Sonic | 510 | 112 | 158 |
| Tridagger | 487 | 108 | 148 |
| Brocken | 523 | 108 | 148 |

- 展示上限 576 面，比赛每车 128 线。大缓存保存在 renderer 实例内，不在渲染调用栈创建。
- 主机实例大小：GarageRenderer 39,872、RaceRenderer 35,272 bytes；相对 R13 合计增加 40,000 bytes。固件可构建不代表运行堆余量已验证。
- 28 组 ASan/UBSan 全通过：12 游戏、15 Vector Run、Launcher。
- 保留 384 场策略比赛、64 组长跑、8000 组裁剪。四车型策略胜场与 R13 一致。
- 生产渲染额外覆盖四车 × 五个展示时刻 × 三档细节，检查圆屏文字边界和缓存切换，输出四车独立放大截图。
- 最终 ESP-IDF 构建通过：镜像 0x49d1f0，分区余量 0x52e10（7%），未更新依赖。

## 生产截图与复验

[改动前](assets/lets-and-go-r14-before.png) · [改动后](assets/lets-and-go-r14-after.png)

PNG 来自生产 C++ 渲染器的主机像素输出，不是概念图或田宫照片贴图。

```sh
SANITIZE=1 bash tools/test_lets_and_go.sh
bash tools/render_lets_and_go.sh /tmp/lets-go-r14-review
python3 tools/lets_and_go_contact_sheet.py /tmp/lets-go-r14-review /tmp/lets-go-r14-review/cars.png --names car-0 car-1 car-2 car-3 showcase-0 race-0
source ../esp-idf/export.sh
idf.py build
```

未烧录、推送或公开发布。AMOLED 观感、High 档帧率、热功耗、空闲堆和栈水位仍需真机验证。
