# RX-78 无损 fast path 真机证据（2026-09-21）

硬件：ESP32-S3，240 MHz，8 MiB 80 MHz Octal PSRAM，CO5300 468×466 framebuffer 面板。  
基线 commit：`ffc6717`；测试分支：`feat/gundam-arena`。  
每个真机 A/B 档位交错运行 24 帧，姿态序列相同；65% 和 100% 分开统计。

## 接受项

### Solid scanline span + trusted depth

实体三角形逐行用原重心公式确认左右边界，保证内部覆盖后省略重复 coverage 判断；三角形级验证逆深度范围后，内部像素省略有限值、正值和 clamp 检查。

| 分辨率 | 参考 raster | 候选 raster | 参考 draw | 候选 draw |
| --- | ---: | ---: | ---: | ---: |
| 65% | 88.187 ms | 85.166 ms | 150.463 ms | 147.260 ms |
| 100% | 142.947 ms | 132.828 ms | 205.211 ms | 194.822 ms |

结论：raster 分别降低 3.4%/7.1%，整帧 draw 分别降低 2.1%/5.1%。

### Native framebuffer composite

为 CPU 可寻址 framebuffer 增加可选行访问与脏区标记接口。仅在 RGB565、rotation 0、完整 clip 和真实 framebuffer 均成立时，光栅器直接合成到 framebuffer 行；其他设备和布局回退原 span API。面板存储为 byte-swapped RGB565，写入前显式转换。

同姿态整屏 FNV-1a 哈希：

- 65%：reference `548123638`，native `548123638`，exact=true。
- 100%：reference `3299443075`，native `3299443075`，exact=true。

| 分辨率 | 参考 blit | native blit | 参考 draw | native draw | 含 present 周期 | 估算 FPS |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 65% | 37.340 ms | 12.920 ms | 149.370 ms | 124.626 ms | 136.472 ms | 7.33 |
| 100% | 24.426 ms | 13.452 ms | 190.424 ms | 179.575 ms | 191.547 ms | 5.22 |

结论：65% blit 降低 65.4%、draw 降低 16.6%；100% blit 降低 44.9%、draw 降低 5.7%。这是本轮最大且逐字节等价的单项收益。

### Framebuffer span transaction

在 native framebuffer 路径出现前，受保护的 `startWrite/endWrite + setAddrWindow/writePixels` 使 blit 稳定减少约 1.1–1.2 ms，总 draw 改善约 0.7–0.8%。保留为 native 条件不满足时的回退优化。

## 否决项

- 交错 `[depth16|color16]`：100% blit 29.72→23.84 ms，但 32-bit 清屏抵消收益；65% draw 143.25→151.63 ms，100% 194.58→199.69 ms。
- 最近邻重复行缓存：bitset 扫描和分支抵消读取节省，真机无稳定收益。
- 直接重关联深度平面：6,720-case 中出现 1 个 framebuffer 像素差，否决。
- 带量化边界回退的深度平面：6,720-case 精确一致，但 RX-78 主机 65% 从约 950 µs 退化到 1,062 µs，未刷真机。
- 保守 early-Z 深度平面：像素一致，但真机 raster 在 65%/100% 分别慢 4.7%/5.6%。
- 8-bit 索引色平面：110 色、无溢出，但 65% draw 150.114→154.953 ms，100% 188.431→202.495 ms。
- 无像素中心面片跳过：整屏哈希一致；65% draw 125.339→125.047 ms（噪声级），100% 180.248→180.499 ms（略慢）。

## 正确性与剩余差距

- `tools/test_gundam_museum_perf.sh`：`pixel_identical_cases=6720`。
- `tools/test_gundam_museum.sh`：几何、构图、控制和重入通过。
- ASan/UBSan 两套测试通过。
- 65% 当前约 7.3 FPS，距离 15 FPS 的 66.7 ms 周期仍约差 69.8 ms；当前主要剩余项是约 85 ms 的 project+raster，而不是约 13 ms 的 blit。

下一阶段应围绕实际覆盖像素和 overdraw 建立计数，验证可保持等深语义的 front-to-back/HZB 组合，或设计专用的实体四边形/凸多边形光栅内核；不再重复已否决的单项 early-Z、索引色和 strip 路线。
