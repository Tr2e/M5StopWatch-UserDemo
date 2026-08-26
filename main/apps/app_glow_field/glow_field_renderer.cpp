#include "glow_field_renderer.h"

#include <algorithm>
#include <esp_timer.h>
#include <hal/hal.h>
#include <mooncake_log.h>

namespace glow_field {
namespace {

constexpr uint32_t kFrameIntervalMs = 33;

void drawStar(LGFX_Sprite& canvas, int x, int y, int radius, int waist, uint16_t color)
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

    auto& canvas = GetHAL().getCanvas();
    for (std::size_t level = 0; level < _palette.size(); ++level) {
        const uint8_t energy = static_cast<uint8_t>(level * 255u / (_palette.size() - 1));
        // Reference fill is #AEFF00. Generate darker RGB565 layers once so
        // rendering never performs HSV conversion or alpha blending.
        constexpr uint8_t red = 174;
        constexpr uint8_t green = 255;
        constexpr uint8_t blue = 0;
        GlowColors& colors = _palette[level];
        colors.outer = canvas.color565(scaleChannel(red, energy / 14), scaleChannel(green, energy / 14),
                                       scaleChannel(blue, energy / 10));
        colors.middle = canvas.color565(scaleChannel(red, energy / 5), scaleChannel(green, energy / 5),
                                        scaleChannel(blue, energy / 4));
        colors.inner = canvas.color565(scaleChannel(red, energy * 3 / 5), scaleChannel(green, energy * 3 / 5),
                                       scaleChannel(blue, energy / 2));
        colors.core = canvas.color565(scaleChannel(red, energy), scaleChannel(green, energy),
                                      scaleChannel(blue, energy));
    }
}

void Renderer::render(const Engine& engine, uint32_t nowMs, bool rippleMode, uint32_t modeNoticeUntilMs,
                      RenderScene scene)
{
    if (_lastFrameMs != 0 && nowMs - _lastFrameMs < kFrameIntervalMs) return;
    _lastFrameMs = nowMs;

    if (!_sceneInitialized || scene != _activeScene) {
        _activeScene = scene;
        _sceneInitialized = true;
        _statsStartedMs = nowMs;
        _frameCount = 0;
        _frameTimeTotalUs = 0;
        _maxFrameTimeUs = 0;
        mclog::tagInfo("GlowField", "benchmark scene={}", sceneName(scene));
    }

    const int64_t frameStartedUs = esp_timer_get_time();
    auto& canvas = GetHAL().getCanvas();
    if (scene == RenderScene::Solid) {
        canvas.fillScreen(_palette.back().core);
        GetHAL().feedTheDog();
        GetHAL().updateCanvas();

        const uint32_t elapsedUs = static_cast<uint32_t>(esp_timer_get_time() - frameStartedUs);
        ++_frameCount;
        _frameTimeTotalUs += elapsedUs;
        _maxFrameTimeUs = std::max(_maxFrameTimeUs, elapsedUs);
        reportStats(nowMs, scene);
        return;
    }

    canvas.fillScreen(TFT_BLACK);
    const uint16_t idleColor = canvas.color565(10, 15, 18);

    for (std::size_t i = 0; i < engine.dotCount(); ++i) {
        const Dot& dot = engine.dots()[i];
        if (!dot.visible) continue;

        uint8_t level = static_cast<uint8_t>(dot.energy >> 4);
        if (scene == RenderScene::IdleDots) {
            level = 0;
        } else if (scene == RenderScene::EnergizedDots) {
            level = static_cast<uint8_t>(1 + (i % 15));
        }
        if (level == 0) {
            drawStar(canvas, dot.x, dot.y, 5, 2, idleColor);
            continue;
        }

        const GlowColors& colors = _palette[level];
        // Only energized points pay for Glow layers. The star itself stays
        // crisp while the two small circles provide the soft reference halo.
        canvas.fillCircle(dot.x, dot.y, 7 + level / 3, colors.outer);
        canvas.fillCircle(dot.x, dot.y, 5 + level / 5, colors.middle);
        drawStar(canvas, dot.x, dot.y, 5 + level / 5, 2 + level / 8, colors.inner);
        drawStar(canvas, dot.x, dot.y, 4 + level / 6, 1 + level / 8, colors.core);
    }

    // A short, language-free mode hint avoids adding a permanent settings UI
    // over the animation: rings mean Ripple, a star means Paint.
    if (scene == RenderScene::Interactive && static_cast<int32_t>(modeNoticeUntilMs - nowMs) > 0) {
        const int x = _width / 2;
        const int y = _height - 15;
        const uint16_t hintColor = canvas.color565(72, 106, 0);
        if (rippleMode) {
            canvas.drawCircle(x, y, 3, hintColor);
            canvas.drawCircle(x, y, 7, hintColor);
            canvas.drawCircle(x, y, 11, hintColor);
        } else {
            drawStar(canvas, x, y, 10, 3, hintColor);
        }
    }

    GetHAL().feedTheDog();
    GetHAL().updateCanvas();

    const uint32_t elapsedUs = static_cast<uint32_t>(esp_timer_get_time() - frameStartedUs);
    ++_frameCount;
    _frameTimeTotalUs += elapsedUs;
    _maxFrameTimeUs = std::max(_maxFrameTimeUs, elapsedUs);
    reportStats(nowMs, scene);
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
