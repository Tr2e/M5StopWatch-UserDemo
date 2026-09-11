# Let's & Go Racer 音频资产归档

状态：2026-09-11 起，赛车 App 暂停 BGM 与音频反馈。这里保存清理前可工作的原创 8-bit 合成器、HAL 流封装和主机测试，供以后重新评估；该目录位于 `docs/assets`，不会被 `main/CMakeLists.txt` 的 App 源码扫描纳入固件。

## 归档内容

- `runtime/chip_synth.cpp/.h`：车库、比赛、最后一圈和结算 BGM，以及 14 类提示音的实时合成器。
- `runtime/racer_audio.cpp/.h`：HAL 单所有者音频流的打开、关闭、场景切换和回调封装。
- `tests/lets_and_go_audio_test.cpp`：波形、场景、提示音、分块一致性与试听 WAV 生成测试。
- `tests/lets_and_go_audio_lifecycle_test.cpp`：不可用、竞争流、重复关闭与并发生命周期测试。
- `host/hal/hal.h`：生命周期测试使用的最小 HAL 替身。
- 同期试听文件仍保存在相邻目录：`../lets-and-go-r20-audio-preview.wav`。

当前生产代码不包含 `RacerAudio`、`ChipSynth`、`MusicScene` 或 `SoundCue` 的引用，默认 `tools/test_lets_and_go.sh` 也不再编译本归档。比赛流程、物理、渲染和输入逻辑不依赖这些文件。游戏仍保留独立振动反馈；系统启动音效及其他 App 的音频代码没有变更。

## 手工验证归档

归档代码不属于日常回归。如需确认资产仍可独立构建，可在仓库根目录执行：

```sh
c++ -std=c++17 -Wall -Wextra -Werror -pedantic docs/assets/lets-and-go-audio-archive/tests/lets_and_go_audio_test.cpp docs/assets/lets-and-go-audio-archive/runtime/chip_synth.cpp -o /tmp/lets-go-audio-test
/tmp/lets-go-audio-test /tmp/lets-go-audio-preview.wav

c++ -std=c++17 -Wall -Wextra -Werror -pedantic -pthread -Idocs/assets/lets-and-go-audio-archive/host docs/assets/lets-and-go-audio-archive/tests/lets_and_go_audio_lifecycle_test.cpp docs/assets/lets-and-go-audio-archive/runtime/racer_audio.cpp docs/assets/lets-and-go-audio-archive/runtime/chip_synth.cpp -o /tmp/lets-go-audio-lifecycle-test
/tmp/lets-go-audio-lifecycle-test
```

## 恢复约束

恢复时应把运行代码放回赛车 App 源码目录，重新接入 App 打开、场景切换、提示触发和关闭生命周期，并恢复主机回归。随后必须在 StopWatch 真机核对音频回调耗时、比赛帧率、断续、音量、退出尾音和与系统单次提示音的抢占关系，不能把归档测试通过视为真机验收完成。
