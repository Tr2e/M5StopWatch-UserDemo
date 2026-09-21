# Gundam Museum P0 真机基线

- 日期：2026-09-21
- 设备：M5StopWatch / ESP32-S3 rev 0.2 / 8 MiB PSRAM
- USB VID:PID：`303A:1001`
- USB 序列号与芯片 MAC：`44:1B:F6:C1:8A:00`
- 源码基线：`1f11597`
- ESP-IDF：5.5.4
- 显示总线：80 MHz

## 方法

临时设备基准固件在 HAL 初始化后停止 LVGL，绕过 Launcher 和人工输入，自动渲染 RX-78 与 Nu Gundam。每个模型分别测量 65% 和 100% 内部采样，每档先执行一帧暖机，再渲染 24 个确定性 yaw/pitch 组合。

每帧都使用生产 `MuseumRenderer`、整屏背景、显示 framebuffer 事务和真实 AMOLED 提交。`fps_x10` 使用 24 帧完整 wall time 计算，包含 draw 与 present。

这是固定角度设备基准，用于可重复 A/B；不代替最终触摸手感验收。

## 结果

| 模型 | 采样 | FPS | draw | present | peak | clear | cull | project+raster | blit |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| RX-78 | 65% | 6.2 | 148,717 us | 11,607 us | 172,132 us | 21,405 us | 4,019 us | 88,144 us | 35,028 us |
| RX-78 | 100% | 4.5 | 206,579 us | 11,606 us | 239,284 us | 27,421 us | 4,022 us | 146,093 us | 28,931 us |
| Nu | 65% | 1.3 | 708,329 us | 11,659 us | 777,384 us | 23,495 us | 4,548 us | 651,178 us | 28,971 us |
| Nu | 100% | 1.3 | 748,683 us | 11,662 us | 824,708 us | 29,445 us | 4,551 us | 685,551 us | 29,004 us |

### 内存

- `working_bytes=1,306,908`
- Museum 打开后 internal free：267,035 B
- 基准期间 internal minimum：262,975 B
- 结束时 internal largest block：188,416 B
- Museum 打开后 PSRAM free：6,050,072 B
- 基准期间 PSRAM minimum：5,984,532 B
- 结束时 PSRAM largest block：6,029,312 B

53 KiB 的 `424×32` RGB565+深度条带在当前 internal RAM 余量下有可行性，但仍必须保留分配失败回退和至少 32 KiB 安全余量。

## 结论

1. `present_us` 稳定在约 11.6 ms，不是当前首要瓶颈；P3 dirty rect 提交延后。
2. RX-78 的首要瓶颈是 `project_raster_us`，65% 约 88 ms，100% 约 146 ms。
3. Nu 的隐藏线路径远高于 RX-78，65% 仍需约 651 ms；降低填充分辨率几乎没有降低总耗时，说明原生尺寸隐藏线描边/边处理是独立主瓶颈。
4. P1 仍应推进条带化，但必须分成两条 A/B：RX-78 的实体填充条带，以及 Nu 的隐藏线面/边候选裁剪。

## 构建与烧录证据

- 临时 benchmark app 大小：1,379,840 B。
- 临时 benchmark BIN SHA-256：`47b5c9f8610c8797869c1848a62d17e4c44dd2fd76e02884b40c2a24da494b60`
- 启动 ELF SHA-256 前缀：`8490b3d38`
- PSRAM 自检：通过。
- 仅烧录应用分区，写后 Hash 校验通过；设置/存档分区未改动。
- 临时自动 benchmark 入口在数据采集后已从源码移除；正常产品固件已重建、刷回并确认 Launcher 正常启动。

## 主机回归

- `tools/test_gundam_museum.sh`：通过，864 个几何/剪裁/局部刷新用例完成。
- `tools/test_gundam_museum_perf.sh`：首次重跑在 Nu、90%、完整装备、`pitch=0.7`、`yaw_index=6` 定位到 3 个隐藏线像素不一致。根因是拖动降采样后将低分辨率投影坐标直接缩放复用，浮点舍入在深度边界抹掉了孤立墨线像素。修复为原生分辨率描边前使投影缓存失效并重算；修复后 `pixel_identical_cases=6720`。
- 性能测试在不一致时现在会输出模型、采样、装备、姿态、首个差异位置及全部差异像素，便于后续条带边界回归定位。
- `SANITIZE=1 tools/test_gundam_museum.sh`：通过，864 个用例。
- `SANITIZE=1 tools/test_gundam_museum_perf.sh`：通过，`pixel_identical_cases=6720`，未见 ASan/UBSan 错误。
- 修复后正常产品 BIN 大小：4,982,000 B；SHA-256：`e683cb41b0febc91fec5d836c70a2692e72eb92bc6bfc8383d877ca4cd7d3c59`；已刷入同一设备并通过写后 Hash 校验。
