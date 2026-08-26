#include "glow_field_renderer.h"

#include <algorithm>
#include <cmath>
#include <esp_timer.h>
#include <hal/hal.h>
#include <mooncake_log.h>

namespace glow_field {
namespace {

constexpr uint32_t kFrameIntervalMs = 33;
constexpr int kDirtyBandHeight = 19;
constexpr int kMaxDotRadius = 18;
constexpr int kRingThickness = 5;
// Previous 20px ring sat flush to the bezel; its midline is the selector path.
// Collapse the thinner band onto that orbit instead of sliding it outward.
constexpr int kRingPathInset = 12;
constexpr uint8_t kThemeSaturation = 191;  // 75%, softer than a fully saturated wheel.
constexpr float kPi = 3.14159265358979323846f;

struct RingLayout {
    int pathRadius;
    int innerRadius;
    int outerRadius;
};

RingLayout ringLayout(int width, int height)
{
    const int pathRadius = std::min(width, height) / 2 - kRingPathInset;
    const int innerRadius = pathRadius - kRingThickness / 2;
    return {pathRadius, innerRadius, innerRadius + kRingThickness};
}

struct Rgb8 {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

Rgb8 hueToRgb(uint16_t hue)
{
    const uint16_t normalized = hue % 360u;
    const uint8_t sector = static_cast<uint8_t>(normalized / 60u);
    const uint8_t offset = static_cast<uint8_t>((normalized % 60u) * 255u / 60u);
    switch (sector) {
        case 0: return {255, offset, 0};
        case 1: return {static_cast<uint8_t>(255 - offset), 255, 0};
        case 2: return {0, 255, offset};
        case 3: return {0, static_cast<uint8_t>(255 - offset), 255};
        case 4: return {offset, 0, 255};
        default: return {255, 0, static_cast<uint8_t>(255 - offset)};
    }
}

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

uint8_t mixWithWhite(uint8_t channel, uint8_t saturation)
{
    return static_cast<uint8_t>(255u -
                                (static_cast<uint16_t>(255u - channel) * saturation) / 255u);
}

int scaledSize(int pixels)
{
    return (pixels * 13 + 5) / 10;
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
    _displayedColorIndexes = {};
    _displayedLevelsValid = false;
    _displayedHint = 0;
    _colorSelectionMode = false;
    _ringNeedsFullRefresh = false;
    _selectedHue = 79;
    _displayedSelectorHue = _selectedHue;

    auto& display = GetHAL().getDisplay();
    // The AMOLED driver already owns a PSRAM framebuffer. Keep automatic
    // full-screen commits disabled while Glow Field is active so Interactive
    // can submit only the horizontal bands whose quantized energy changed.
    display.setAutoDisplay(false);
    buildPalettes();

    const int centerX = _width / 2;
    const int centerY = _height / 2;
    const RingLayout ring = ringLayout(_width, _height);
    for (std::size_t segment = 0; segment < _ringSegments.size(); ++segment) {
        const float startDegrees = static_cast<float>(segment) * 360.0f / _ringSegments.size() - 90.0f;
        const float endDegrees = static_cast<float>(segment + 1) * 360.0f /
                                     _ringSegments.size() -
                                 90.0f;
        const float middleDegrees = (startDegrees + endDegrees) * 0.5f + 90.0f;
        const float startAngle = startDegrees * kPi / 180.0f;
        const float endAngle = endDegrees * kPi / 180.0f;
        const Rgb8 hueRgb = hueToRgb(static_cast<uint16_t>(middleDegrees + 360.0f) % 360u);
        RingSegment& ringSegment = _ringSegments[segment];
        ringSegment.minY = static_cast<int16_t>(_height);
        ringSegment.maxY = -1;
        for (std::size_t boundary = 0; boundary <= kRingRadialBands; ++boundary) {
            const int radius = ring.innerRadius + static_cast<int>(boundary * kRingThickness /
                                                               kRingRadialBands);
            ringSegment.startX[boundary] = static_cast<int16_t>(centerX + std::cos(startAngle) * radius);
            ringSegment.startY[boundary] = static_cast<int16_t>(centerY + std::sin(startAngle) * radius);
            ringSegment.endX[boundary] = static_cast<int16_t>(centerX + std::cos(endAngle) * radius);
            ringSegment.endY[boundary] = static_cast<int16_t>(centerY + std::sin(endAngle) * radius);
            ringSegment.minY = std::min(ringSegment.minY,
                                        std::min(ringSegment.startY[boundary],
                                                 ringSegment.endY[boundary]));
            ringSegment.maxY = std::max(ringSegment.maxY,
                                        std::max(ringSegment.startY[boundary],
                                                 ringSegment.endY[boundary]));
        }
        ringSegment.colors[0] = display.color565(mixWithWhite(hueRgb.red, kThemeSaturation),
                                                 mixWithWhite(hueRgb.green, kThemeSaturation),
                                                 mixWithWhite(hueRgb.blue, kThemeSaturation));
    }

    display.fillScreen(TFT_BLACK);
    display.display(0, 0, _width, _height);
}

void Renderer::buildPalettes()
{
    auto& display = GetHAL().getDisplay();
    for (std::size_t hueSlot = 0; hueSlot < kHueSlots; ++hueSlot) {
        const Rgb8 hueRgb = hueSlot == 5 ? Rgb8{174, 255, 0}
                                         : hueToRgb(static_cast<uint16_t>(hueSlot * 360u /
                                                                         kHueSlots));
        const Rgb8 rgb = {mixWithWhite(hueRgb.red, kThemeSaturation),
                          mixWithWhite(hueRgb.green, kThemeSaturation),
                          mixWithWhite(hueRgb.blue, kThemeSaturation)};
        for (std::size_t level = 0; level < _palettes[hueSlot].size(); ++level) {
            const uint8_t energy = static_cast<uint8_t>(
                level * 255u / (_palettes[hueSlot].size() - 1));
            GlowColors& colors = _palettes[hueSlot][level];
            colors.outer = display.color565(scaleChannel(rgb.red, energy / 18),
                                            scaleChannel(rgb.green, energy / 18),
                                            scaleChannel(rgb.blue, energy / 13));
            colors.middle = display.color565(scaleChannel(rgb.red, energy / 6),
                                             scaleChannel(rgb.green, energy / 6),
                                             scaleChannel(rgb.blue, energy / 5));
            colors.inner = display.color565(scaleChannel(rgb.red, energy * 3 / 5),
                                            scaleChannel(rgb.green, energy * 3 / 5),
                                            scaleChannel(rgb.blue, energy / 2));
            colors.core = display.color565(scaleChannel(rgb.red, energy),
                                           scaleChannel(rgb.green, energy),
                                           scaleChannel(rgb.blue, energy));
        }
    }
}

void Renderer::close()
{
    auto& display = GetHAL().getDisplay();
    display.fillScreen(TFT_BLACK);
    display.display();
    display.setAutoDisplay(true);
}

void Renderer::updateColorSelection(bool enabled, uint16_t selectedHue)
{
    selectedHue %= 360u;
    if (enabled != _colorSelectionMode) {
        _colorSelectionMode = enabled;
        _ringNeedsFullRefresh = true;
    }
    if (selectedHue == _selectedHue) return;
    _selectedHue = selectedHue;
    _fullFrameReady = false;
}

void Renderer::render(const Engine& engine, uint32_t nowMs, bool rippleMode, uint32_t modeNoticeUntilMs,
                      RenderScene scene, bool colorSelectionMode, uint16_t selectedHue)
{
    updateColorSelection(colorSelectionMode, selectedHue);
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
    const bool showHint = scene == RenderScene::Interactive && !colorSelectionMode &&
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
    return static_cast<uint8_t>(std::max(dot.energy, dot.rippleEnergy) >> 4);
}

uint8_t Renderer::colorIndexForDot(const Dot& dot, RenderScene scene) const
{
    if (scene == RenderScene::Interactive && dot.rippleEnergy >= dot.energy && dot.rippleEnergy > 0) {
        return dot.rippleColorIndex;
    }
    return dot.colorIndex;
}

void Renderer::drawDot(LGFX_Device& target, const Dot& dot, uint8_t level, uint16_t idleColor) const
{
    if (level == 0) {
        drawStar(target, dot.x, dot.y, scaledSize(7), scaledSize(3), idleColor);
        return;
    }
    const uint8_t colorIndex = dot.rippleEnergy >= dot.energy && dot.rippleEnergy > 0
                                   ? dot.rippleColorIndex
                                   : dot.colorIndex;
    const GlowColors& colors = _palettes[colorIndex % _palettes.size()][level];
    target.fillCircle(dot.x, dot.y, scaledSize(7 + level / 3), colors.outer);
    target.fillCircle(dot.x, dot.y, scaledSize(5 + level / 5), colors.middle);
    drawStar(target, dot.x, dot.y, scaledSize(6 + level / 4), scaledSize(2 + level / 6),
             colors.inner);
    drawStar(target, dot.x, dot.y, scaledSize(5 + level / 5), scaledSize(2 + level / 10),
             colors.core);
}

void Renderer::drawHint(LGFX_Device& target, bool rippleMode) const
{
    const int x = _width / 2;
    const int y = _height - 15;
    const std::size_t hueSlot = static_cast<std::size_t>(_selectedHue) * kHueSlots / 360u;
    const uint16_t hintColor = _palettes[hueSlot].back().core;
    if (rippleMode) {
        target.drawCircle(x, y, 3, hintColor);
        target.drawCircle(x, y, 7, hintColor);
        target.drawCircle(x, y, 11, hintColor);
    } else {
        drawStar(target, x, y, 10, 3, hintColor);
    }
}

void Renderer::drawColorRing(LGFX_Device& target, int top, int bottom) const
{
    const int centerX = _width / 2;
    const int centerY = _height / 2;
    const int pathRadius = ringLayout(_width, _height).pathRadius;
    for (const RingSegment& segment : _ringSegments) {
        if (segment.maxY < top || segment.minY >= bottom) continue;
        for (std::size_t band = 0; band < kRingRadialBands; ++band) {
            const std::size_t outer = band + 1;
            target.fillTriangle(segment.startX[band], segment.startY[band],
                                segment.startX[outer], segment.startY[outer],
                                segment.endX[outer], segment.endY[outer], segment.colors[band]);
            target.fillTriangle(segment.startX[band], segment.startY[band],
                                segment.endX[outer], segment.endY[outer],
                                segment.endX[band], segment.endY[band], segment.colors[band]);
        }
    }

    const float selectedAngle = (static_cast<float>(_selectedHue) - 90.0f) * kPi / 180.0f;
    const int selectedX = centerX + static_cast<int>(std::cos(selectedAngle) * pathRadius);
    const int selectedY = centerY + static_cast<int>(std::sin(selectedAngle) * pathRadius);
    const std::size_t hueSlot = static_cast<std::size_t>(_selectedHue) * kHueSlots / 360u;
    if (selectedY + 10 >= top && selectedY - 10 < bottom) {
        target.fillCircle(selectedX, selectedY, 10, _palettes[hueSlot].back().core);
    }
}

void Renderer::renderFullFrame(const Engine& engine, bool rippleMode, bool showHint, RenderScene scene)
{
    auto& display = GetHAL().getDisplay();
    if (!_fullFrameReady) {
        display.startWrite();
        if (scene == RenderScene::Solid) {
            const std::size_t hueSlot = static_cast<std::size_t>(_selectedHue) * kHueSlots / 360u;
            display.fillScreen(_palettes[hueSlot].back().core);
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

    if (_ringNeedsFullRefresh) markRange(0, _height - 1);
    if (_colorSelectionMode && _displayedSelectorHue != _selectedHue) {
        const int centerY = _height / 2;
        const int pathRadius = ringLayout(_width, _height).pathRadius;
        auto selectorY = [&](uint16_t hue) {
            const float angle = (static_cast<float>(hue) - 90.0f) * kPi / 180.0f;
            return centerY + static_cast<int>(std::sin(angle) * pathRadius);
        };
        // The old implementation dirtied the entire vertical span between the
        // two positions. These two small, disjoint bands are all that changed.
        const int oldY = selectorY(_displayedSelectorHue);
        const int newY = selectorY(_selectedHue);
        markRange(oldY - 11, oldY + 11);
        markRange(newY - 11, newY + 11);
    }

    for (std::size_t i = 0; i < engine.dotCount(); ++i) {
        const Dot& dot = engine.dots()[i];
        const uint8_t level = levelForDot(dot, i, RenderScene::Interactive);
        const uint8_t colorIndex = colorIndexForDot(dot, RenderScene::Interactive);
        if (!_displayedLevelsValid || level != _displayedLevels[i] ||
            colorIndex != _displayedColorIndexes[i]) {
            markRange(dot.y - kMaxDotRadius, dot.y + kMaxDotRadius);
        }
        _displayedLevels[i] = level;
        _displayedColorIndexes[i] = colorIndex;
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
        if (_colorSelectionMode) drawColorRing(display, top, bottom);
        display.clearClipRect();
        display.endWrite();
        display.display(0, top, _width, bottom - top);
        first = last + 1;
    }
    _ringNeedsFullRefresh = false;
    _displayedSelectorHue = _selectedHue;
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
