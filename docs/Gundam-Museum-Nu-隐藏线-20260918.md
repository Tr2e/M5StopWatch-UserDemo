# Gundam Museum · 牛高达隐藏线、RX-78 恢复着色

## 范围

用户要求 RX-78 恢复分面着色，把隐藏线改到牛高达；强袭与扎古、沙扎比、命运一样暂时不进入浏览。几何、比例、站姿和装备未改。

## 实现

- `ModelId::NuGundam` 走隐藏线：先填房间纸色 `0xdefb` 和深度，再描去重可见边，线色 `0x4208`。
- RX-78 回到原来的纯色填面路径，拖动仍是 65% 采样、松手后 180ms 回到 100%。
- 牛高达拖动时 65% 填深度，再放大到 424 描 1px 边；松手立即 100%。
- 浏览：RX-78 → 牛高达。强袭源文件保留，主机测试仍可构建。

## 自检

`SANITIZE=1 bash tools/test_gundam_museum.sh /tmp/gundam-nu-wireframe-20260918 rx78`

`SANITIZE=1 bash tools/test_gundam_museum.sh /tmp/gundam-nu-wireframe-20260918 nu`
