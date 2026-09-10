# StopWatch 高性能渲染与 App 优化规范

> 日期：2026-09-11
>
> 状态：设计基线
>
> 参考实现：Vector Canyon Fighter（Vector Run）
>
> 适用范围：StopWatch UserDemo 的 Launcher、动态图形 App、游戏、仪表盘及普通 LVGL 页面

## 1. 目标

建立一套可复用的高性能运行框架，使不同 App 不再各自实现帧率控制、屏幕提交、性能统计和降级策略。

主要目标：

- 减少 framebuffer 重复复制和重复屏幕事务。
- 保证动画速度、物理速度不随帧率变化。
- 避免慢帧形成无法恢复的逻辑积压。
- 在持续超出预算时有序降低非关键画质。
- 避免运行时频繁分配、内存碎片和不可控的 PSRAM 访问。
- 为真机性能判断提供统一、可复现的数据。
- 同时支持高速直绘 App 和 LVGL 系统页面。

本规范不要求所有 App 放弃 LVGL，也不把“降低分辨率”作为默认方案。优化必须先定位瓶颈，再选择最小且可验证的改动。

## 2. Vector Run 的现有性能基线

Vector Run 当前使用以下关键策略：

- 显示节拍约为 30 Hz，`kFrameIntervalMs = 33`。
- 仿真采用固定 60 Hz 步长，`kSimulationStepSeconds = 1 / 60`。
- 单轮最多追赶 5 个仿真步；超过上限则清除积压。
- 直接绘制到设备已有 framebuffer，不创建第二张全屏 Sprite。
- 一帧只建立一次 `startWrite()` / `endWrite()` 显示事务。
- 使用固定容量数组保存地形、投影点和事件窗口。
- 每 5 秒统计实际 FPS、平均/最大渲染耗时和仿真 clamp 次数。
- 使用 `High / Medium / Low` 三级渲染预算，并以滞回规则防止画质来回跳变。

当前 Vector Run 真机完整画面基线约为：

- 巡航：21.4–21.7 FPS，平均渲染 43–44 ms。
- 满 Boost：20.7–20.9 FPS，平均渲染约 45 ms。
- 正常运行无仿真积压，栈水位稳定。

这些数据是当前 Vector Run 内容复杂度下的真机基线，不应直接作为所有 App 的统一目标。简单页面应达到更高帧率或进入事件驱动的静止状态。

## 3. 核心架构

推荐统一流程：

```text
异步输入采样
      ↓
主循环读取输入快照
      ↓
固定步长逻辑更新（可选）
      ↓
性能预算选择画质等级
      ↓
单一 framebuffer 绘制
      ↓
一次显示事务提交
      ↓
记录 draw / present / FPS / backlog
```

框架分为五个相互独立的组件。

### 3.1 `PerformanceFrameLoop`

负责帧调度和固定步长逻辑，不包含具体 App 业务。

职责：

- 使用单调毫秒时钟计算经过时间。
- 控制目标显示帧率，时间未到时立即返回。
- 使用 accumulator 执行固定步长逻辑。
- 限制单轮最大追赶次数。
- 统计慢帧、逻辑积压和丢弃的累计时间。
- App 暂停、恢复、打开和关闭时显式 reset。

推荐默认值：

- 动态直绘 App：30 FPS 显示、60 Hz 逻辑。
- 轻量动画 UI：30 FPS；动画停止后转为事件驱动。
- 静态页面：仅在状态变化时重绘。
- 单轮最多追赶 4–5 个逻辑步。

不能使用阻塞式 delay 控制动画速度。动画位置必须由经过时间或固定逻辑步决定。

### 3.2 `DisplayFrameScope`

负责显示事务，确保一帧只有一次提交边界。

建议使用 RAII 接口：

```cpp
class DisplayFrameScope {
public:
    explicit DisplayFrameScope(Display& display) : _display(display) {
        _display.startWrite();
    }
    ~DisplayFrameScope() {
        _display.endWrite();
    }
private:
    Display& _display;
};
```

使用约束：

- App 内部绘制函数不能自行重复提交整屏。
- 禁止在已有全屏 framebuffer 的情况下再创建同尺寸 Sprite 后整屏复制。
- Renderer 只负责绘制；显示事务由最外层帧函数持有。
- 异常返回、校准页和暂停页也必须正确关闭事务。

### 3.3 `RenderBudgetController`

根据持续性能而非单个尖峰选择画质等级。

Vector Run 的已验证规则：

- 初始为 `High`。
- 连续两个 5 秒窗口低于 19 FPS、平均渲染超过 50 ms，或多次发生仿真积压时，降为 `Medium`。
- `Medium` 下连续两个窗口低于 16 FPS、平均渲染超过 60 ms，或发生严重积压时，降为 `Low`。
- 连续四个窗口达到至少 20.5 FPS、平均渲染不高于 47 ms且无积压时，只恢复一级。
- 单次 Wi-Fi、日志或系统任务造成的尖峰不能触发降级。

不同 App 可以提供自己的阈值，但必须保留以下原则：

- 降级看连续窗口，不能看单帧。
- 恢复比降级慢。
- 每次只改变一级。
- 先删除装饰，再降低次要细节，最后才考虑分辨率。
- 核心交互、文字、告警和主体轮廓在所有等级下保持完整。

### 3.4 `PerformanceMetrics`

统一采集以下指标：

- `fps_x10`：实际成功呈现帧率。
- `draw_us`：软件绘制耗时。
- `present_us`：framebuffer 到屏幕的提交耗时。
- `frame_peak_us`：窗口内最大完整帧耗时。
- `simulation_clamps`：逻辑追赶达到上限的次数。
- `quality`：当前画质等级。
- `free_internal`、`largest_internal`：内部 RAM 状态。
- `stack_watermark`：任务剩余栈水位。
- 当前画布尺寸、内部渲染尺寸和实际回退模式。

日志应按 2–5 秒窗口输出，禁止逐帧打印。测量绘制与传屏必须分段，否则无法判断继续优化 CPU 算法还是显示链路。

### 3.5 `QualityProfile`

每个 App 定义可降级内容，而不是让公共框架猜测业务语义。

推荐顺序：

1. 关闭粒子、辉光、阴影和装饰线。
2. 降低远景、背景或非交互对象密度。
3. 减少动画采样或更新频率。
4. 降低动态场景内部渲染分辨率，HUD 和文字保持原生分辨率。
5. 最后才降低整体帧率。

## 4. 数据与内存策略

### 4.1 固定容量优先

- 热路径优先使用 `std::array`、对象池和预分配缓存。
- `open()` 时完成必要分配，`close()` 时统一释放。
- `render()` 中禁止容器扩容和频繁 `new/delete`。
- 使用 `static_assert` 锁定关键对象和缓存的内存预算。

Vector Run 已采用紧凑的 `int16_t x/y` 投影缓存，并以编译期断言限制 Renderer 和地形模型大小。

### 4.2 增量计算

- 只更新进入可见窗口的新数据。
- 复用上一帧仍有效的几何、布局、文本宽度和投影结果。
- 先做粗粒度可见性判断，再执行投影、裁剪和像素绘制。
- 远景使用稀疏采样，近景保留结构精度。

### 4.3 内存区域选择

- 高频随机访问、小型行缓冲和深度缓冲优先内部 RAM。
- 大型、顺序访问、低频缓存可以放入 PSRAM。
- 所有优先分配必须有明确回退路径，并在日志中报告实际分配结果。
- 不能仅根据配置开关宣称优化生效，必须记录实际画布尺寸和内存位置。

## 5. 输入与渲染解耦

I²C、GPIO、触摸和 RGB 写入不能阻塞主渲染循环。

推荐模型：

- 后台任务以固定周期采样外设。
- 连续轴使用最新快照。
- 按键边沿和菜单方向使用小型事件邮箱。
- 主线程只读取定长原子状态，不同步等待 I²C。
- 总线失败后重新对齐采样节拍，不能密集追赶过期任务。
- App 关闭时先停止采样任务，再释放总线和 Grove 供电。

这样可以避免外设瞬时超时直接转化为画面卡顿，也能防止低帧率页面漏掉短按操作。

## 6. 两种 App 渲染模式

### 6.1 高速直绘模式

适用于：

- 游戏
- 实时仪表盘
- 高频动画
- 大量自定义线条或像素绘制

要求：

- 使用 `PerformanceFrameLoop`。
- 使用单一 framebuffer 和一次事务提交。
- Renderer 不依赖 LVGL 对象树。
- 支持至少两档可验证的画质预算。
- 具备生命周期 reset 和性能窗口日志。

### 6.2 LVGL UI 模式

适用于：

- Launcher
- 设置、表单和列表
- 文本为主的页面
- 需要标准控件、布局和触摸事件的页面

LVGL 页面不应机械改成全屏游戏直绘，应采用：

- 保留 LVGL 脏区域刷新。
- 动画过程中只更新真正变化的属性。
- 避免同一帧同时触发布局、文本测量和多个控件重绘。
- 缓存图标和固定文本，避免切换时重新解码或创建对象。
- 滚动结束后停止周期重绘，切回事件驱动。
- 合并 framebuffer 提交，避免控件级重复传屏。
- 对 Launcher 的图标缩放、标签、分页指示器分别计时，定位主要开销。

Launcher 卡顿的优化重点是 LVGL 动画和失效区域，而不是复制 Vector Run 的游戏循环。

## 7. UserDemo 公共接口建议

建议新增公共目录：

```text
main/apps/common/performance/
├── performance_frame_loop.h
├── display_frame_scope.h
├── performance_metrics.h
├── render_budget_controller.h
└── quality_profile.h
```

App 最小接入形式：

```cpp
void ExampleApp::onRunning()
{
    const uint32_t now = GetHAL().millis();
    const auto plan = _frameLoop.begin(now);

    _input.updateSnapshot(now);

    for (uint8_t i = 0; i < plan.simulationSteps; ++i) {
        _model.step(_input.state(), plan.fixedStepSeconds);
    }

    if (!plan.shouldRender) return;

    const auto drawStart = GetHAL().micros();
    {
        DisplayFrameScope frame(display);
        _renderer.render(_model, _budget.detail());
    }
    _metrics.observeFrame(GetHAL().micros() - drawStart,
                          plan.clampedSimulation);
    _budget.observe(_metrics.completedWindow());
}
```

实际实现应进一步把 `draw_us` 和 `present_us` 分开测量；示例只表达组件关系。

## 8. 迁移顺序

建议分阶段迁移，避免一次修改整个 UserDemo 显示架构。

### 阶段 A：抽取 Vector Run 公共能力

- 抽取帧循环、显示事务、指标统计和通用三级预算状态机。
- 保持 Vector Run 像素输出和真机性能不变。
- 为时间回绕、慢帧、积压、降级和恢复增加 Host 测试。

### 阶段 B：选择第二个动态 App 验证复用

- 接入一个结构较简单的动画 App。
- 比较迁移前后的 draw、present、FPS、内存和画面一致性。
- 验证 App 反复打开/关闭后没有资源泄漏。

### 阶段 C：优化 Launcher

- 为滚动动画建立分段耗时。
- 检查图标对象数量、重复副本、标签刷新和分页指示器刷新。
- 缩小失效区域，避免无变化时更新时钟之外的控件。
- 保留 LVGL 交互语义和无限滚动行为。

### 阶段 D：推广到其他 App

- 高频动画 App 使用直绘模式。
- 普通页面使用 LVGL 事件驱动模式。
- 将统一性能日志纳入每个 App 的真机验收。

## 9. 验收门禁

每个迁移 App 至少满足：

- 功能、输入和像素语义没有非预期变化。
- 一帧最多一次整屏显示事务。
- 热路径无未经预算的动态分配。
- 慢帧后逻辑不会无限追赶。
- 性能日志能区分绘制、传屏和输入耗时。
- 记录真实 HAL 尺寸、实际内部渲染尺寸及回退模式。
- 连续运行和反复开关 App 后栈水位与可用内存稳定。
- Wi-Fi、外设断连或偶发慢帧不会触发画质闪变。
- High/Medium/Low 都保留核心内容和可操作性。
- 所有结论以同设备、同场景、同构建参数的真机数据为准。

推荐目标：

- Launcher 滚动期间达到稳定、均匀的视觉节拍，不出现明显长帧。
- 简单动画页面以 30 FPS 为优先目标。
- 复杂直绘页面首先保证固定逻辑节拍和稳定帧时间，再依据内容复杂度设定 FPS 门限。
- 静态页面在无状态变化时不持续整屏重绘。

## 10. 常见反模式

必须避免：

- 已有 framebuffer 时再维护第二张全屏 Sprite。
- 在每个绘制组件内部独立调用屏幕提交。
- 用 `delay()` 驱动动画或物理速度。
- 每帧打印日志、创建字符串或扩容容器。
- 在渲染主线程同步等待 I²C 外设。
- 只测总帧时间，不区分 draw 和 present。
- 因单个尖峰立即切换画质。
- 为追求 FPS 优先删除核心主体、HUD 或交互反馈。
- 用主机基准代替真机结果。
- 配置要求半分辨率，却不核对设备实际是否进入该路径。

## 11. 现有参考代码

- 帧循环与固定步长：`main/apps/app_vector_canyon_fighter/app_vector_canyon_fighter.cpp`
- 单 framebuffer 显示事务：`main/apps/app_vector_canyon_fighter/vector_canyon_renderer.cpp`
- 自适应渲染预算：`main/apps/app_vector_canyon_fighter/render_budget_controller.h`
- 固定容量投影缓存：`main/apps/app_vector_canyon_fighter/vector_canyon_renderer.h`
- 增量地形窗口：`main/apps/app_vector_canyon_fighter/model/explicit_canyon_stream.h`
- 真机性能门禁：`docs/Vector-Canyon-Fighter-G4-H9-高速性能与视觉降级门禁.md`

## 12. 结论

Vector Run 的流畅度不是来自单一绘图技巧，而是来自完整闭环：稳定帧调度、固定逻辑步、单次显示提交、固定内存、增量数据、输入解耦、持续性能观测和有滞回的画质降级。

UserDemo 应将这些能力抽成公共基础设施，同时保留两种渲染模式：动态图形使用高速直绘，系统 UI 使用经过优化的 LVGL。最终目标不是让所有 App 使用同一种 Renderer，而是让所有 App 遵守同一套帧预算、资源生命周期和真机验收规范。
