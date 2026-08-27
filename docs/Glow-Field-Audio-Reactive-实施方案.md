# Glow Field Audio Reactive 实施方案

## 1. 文档状态

- 目标分支：`feat/glow-field`
- 方案状态：待实施
- 适用设备：ESP32-S3 StopWatch，逻辑显示尺寸 468×466，8MB PSRAM
- 依赖能力：现有 `Audio.FFT` 实时麦克风频谱、Glow Field 点阵引擎与脏带渲染器
- 视觉原则：沿用现有 SymbolMix 元素和四色板，不在本阶段调整颜色

本方案为 Glow Field 增加第三种交互模式 `Audio Reactive`。该模式使用设备麦克风的实时频谱驱动点阵，使画面表现音乐能量、节奏冲击和频段变化；现有 Ripple、Paint、颜色选择和外观选择能力保持可用且行为不回归。

## 2. 目标与非目标

### 2.1 目标

1. 直接复用 HAL 已提供的 20 段实时频谱和主频数据，不复制或分叉 FFT 实现。
2. 将音乐的持续能量、低频冲击、中频结构和高频瞬态映射为不同层次的点阵效果。
3. Audio Reactive 作为 Ripple、Paint 之外的正式第三模式，可通过现有模式按键进入。
4. 音乐响应与触摸事件可以叠加：音乐负责持续场，触摸保留即时、明确的人工反馈。
5. 保持 SymbolMix 的三角、圆、叉、方块以及电光青、荧光洋红、质子绿、亮黄四色板不变。
6. 在真机上维持 30FPS 目标，连续运行无看门狗复位、内存增长或明显音画迟滞。
7. 所有音频只在设备本地实时处理，不保存 PCM、不录音落盘、不上传数据。

### 2.2 非目标

- 不做音乐识别、歌曲识别、歌词、节拍 BPM 精确测量或音高调音器。
- 不承诺低于 FFT 分辨率的精密频率测量。
- 不增加新的配色设置，也不在本阶段改变 SymbolMix 现有颜色。
- 不改造 `Audio.FFT` 的现有视觉界面。
- 不在首版增加多套 Audio Reactive 预设，先完成一套可调参数的主效果。
- 不引入新的第三方 DSP 库；继续使用项目已有 ESP-DSP 和 HAL 音频链路。

## 3. 当前基础能力

### 3.1 Audio.FFT 数据链路

现有 HAL 已完成：

```text
ES8311 麦克风输入
  -> 44.1kHz / 16-bit / 单声道
  -> 512 点采样窗，256 点 Hop，50% 重叠
  -> 去直流 + Hann 窗
  -> ESP-DSP 浮点 FFT
  -> 20 个对数频段
  -> 自适应噪声底、动态归一化、Attack/Release 平滑
  -> bands[20]（0.0～1.0）+ peakFrequencyHz
```

基础时域参数：

| 参数 | 数值 | 意义 |
| --- | ---: | --- |
| 采样率 | 44100Hz | 麦克风输入采样率 |
| FFT 长度 | 512 samples | 约 11.6ms 分析窗口 |
| Hop 长度 | 256 samples | 约 5.8ms 数据步长 |
| 频率分辨率 | 约 86.1Hz | 相邻 FFT bin 间隔 |
| 频段数 | 20 | 对数分布，输出已归一化 |

HAL 对外接口已经足够支撑首版：

```cpp
GetHAL().updateAudioSpectrum();
const auto& spectrum = GetHAL().getAudioSpectrum();
```

### 3.2 Glow Field 基础

当前 Glow Field 已具备：

- 392 个左右的蜂窝点阵，最多支持 512 点；
- 独立 `energy` 与 `rippleEnergy` 两种能量来源；
- Paint、Ripple、触摸插值和随机 K1 触发；
- 最多 6 个并发 Ripple；
- SymbolMix 四元素、四色板和按事件变化的符号身份；
- 16 档视觉能量与预计算调色板；
- 30FPS 渲染节流、19px 脏带重绘和局部面板提交；
- 颜色选择态、外观选择态和尺寸调整。

音频能力应作为第三种能量来源接入，而不是持续伪造触摸事件。这样可以保留音乐的连续结构，并避免音频每帧触发大量 Ripple 导致活动槽位饱和。

## 4. 用户体验定义

### 4.1 模式入口

短按 B/K2 按以下顺序循环：

```text
Ripple -> Paint -> Audio Reactive -> Ripple
```

进入 Audio Reactive 后显示约 900ms 的音频模式提示。提示应使用小型频谱/声波符号，不引入常驻文字，以维持当前沉浸式界面。

### 4.2 Audio Reactive 中的交互

| 操作 | 行为 |
| --- | --- |
| 环境音乐/声音 | 持续驱动点阵能量和频段结构 |
| 低频冲击 | 触发有节制的中心或近中心扩散波 |
| 高频瞬态 | 触发少量、短促的局部亮点 |
| 手指点击 | 在音乐场之上叠加一次原有手指 Ripple，保持稳定元素渐隐 |
| 短按 A/K1 | 随机位置触发一次手动强调 Ripple；SymbolMix 下沿用 K1 变形变色规则 |
| 长按 A/K1 | 进入或退出外观选择态 |
| 长按 B/K2 | 进入或退出颜色选择态 |
| A+B 长按 | 退出应用 |

Audio Reactive 中不启用连续 Paint 手势。手指按下按 Ripple 处理，保证音乐背景之上仍有清晰、可预期的点击反馈。

### 4.3 选择态处理

- 进入颜色选择态或外观选择态时，音频采样继续运行，防止 I2S 数据积压和重新进入时读取旧数据。
- 音频视觉层在 180～260ms 内柔和降到暗态，避免选择 UI 被持续音乐覆盖。
- 退出选择态后重置 onset 历史和自适应阈值，在约 200ms 内淡入，避免恢复瞬间误触发强 Ripple。
- SymbolMix 继续忽略用户色相；其他形状将所选色相作为音频配色的基准色。

## 5. 总体架构

```text
HAL AudioSpectrumFrame
  bands[20] + peakFrequencyHz
            |
            v
AudioReactiveController（新增）
  - 时间归一化平滑
  - 4 组频段能量
  - 总能量 / 频谱质心
  - Spectral Flux / onset 检测
  - 事件节流和随机种子
            |
            v
AudioReactiveFrame（纯数据）
            |
            +----------------------+
            |                      |
            v                      v
Engine 音频持续层            Engine 音频事件层
audioEnergy per dot          audio ripple / sparks
            |                      |
            +----------+-----------+
                       v
Renderer 能量仲裁
max(touch energy, ripple energy, audio energy)
                       |
                       v
现有 16 档 Glow + SymbolMix / 普通形状绘制
```

职责边界：

- HAL 只负责音频采样和频谱，不加入 Glow Field 视觉语义。
- `AudioReactiveController` 只将频谱转换成稳定的视觉特征，不直接绘图。
- `Engine` 决定哪些点被点亮、何时触发波纹以及能量如何衰减。
- `Renderer` 只根据最终点状态绘制，不运行 FFT 或节拍算法。
- `AppGlowField` 管理模式、输入、生命周期和调度。

## 6. 音频特征设计

### 6.1 频段聚合

20 段频谱按现有输出顺序聚合为四组。首版使用每组 5 段，并保留组内低序号轻微加权：

| 视觉组 | 原始 Band | 大致范围 | 主要用途 |
| --- | --- | --- | --- |
| Bass | 0～4 | 86～517Hz | 呼吸、冲击、主 Ripple |
| Low Mid | 5～9 | 517Hz～1.38kHz | 点阵主体密度和中近场运动 |
| Mid | 10～14 | 1.38～5.51kHz | 元素活跃度、局部形状变化 |
| Treble | 15～19 | 5.51～22.05kHz | 稀疏火花、边缘细节 |

麦克风高频响应和实际外壳声学会影响最后一组；真机调参时允许将 Treble 上限有效截断到 Band 17 或 18，但不改变 HAL 的20段输出。

### 6.2 时间平滑

HAL 已有频段平滑，Glow Field 仍需要面向视觉的包络，但必须使用基于 `dt` 的时间常数，不能使用依赖循环帧率的固定系数：

```text
alpha = 1 - exp(-dt / tau)
```

建议初值：

| 特征 | Attack | Release |
| --- | ---: | ---: |
| Bass | 28ms | 210ms |
| Low Mid | 35ms | 180ms |
| Mid | 24ms | 145ms |
| Treble | 14ms | 95ms |
| Overall Energy | 40ms | 240ms |

低频保留较长尾部以形成重量感，高频快速熄灭以保持轻盈。参数必须集中定义，不散落在 App、Engine 和 Renderer 中。

### 6.3 总能量与频谱质心

```text
overall = 0.38*bass + 0.28*lowMid + 0.22*mid + 0.12*treble
centroid = sum(bandEnergy[i] * i) / sum(bandEnergy[i])
```

- `overall` 控制场景的整体活跃比例，但不直接让全部点同时变亮。
- `centroid` 控制活跃区域的空间倾向和颜色角色选择，使低沉与明亮的声音具有不同构图。
- `peakFrequencyHz` 首版仅用于串口诊断，不直接控制动画，避免安静环境下主频跳动污染画面。

### 6.4 Onset/冲击检测

使用正向 Spectral Flux，不做 BPM 识别：

```text
flux = weightedSum(max(currentBand[i] - previousBand[i], 0))
threshold = movingMean(flux) + sensitivity * movingDeviation(flux)
onset = flux > threshold && bass/lowMid energy > minimum && cooldown expired
```

建议初值：

- 历史窗口：约 450～650ms；
- 最短冲击间隔：160ms；
- 普通冲击 Ripple：最大每秒 4 次；
- 强冲击可提高半径和峰值，但不额外占用多个 Ripple 槽；
- 从暂停/选择态恢复后的前 200ms 禁止触发 onset。

阈值使用自适应历史而不是固定音量值，因为 HAL 频段已经动态归一化，不同环境下绝对数值不具备稳定含义。

## 7. 视觉映射

### 7.1 持续音频场

每个 Dot 在 `reset()` 时获得稳定的音频身份：

- `audioBandIndex`：0～19，使用点位坐标哈希确定并加入径向偏置；
- `audioGroupIndex`：Bass、Low Mid、Mid、Treble；
- `audioPhase`：用于空间波动和离散阈值；
- 身份在一次应用会话内稳定，不逐帧随机。

空间分布建议：

- Bass 偏向中心与内圈，但不形成硬圆盘；
- Low Mid 分布最广，构成主要视觉体积；
- Mid 以分散簇和斜向区域出现；
- Treble 偏向外圈及稀疏点位；
- 通过坐标哈希加入约 25%～35% 扰动，避免看成四层固定均衡器。

点能量由对应原始频段、四组能量、总体能量和稳定阈值共同决定。低能量时只激活少数点，强音乐才逐步提高点亮比例，不能让 392 个点持续同步呼吸。

### 7.2 低频 Ripple

- onset 触发点位位于中心附近 0～48px 范围，强冲击更靠近中心。
- 波纹强度由 Bass、Low Mid 和 flux 共同确定。
- 音频 Ripple 使用独立入口，持续时间建议 900～1250ms，比手指 Ripple 更克制。
- 最大同时活动音频 Ripple 建议 3 个；与现有总 Ripple 上限共同受控。
- 相邻冲击不重复触发完全相同的中心和颜色角色。
- 不使用单一硬圆环，继续沿用当前逐点传播、散射和余辉逻辑。

### 7.3 中频结构

- Low Mid 决定点亮簇的覆盖规模。
- Mid 决定 SymbolMix 活跃点的形状变化概率。
- 形状变化只发生在已被音频点亮的元素中，且每点最短变化周期建议 90～140ms。
- 不逐帧全场随机换形，避免“电视雪花”感和过多脏区。

### 7.4 高频火花

- Treble 上升沿超过阈值时生成 1～3 个局部亮点。
- 火花持续 80～180ms，不生成完整全屏 Ripple。
- 最短发射间隔 65～90ms。
- 亮黄优先用于瞬态高光，但单帧黄色活跃点比例应受上限控制。
- 高频持续存在时降低重复概率，只有新的上升沿增加火花，避免常亮。

### 7.5 配色策略

本阶段不修改调色板，仅定义角色：

| 声音角色 | SymbolMix 主色倾向 |
| --- | --- |
| Bass 核心 | 荧光洋红，少量亮黄峰值 |
| Low Mid 主体 | 质子绿与荧光洋红 |
| Mid 结构 | 质子绿与电光青 |
| Treble/外缘 | 电光青，亮黄用于短促火花 |

普通 Star、Hexagon、Circle、Triangle 使用所选色相作为基色，根据频段只调整已有色相槽偏移，不新增白色或改变饱和度方案。

## 8. 数据结构与接口调整

### 8.1 新增音频控制器

建议新增：

```text
main/apps/app_glow_field/glow_field_audio_reactive.h
main/apps/app_glow_field/glow_field_audio_reactive.cpp
```

核心数据接口：

```cpp
struct AudioReactiveFrame {
    std::array<float, 20> bands;
    std::array<float, 4> groups;
    float overallEnergy;
    float spectralCentroid;
    float onsetStrength;
    float trebleTransient;
    bool onset;
    uint32_t sequence;
};

class AudioReactiveController {
public:
    void reset(uint32_t nowMs);
    AudioReactiveFrame update(const std::array<float, 20>& bands,
                              float peakFrequencyHz,
                              uint32_t nowMs);
    void setVisualPaused(bool paused, uint32_t nowMs);
};
```

Controller 不保存 PCM，只保存上一帧频段、包络、flux 统计和冷却状态。

### 8.2 Engine 音频能量层

为 `Dot` 增加独立状态，建议控制在每点 4～8 字节：

```cpp
uint8_t audioEnergy;
uint8_t audioColorIndex;
uint8_t audioSymbolIndex;
uint8_t audioBandIndex;
bool audioUsesSymbolPalette;
```

Engine 新接口：

```cpp
void applyAudioFrame(const AudioReactiveFrame& frame,
                     uint32_t nowMs,
                     bool symbolMix);
void triggerAudioRipple(int x, int y, uint32_t nowMs,
                        uint8_t strength, bool symbolMix);
void clearAudioReaction(bool immediate);
```

音频持续层与触摸 `energy`、波纹 `rippleEnergy` 分离。退出 Audio Reactive 时只清理 `audioEnergy`，不得清掉仍在衰减的手指 Ripple。

### 8.3 Renderer 仲裁

将当前两路能量仲裁：

```text
max(dot.energy, dot.rippleEnergy)
```

扩展为三路：

```text
max(dot.energy, dot.rippleEnergy, dot.audioEnergy)
```

来源优先级用于相同能量时决定符号和颜色：

```text
手指/K1 Ripple > 手指 Paint > 音频 Ripple > 音频持续层 > 暗态身份
```

`visualKeyForDot()` 必须包含音频来源的颜色和符号，否则能量相同但颜色/形状变化时脏带不会被标记。

### 8.4 模式类型

当前 `InteractionMode` 是 `AppGlowField` 的私有枚举，Renderer 只接收 `bool rippleMode`。接入第三模式后应改为共享强类型枚举：

```cpp
enum class InteractionMode : uint8_t {
    Ripple,
    Paint,
    AudioReactive,
};
```

Renderer 接收枚举并按模式绘制提示，避免继续增加多个布尔参数。

## 9. 调度与生命周期

### 9.1 `onOpen()`

- 创建并 reset `AudioReactiveController`；
- Engine 建立点阵时生成稳定音频身份；
- 默认模式仍保持当前 Ripple，不自动开启麦克风视觉；
- 不额外创建大块 PCM 缓冲。

### 9.2 `onRunning()`

Audio Reactive 激活时的建议顺序：

```text
1. 更新按钮
2. 读取一个 HAL spectrum hop
3. Controller 更新特征
4. 处理触摸和模式状态
5. Engine applyAudioFrame / 触发受控事件
6. Engine 固定步长模拟
7. Renderer 以 30FPS 节流绘制
```

`updateAudioSpectrum()` 当前会同步读取 256 个采样，约占 5.8ms。Audio Reactive 中应在每个主循环调用一次以持续消费 I2S 数据，不按 30FPS 采样，否则可能积累旧数据并增加音画延迟。Renderer 仍独立限制为约 30FPS。

若真机证明同步采样导致帧率或触摸延迟不达标，再将频谱更新移动到 HAL 内的专用低优先级任务，并使用双缓冲/序号发布 `AudioSpectrumFrame`。首版不预先引入并发改造，以降低对所有音频功能的风险。

### 9.3 模式切换

- 进入 Audio Reactive：结束 Paint touch、重置 Controller 历史、音频层淡入。
- 离开 Audio Reactive：停止生成音频事件，音频持续层在约 220ms 内衰减。
- 切换期间现有手指 Ripple 自然结束，不整场清屏。
- 进入颜色/外观选择态：暂停音频视觉输出但继续消费音频数据。

### 9.4 `onClose()`

- 停止 Audio Reactive 输出；
- 清空 Controller 和音频 Dot 状态；
- 不关闭全局 AudioCodec，因为其生命周期属于 HAL；
- 按现有流程恢复 LVGL 和显示提交。

## 10. 性能预算与保护策略

### 10.1 预算

| 项目 | 目标 |
| --- | ---: |
| 稳定渲染 | 30FPS，10秒窗口不低于 28FPS |
| FFT Hop 读取 | 约 5.8ms，异常最大值需记录 |
| Controller 特征计算 | 平均 <0.3ms |
| Engine 音频映射 | 平均 <0.8ms / simulation step |
| Interactive 渲染 | 平均 <26ms，常态最大帧 <50ms |
| 感知音画延迟 | 强冲击到首个视觉响应不超过 80ms |
| 额外常驻内存 | <8KB，不含已有 HAL FFT 缓冲 |

### 10.2 事件限流

- 总 Ripple 数继续受 `kMaxRipples = 6` 限制；
- 音频专用活动 Ripple 最多 3 个；
- onset 冷却不短于 160ms；
- 高频火花每次最多 3 点，间隔不短于 65ms；
- 音频符号变化周期不短于 90ms；
- 同一音频帧只提交一次 Engine target，不因主循环追赶重复触发事件。

### 10.3 脏区控制

- 音频能量继续量化为 16 档，档位未变化的 Dot 不标脏；
- 连续场使用稳定点位阈值，避免每帧随机选择全新点集合；
- 形状和颜色仅在活跃点、事件点上变化；
- 若单帧超过约 70% 的脏带，可评估一次全屏提交与多脏带提交的实际成本，选择更快路径；
- 统计实际脏带数量，不能只看 FPS 判断瓶颈。

### 10.4 降级策略

性能不足时按以下顺序降级，不先减少点阵数量：

1. 降低音频持续层的活跃点比例；
2. 降低符号变化频率和高频火花数；
3. 将音频视觉更新限制到 50～60Hz，但保持 I2S Hop 持续消费；
4. 合并相邻脏带或在高覆盖时切换全屏提交；
5. 最后才考虑将 Renderer 限制到 28FPS。

## 11. 日志与可观测性

每 10 秒输出一次 Audio Reactive 统计：

```text
[GlowField.Audio] fps=30.0 renderAvg=xx.xxms renderMax=xx.xxms
audioFps=xxx.x audioAvg=x.xxms audioMax=x.xxms
onsets=n sparks=n dirtyBandsAvg=x.x bass=x.xx overall=x.xx
```

调试构建可额外输出四组频段值和自适应 onset 阈值；正式版本不逐帧打印，避免串口影响性能。

需要区分以下时间：

- HAL 采样/FFT 时间；
- Controller 特征提取时间；
- Engine 音频映射时间；
- Renderer 几何绘制和面板提交时间。

## 12. 实施阶段

### Phase A：基线与数据接口

1. 在当前固件记录 Audio.FFT 的频谱更新速率和单次耗时。
2. 记录 Glow Field Interactive 当前 FPS、平均/最大帧时和脏带数量。
3. 新增 `AudioReactiveController` 和 `AudioReactiveFrame`，暂不接渲染。
4. 使用合成频段序列验证分组、时间平滑、flux、冷却和暂停恢复。

通过条件：Controller 对相同输入产生确定结果；静音不会持续产生 onset；不同 `dt` 下包络时间基本一致。

### Phase B：第三模式与持续音频层

1. 将模式枚举扩展为 Ripple、Paint、Audio Reactive。
2. 增加 Engine `audioEnergy` 和稳定音频身份。
3. 接入四组频段到持续点阵场。
4. 扩展 Renderer 三路能量仲裁和 visual key。
5. 完成模式提示、进入/退出淡入淡出。

通过条件：播放低频、中频、高频内容时能观察到不同空间结构；现有 Ripple/Paint 行为不变。

### Phase C：事件层与触摸叠加

1. 加入自适应 onset 检测。
2. 加入受控低频音频 Ripple。
3. 加入中频符号变化和高频火花。
4. Audio Reactive 中接入手指 Ripple 和 K1 随机强调。
5. 完成选择态暂停视觉、恢复抑制误触发。

通过条件：节拍明确但不机械地每帧触发；手指响应不会被音乐吞没；安静时画面稳定。

### Phase D：性能与真机调参

1. 采集安静环境、手机外放、近距离音箱和人声四类输入。
2. 调整噪声门、包络时间、onset 阈值、Ripple 冷却和火花密度。
3. 采集 10 秒窗口性能和强音乐连续 15 分钟稳定性。
4. 如同步频谱更新不达标，实施 HAL 后台 spectrum worker；否则保持简单架构。
5. 检查播放按键音、Boot SFX 或其他 AudioCodec 播放时的锁竞争和恢复行为。

通过条件：达到第 13 节验收标准，无崩溃、WDT 或持续数据滞后。

### Phase E：文档与阶段提交

1. 更新 Glow Field 总体方案的模式、按键、性能数据和实现状态。
2. 写明 Audio Reactive 参数及真机最终值。
3. 完成代码格式检查、完整编译、烧录和启动日志验证。
4. 创建阶段 commit，并在用户确认后 push。

## 13. 验收标准

### 13.1 功能验收

- B/K2 可稳定循环进入三个模式，提示与实际模式一致。
- Audio Reactive 能对持续音乐产生连续场，对明显节拍产生离散冲击。
- 低频、中频和高频输入的画面具有可辨识差异。
- Audio Reactive 中触摸点击与 K1 强调均正常工作。
- 颜色选择和外观选择可进入、退出，恢复后不产生误爆发。
- SymbolMix 使用既定四色；其他形状继续遵循当前所选色相。
- 退出 Audio Reactive 后不存在残留音频能量或后台视觉事件。

### 13.2 视觉验收

- 安静环境 2 秒后，音频活跃点不超过可见点约 5%，且不持续全场闪烁。
- 低频冲击是多点传播和自然余辉，不退化为单一硬圆环。
- 高频表现轻、短、稀疏，不形成大片白/黄噪点。
- 音乐持续响起时，不出现全部元素同步缩放的“呼吸灯”效果。
- SymbolMix 换色换形具有节制，同一帧不发生全场随机重排。
- 手指 Ripple 的视觉优先级高于音乐背景，点击位置清楚。

### 13.3 性能与稳定性验收

- 10 秒稳定窗口平均达到约 30FPS，最低窗口不低于 28FPS。
- 普通运行平均渲染时间低于 26ms，除首帧/模式全刷外最大帧尽量低于 50ms。
- 强冲击到视觉响应不超过约 80ms，无明显音画拖尾。
- 强音乐连续运行 15 分钟，无崩溃、看门狗复位、PSRAM 分配失败或持续内存增长。
- 从 Audio Reactive 切回 Ripple/Paint 后，触摸采样和动画手感不劣化。
- 退出应用后 LVGL、Launcher 和其他 App 正常运行。

### 13.4 回归验收

- Ripple 点击、K1 随机 Ripple、Paint 点击和 K1 变形变色保持现有行为。
- SymbolMix 默认模式、四色比例、Ripple 黄色参与策略不回归。
- 颜色选择色环和外观尺寸调整不出现新增卡顿。
- Launcher 图片图标继续正常加载。
- `Audio.FFT` App 本身显示和主频数值不受影响。

## 14. 风险与对策

| 风险 | 影响 | 对策 |
| --- | --- | --- |
| 同步 I2S 读取阻塞主循环 | 触摸或渲染迟滞 | 首版测量；不达标再转 HAL 后台任务和双缓冲 |
| HAL 自动归一化导致持续音乐过满 | 全场常亮、节拍不明显 | Controller 使用相对 flux、活跃比例阈值和事件冷却 |
| 高频频段受麦克风/外壳影响 | 火花过少或噪声过多 | 真机调整有效高频范围与门限，不改 HAL 20 段接口 |
| 音频变化导致所有脏带持续更新 | FPS 下降 | 16 档量化、稳定点位身份、高覆盖时比较全屏提交 |
| Ripple 槽被快速节拍占满 | 新冲击替换旧波纹 | 音频活动上限 3、最短 160ms 冷却、强度合并 |
| 按键音播放与频谱读取竞争 | 短暂断帧 | 保持 try-lock 跳过；恢复时不补发旧 onset |
| 恢复选择态后出现假节拍 | 视觉突然爆发 | reset flux 历史，200ms onset 抑制，音频层淡入 |
| 逐帧换色换形产生脏乱感 | 视觉噪声和性能下降 | 只改变活跃点，最短变化周期 90～140ms，稳定随机种子 |

## 15. 文件级改动清单

| 文件 | 计划改动 |
| --- | --- |
| `main/apps/app_glow_field/glow_field_audio_reactive.h/.cpp` | 新增频段聚合、包络、flux、onset 和视觉帧数据 |
| `main/apps/app_glow_field/app_glow_field.h/.cpp` | 第三模式、音频调度、生命周期、选择态暂停与触摸叠加 |
| `main/apps/app_glow_field/glow_field_engine.h/.cpp` | Dot 音频能量层、稳定音频身份、持续场和音频事件接口 |
| `main/apps/app_glow_field/glow_field_renderer.h/.cpp` | 三路能量仲裁、Audio 模式提示、音频 visual key 和统计 |
| `main/hal/hal_audio.cpp` | 首版只增加必要统计；后台任务仅在性能不达标时实施 |
| `docs/Glow-Field-实施方案.md` | 实施完成后同步模式、按键、真机数据和状态 |

## 16. 最终实施决策

1. Audio Reactive 是正式第三模式，不替代 Ripple 或 Paint。
2. 复用 HAL 的 20 段频谱；不在 Glow Field 内重复 FFT。
3. 为 Dot 增加独立 `audioEnergy`，保证音乐持续场与手指事件可组合。
4. 节拍检测采用自适应 Spectral Flux，不做 BPM 识别。
5. 音频响应分为持续场、低频 Ripple、中频结构和高频火花四层。
6. SymbolMix 配色保持不变；本阶段只调整颜色的使用角色，不调整颜色值。
7. 首版使用当前同步 HAL 接口并用真机数据决策是否后台化，避免无证据扩张底层改造。
8. 性能降级优先减少音频活跃度和变化频率，不减少现有星星数量。

## 17. V2：理想音乐能量响应升级方案

### 17.1 升级目标

首版已经建立持续音频场、低频 Ripple、中频换形和高频火花，但其输入门限建立在自动归一化后的频谱上，无法表达真实输入强度；点位采用独立随机阈值，正常音乐下可见密度偏低；单一 onset 也不足以区分低频节拍、中频重音和高频瞬态。

V2 将体验目标明确为“音乐播放器式律动反应器”，内部仍保持音乐能量响应的技术定位：

```text
真实输入门控决定是否进入音乐状态
  + 平滑频谱形成持续律动场
  + 快速频谱形成 Bass / Mid / Treble 瞬态
  + 慢速包络表达音乐段落与高潮
```

麦克风输入仍会响应环境中的人声和其他声音；本阶段不做音乐分类。若未来需要只响应设备内部播放内容，应直接接入播放器 PCM，而不是在麦克风链路增加分类器。

### 17.2 双阈值真实音量门控

HAL 在自动频谱归一化之前增加宽带电平测量：

```cpp
float inputRmsDbfs;
float noiseFloorDbfs;
float signalToNoiseDb;
float signalConfidence;
bool signalActive;
```

门控使用 dBFS 与相对噪声底共同判断，并带打开/关闭迟滞：

| 状态 | 初始条件 | 保持时间 |
| --- | --- | ---: |
| 打开 | `input > -50dBFS` 且 `SNR > 8dB` | 60ms |
| 保持 | `SNR >= 4dB` 且 `input >= -58dBFS` | 持续 |
| 关闭 | 任一保持条件不满足 | 380ms |

首次进入检测后先用 `450ms` 建立环境噪声基线；门打开期间冻结噪声底向上学习，避免持续音乐被误吸收到噪声模型。

这些是首轮真机参数，不代表声压级。最终值必须结合麦克风30dB输入增益、设备外壳和典型播放距离校准。

噪声底只在信号关闭或输入接近当前噪声底时更新：下降较快、上升很慢，防止持续音乐被误学习为噪声。`signalConfidence` 在开关门之间连续变化，用于视觉淡入淡出，避免布尔门导致画面突变。

### 17.3 双频谱输出

HAL 同时发布两种频谱：

```cpp
bands[20]           // 已平滑，驱动持续能量场
transientBands[20]  // 噪声过滤并归一化，但未做视觉平滑
```

- `bands` 保持现有 Audio.FFT 的稳定观感；
- `transientBands` 只用于 Spectral Flux 和瞬态检测；
- 信号门关闭时，`transientBands` 立即归零，`bands` 按原 Release 自然下降；
- 不复制 FFT，也不增加第二次 FFT 运算。

### 17.4 三条时间尺度

#### 快速层：10～220ms

- Bass 正向 flux：触发主要 Ripple；
- Mid 正向 flux：触发局部 Bloom 和短时换形窗口；
- Treble 正向 flux：触发2～5个相邻元素构成的微型火花簇；
- 三类瞬态分别维护自适应均值、偏差和冷却，不共享一个阈值。

#### 律动层：180～600ms

- `grooveEnergy` 使用约65ms Attack / 360ms Release；
- Bass onset 注入一次 `groovePulse`，约280ms 衰减；
- Groove 控制局部 Glow 强度、活动密度和流动速度，不做全场同步缩放；
- 正常音乐下保持可见点约22%～35%，强节奏下达到40%～55%。

#### 段落层：0.8～3秒

- `slowEnergy` 使用约280ms Attack / 1400ms Release；
- 控制活动区域规模、高潮密度上限和中频换形参与率；
- 音乐停止后由真实门控触发退场，保留约300～500ms余韵后回到暗态。

### 17.5 分频瞬态检测

分别计算三组正向 flux：

| 检测器 | Band | 角色 | 最短间隔 |
| --- | --- | --- | ---: |
| Bass onset | 0～5 | Kick、低频重拍、大 Ripple | 145ms |
| Mid accent | 5～14 | Snare、旋律重音、局部 Bloom | 110ms |
| Treble hit | 14～19 | Hi-hat、齿音、火花簇 | 65ms |

每个检测器使用约550ms历史窗口：

```text
threshold = max(minThreshold, mean + sensitivity * deviation)
```

事件还必须满足 `signalActive` 和最低 `signalConfidence`。短时估计不称为 BPM；只维护 onset 间隔的稳定度，为连续 Groove 提供节拍相位参考。

### 17.6 可控活跃密度

不再仅依赖每点 `0.12～0.70` 的固定阈值，而是先由音乐状态确定目标密度，再用稳定点位哈希选出参与者：

| 状态 | 目标密度 |
| --- | ---: |
| 门控关闭 | 0% |
| 刚进入/很弱音乐 | 8%～15% |
| 正常音乐 | 22%～35% |
| 强节奏/高潮 | 40%～55% |
| 瞬时峰值 | 上限60% |

点位仍按自己的20段频谱身份获得强弱，目标密度只保证画面规模。参与身份通过坐标哈希和缓慢移动的整数流动相位决定，不逐帧完全随机，因此画面会流动但不会产生电视雪花感。

### 17.7 V2 视觉角色

| 音乐信息 | 效果 |
| --- | --- |
| Signal Confidence | 整体音乐层淡入淡出 |
| Overall / Groove | 点位密度、Glow强度、流动速度 |
| Slow Energy | 活动区域规模与高潮参与率 |
| Bass onset | 中心附近主 Ripple，保持多点传播与余辉 |
| Mid accent | 4～9点局部 Bloom，开启180～320ms换形窗口 |
| Treble hit | 2～5点相邻火花簇，80～180ms快速消失 |
| Spectral Centroid | 流动方向与径向重心 |

颜色值保持现状。SymbolMix 中洋红承担低频核心，绿/青承担中高频结构，亮黄只做受控瞬态高光。

### 17.8 性能约束

- FFT仍为单次512点，不增加重复频域计算；
- HAL新增的 RMS、SNR 和 transientBands 在现有 FFT 循环内完成；
- Controller 每个新频谱帧只处理20个值；
- Engine 连续场映射限制在最多60Hz，事件检测仍消费每个有效频谱帧；
- 活跃密度最高60%，高覆盖时继续使用现有全脏带快速路径；
- 不减少现有点阵数量；性能不足时优先降低 Bloom/火花密度和换形频率。

### 17.9 V2 分步实施

1. HAL：加入宽带 RMS、噪声底、SNR、迟滞门和 transientBands；
2. Controller：加入信号状态、三类瞬态、Groove/Slow包络；
3. Engine：加入目标密度、流动相位、Mid Bloom和Treble微簇；
4. Renderer：复用现有三路能量仲裁，不增加新的逐像素混合；
5. 合成数据：验证静音、持续低频、Bass脉冲、Mid重音和Treble脉冲；
6. 真机：记录 dBFS/SNR、事件数量、活跃比例、FPS和脏带；
7. 调参：依次校准门控、密度、瞬态，再调整视觉强度，避免同时改变所有参数。

### 17.10 V2 验收

- 安静环境门控稳定关闭，10秒内不产生音乐 Ripple/Bloom/火花；
- 正常距离播放音乐后120ms内进入有效视觉状态；
- 音乐停止后约0.4～0.8秒自然退回暗态；
- 正常音乐可见点比例约22%～35%，高潮不超过60%；
- Kick、Snare/重音和Hi-hat在画面上具有不同的空间尺度；
- 强节拍到首个视觉响应目标不超过60ms；
- 10秒窗口不低于28FPS，连续15分钟无崩溃或WDT；
- Ripple、Paint、颜色选择、外观选择和Audio.FFT不回归。
