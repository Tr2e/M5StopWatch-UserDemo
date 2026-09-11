# M5StopWatch-UserDemo
M5Stack StopWatch user demo for hardware evaluation.

## App Development

- Racer 当前实现与优化资产：[性能与流畅度优化资产总览](docs/Lets-And-Go-Racer-性能与流畅度优化资产总览.md)；交互与 UI 阶段记录：[R27 交互与 UI 优化计划](docs/Lets-And-Go-Racer-R27-交互与UI优化计划.md)；操作与设备检查：[使用与真机验收](docs/Lets-And-Go-Racer-使用与真机验收.md)。
- [外设输入接入与故障排查规范](docs/外设输入接入与故障排查规范.md)：Joystick2 / Dual Button 的供电、采样、慢帧与总线故障处理、新 App 接入和验收入口。
- [科技游戏外设接线设计](docs/科技游戏外设接线设计.md)：现有外设接线与电气检查。

## Build

### Fetch Dependencies

```bash
python3 ./fetch_repos.py
```

### Tool Chains

[ESP-IDF v5.5.4](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32s3/index.html)

### Build

```bash
idf.py build
```

### Flash

```bash
idf.py flash
```
