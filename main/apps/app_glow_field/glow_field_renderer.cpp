#include "glow_field_renderer.h"

#include <algorithm>
#include <esp_timer.h>
#include <hal/hal.h>
#include <mooncake_log.h>

namespace glow_field {
namespace {

constexpr uint32_t kFrameIntervalMs = 33;
constexpr int kDirtyBandHeight = 19;
constexpr int kMaxDotRadius = 12;

template <typename Target>
void drawStar(Target& canvas, int x, int y, int radius, int waist, uint16_t color)
{
    canvas.fillTriangle(x, y - radius, x - waist, y, x + waist, y, color);
    canvas.fillTriangle(x, y + radius, x - waist, y, x + waist, y, color);
    canvas.fillTriangle(x - radius, y, x, y - waist, x, y + waist, color);
    canvas.fillTriangle(x + radius, y, x, y - waist, x, y + waist, color);
}

uint8_t scaleChannel(uint8_t channel, uint8_t level)
{
    return static_cast<uint8_t>((static_cast<uint16_t>(channel) * level) / 255u);
}

const char* sceneName(RenderScene scene)
{
    switch (scene) {
        case RenderScene::Interactive: return "interactive";
        case RenderScene::Solid: return "solid";
        case RenderScene::IdleDots: return "idle-dots";
        case RenderScene::EnergizedDots: return "energized-dots";
    }
    return "unknown";
}

}  // namespace

void Renderer::open(int width, int height, std::size_t dotCount)
{
    _width = width;
    _height = height;
    _dotCount = dotCount;
    _statsStartedMs = 0;
    _frameCount = 0;
    _frameTimeTotalUs = 0;
    _maxFrameTimeUs = 0;
    _lastFrameMs = 0;
    _sceneInitialized = false;
    _fullFrameReady = false;
    _displayedLevels = {};
    _displayedLevelsValid = false;
    _displayedHint = 0;

    auto& display = GetHAL().getDisplay();
    // The AMOLED driver already owns a PSRAM framebuffer. Keep automatic
    // full-screen commits disabled while Glow Field is active so Interactive
    // can submit only the horizontal bands whose quantized energy changed.
    display.setAutoDisplay(false);
    for (std::size_t level = 0; level < _palette.size(); ++level) {
        const uint8_t energy = static_cast<uint8_t>(level * 255u / (_palette.size() - 1));
        // Reference fill is #AEFF00. Generate darker RGB565 layers once so
        // rendering never performs HSV conversion or alpha blending.
        constexpr uint8_t red = 174;
        constexpr uint8_t green = 255;
        constexpr uint8_t blue = 0;
        GlowColors& colors = _palette[level];
        colors.outer = display.color565(scaleChannel(red, energy / 14), scaleChannel(green, energy / 14),
                                        scaleChannel(blue, energy / 10));
        colors.middle = display.color565(scaleChannel(red, energy / 5), scaleChannel(green, energy / 5),
                                         scaleChannel(blue, energy / 4));
        colors.inner = display.color565(scaleChannel(red, energy * 3 / 5), scaleChannel(green, energy * 3 / 5),
                                        scaleChannel(blue, energy / 2));
        colors.core = display.color565(scaleChannel(red, energy), scaleChannel(green, energy),
                                       scaleChannel(blue, energy));
    }

    display.fillScreen(TFT_BLACK);
    display.display(0, 0, _width, _height);
}

void Renderer::close()
{
    auto& display = GetHAL().getDisplay();
    display.fillScreen(TFT_BLACK);
    display.display();
    display.setAutoDisplay(true);
}

void Renderer::render(const Engine& engine, uint32_t nowMs, bool rippleMode, uint32_t modeNoticeUntilMs,
                      RenderScene scene)
{
    if (_lastFrameMs != 0 && nowMs - _lastFrameMs < kFrameIntervalMs) return;
    _lastFrameMs = nowMs;

    if (!_sceneInitialized || scene != _activeScene) {
        _activeScene = scene;
        _sceneInitialized = true;
        _fullFrameReady = false;
        _displayedLevelsValid = false;
        _statsStartedMs = nowMs;
        _frameCount = 0;
        _frameTimeTotalUs = 0;
        _maxFrameTimeUs = 0;
        mclog::tagInfo("GlowField", "benchmark scene={}", sceneName(scene));
    }

    const int64_t frameStartedUs = esp_timer_get_time();
    const bool showHint = scene == RenderScene::Interactive &&
                          static_cast<int32_t>(modeNoticeUntilMs - nowMs) > 0;
    if (scene == RenderScene::Interactive) {
        renderInteractiveDirty(engine, rippleMode, showHint);
    } else {
        // Benchmark scenes deliberately force complete submissions so their
        // numbers continue to expose the panel's full-screen bandwidth.
        renderFullFrame(engine, rippleMode, showHint, scene);
    }

    const uint32_t elapsedUs = static_cast<uint32_t>(esp_timer_get_time() - frameStartedUs);
    ++_frameCount;
    _frameTimeTotalUs += elapsedUs;
    _maxFrameTimeUs = std::max(_maxFrameTimeUs, elapsedUs);
    reportStats(nowMs, scene);
}

uint8_t Renderer::levelForDot(const Dot& dot, std::size_t index, RenderScene scene) const
{
    if (scene == RenderScene::IdleDots) return 0;
    if (scene == RenderScene::EnergizedDots) return static_cast<uint8_t>(1 + (index % 15));
    return static_cast<uint8_t>(dot.energy >> 4);
}

void Renderer::drawDot(LGFX_Device& target, const Dot& dot, uint8_t level, uint16_t idleColor) const
{
    if (level == 0) {
        drawStar(target, dot.x, dot.y, 5, 2, idleColor);
        return;
    }
    const GlowColors& colors = _palette[level];
    target.fillCircle(dot.x, dot.y, 7 + level / 3, colors.outer);
    target.fillCircle(dot.x, dot.y, 5 + level / 5, colors.middle);
    drawStar(target, dot.x, dot.y, 5 + level / 5, 2 + level / 8, colors.inner);
    drawStar(target, dot.x, dot.y, 4 + level / 6, 1 + level / 8, colors.core);
}

void Renderer::drawHint(LGFX_Device& target, bool rippleMode) const
{
    const int x = _width / 2;
    const int y = _height - 15;
    const uint16_t hintColor = target.color565(72, 106, 0);
    if (rippleMode) {
        target.drawCircle(x, y, 3, hintColor);
        target.drawCircle(x, y, 7, hintColor);
        target.drawCircle(x, y, 11, hintColor);
    } else {
        drawStar(target, x, y, 10, 3, hintColor);
    }
}

void Renderer::renderFullFrame(const Engine& engine, bool rippleMode, bool showHint, RenderScene scene)
{
    auto& display = GetHAL().getDisplay();
    if (!_fullFrameReady) {
        display.startWrite();
        if (scene == RenderScene::Solid) {
            display.fillScreen(_palette.back().core);
        } else {
            display.fillScreen(TFT_BLACK);
            const uint16_t idleColor = display.color565(10, 15, 18);
            for (std::size_t i = 0; i < engine.dotCount(); ++i) {
                const Dot& dot = engine.dots()[i];
                if (dot.visible) drawDot(display, dot, levelForDot(dot, i, scene), idleColor);
            }
            if (showHint) drawHint(display, rippleMode);
        }
        display.endWrite();
        _fullFrameReady = true;
    }
    // Static benchmark scenes keep their raster in the panel framebuffer and
    // repeatedly submit it. This measures the actual full-screen transport
    // ceiling without charging every frame for identical geometry synthesis.
    display.display(0, 0, _width, _height);
    GetHAL().feedTheDog();
}

void Renderer::renderInteractiveDirty(const Engine& engine, bool rippleMode, bool showHint)
{
    constexpr std::size_t kMaxBands = (480 + kDirtyBandHeight - 1) / kDirtyBandHeight;
    std::array<bool, kMaxBands> dirty = {};
    const std::size_t bandCount = static_cast<std::size_t>((_height + kDirtyBandHeight - 1) /
                                                           kDirtyBandHeight);

    auto markRange = [&](int top, int bottom) {
        top = std::max(0, top);
        bottom = std::min(_height - 1, bottom);
        for (int band = top / kDirtyBandHeight; band <= bottom / kDirtyBandHeight; ++band) {
            dirty[static_cast<std::size_t>(band)] = true;
        }
    };

    for (std::size_t i = 0; i < engine.dotCount(); ++i) {
        const Dot& dot = engine.dots()[i];
        const uint8_t level = levelForDot(dot, i, RenderScene::Interactive);
        if (!_displayedLevelsValid || level != _displayedLevels[i]) {
            markRange(dot.y - kMaxDotRadius, dot.y + kMaxDotRadius);
        }
        _displayedLevels[i] = level;
    }

    const uint8_t hint = showHint ? (rippleMode ? 1 : 2) : 0;
    if (!_displayedLevelsValid || hint != _displayedHint) {
        markRange(_height - 27, _height - 3);
    }
    _displayedHint = hint;
    _displayedLevelsValid = true;

    auto& display = GetHAL().getDisplay();
    const uint16_t idleColor = display.color565(10, 15, 18);
    for (std::size_t first = 0; first < bandCount;) {
        if (!dirty[first]) {
            ++first;
            continue;
        }
        std::size_t last = first;
        while (last + 1 < bandCount && dirty[last + 1]) ++last;
        const int top = static_cast<int>(first) * kDirtyBandHeight;
        const int bottom = std::min(_height, static_cast<int>(last + 1) * kDirtyBandHeight);

        display.startWrite();
        display.setClipRect(0, top, _width, bottom - top);
        display.fillRect(0, top, _width, bottom - top, TFT_BLACK);
        for (std::size_t i = 0; i < engine.dotCount(); ++i) {
            const Dot& dot = engine.dots()[i];
            if (!dot.visible || dot.y + kMaxDotRadius < top || dot.y - kMaxDotRadius >= bottom) continue;
            drawDot(display, dot, _displayedLevels[i], idleColor);
        }
        if (showHint && _height - 27 < bottom && _height - 3 >= top) drawHint(display, rippleMode);
        display.clearClipRect();
        display.endWrite();
        display.display(0, top, _width, bottom - top);
        first = last + 1;
    }
    GetHAL().feedTheDog();
}

void Renderer::reportStats(uint32_t nowMs, RenderScene scene)
{
    if (_statsStartedMs == 0) {
        _statsStartedMs = nowMs;
        return;
    }
    const uint32_t elapsedMs = nowMs - _statsStartedMs;
    if (elapsedMs < 10000 || _frameCount == 0) return;

    const float fps = static_cast<float>(_frameCount) * 1000.0f / static_cast<float>(elapsedMs);
    const float averageMs = static_cast<float>(_frameTimeTotalUs) / static_cast<float>(_frameCount) / 1000.0f;
    mclog::tagInfo("GlowField", "scene={} {}x{} dots={} fps={:.1f} avg={:.2f}ms max={:.2f}ms",
                   sceneName(scene), _width, _height, _dotCount, fps, averageMs,
                   static_cast<float>(_maxFrameTimeUs) / 1000.0f);

    _statsStartedMs = nowMs;
    _frameCount = 0;
    _frameTimeTotalUs = 0;
    _maxFrameTimeUs = 0;
}

}  // namespace glow_field
