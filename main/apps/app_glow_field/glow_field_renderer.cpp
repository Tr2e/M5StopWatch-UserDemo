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
constexpr int kMaxDotRadius = 24;
constexpr uint8_t kMinShapeScale = 80;
constexpr uint8_t kMaxShapeScale = 150;
constexpr int kRingThickness = 5;
// Previous 20px ring sat flush to the bezel; its midline is the selector path.
// Collapse the thinner band onto that orbit instead of sliding it outward.
constexpr int kRingPathInset = 12;
constexpr uint8_t kThemeSaturation = 191;  // 75%, softer than a fully saturated wheel.
constexpr uint8_t kRingOpacity = 51;       // 20%, pre-blended against the black field.
constexpr uint8_t kSelectorOpacity = 153;  // 60% core.
constexpr uint8_t kSelectorGlowInnerOpacity = 82;  // 32% inner halo.
constexpr uint8_t kSelectorGlowOuterOpacity = 36;  // 14% outer halo.
constexpr int kSelectorGlowRadius = 17;
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

constexpr std::array<Rgb8, kSymbolGlyphCount> kSymbolColors = {
    Rgb8{0, 240, 255},    // cyber cyan    #00F0FF
    Rgb8{255, 0, 127},    // neon magenta  #FF007F
    Rgb8{0, 255, 102},    // proton green  #00FF66
    Rgb8{255, 234, 0},    // electric yellow #FFEA00
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

template <typename Target>
void drawTriangle(Target& canvas, int x, int y, int radius, uint16_t color)
{
    const int halfWidth = radius * 7 / 8;
    canvas.fillTriangle(x, y - radius, x - halfWidth, y + radius / 2,
                        x + halfWidth, y + radius / 2, color);
}

template <typename Target>
void drawGlyph(Target& canvas, DotShape shape, int x, int y, int radius, int waist, uint16_t color)
{
    radius = std::max(1, radius);
    waist = std::max(1, waist);
    switch (shape) {
        case DotShape::Star:
            drawStar(canvas, x, y, radius, waist, color);
            break;
        case DotShape::Hexagon: {
            const int halfWidth = std::max(1, radius * 7 / 8);
            const int halfMiddle = std::max(1, radius / 2);
            canvas.fillRect(x - halfWidth, y - halfMiddle, halfWidth * 2 + 1,
                            halfMiddle * 2 + 1, color);
            canvas.fillTriangle(x, y - radius, x - halfWidth, y - halfMiddle,
                                x + halfWidth, y - halfMiddle, color);
            canvas.fillTriangle(x, y + radius, x - halfWidth, y + halfMiddle,
                                x + halfWidth, y + halfMiddle, color);
            break;
        }
        case DotShape::Circle:
            canvas.fillCircle(x, y, std::max(1, radius * 3 / 4), color);
            break;
        case DotShape::Triangle:
            drawTriangle(canvas, x, y, radius, color);
            break;
        case DotShape::SymbolMix:
            drawStar(canvas, x, y, radius, waist, color);
            break;
    }
}

template <typename Target>
void drawCross(Target& canvas, int x, int y, int radius, int waist, uint16_t color)
{
    const int thickness = std::max(1, waist);
    canvas.fillTriangle(x - radius, y - radius + thickness,
                        x - radius + thickness, y - radius,
                        x + radius, y + radius - thickness, color);
    canvas.fillTriangle(x - radius, y - radius + thickness,
                        x + radius, y + radius - thickness,
                        x + radius - thickness, y + radius, color);
    canvas.fillTriangle(x + radius - thickness, y - radius,
                        x + radius, y - radius + thickness,
                        x - radius + thickness, y + radius, color);
    canvas.fillTriangle(x + radius - thickness, y - radius,
                        x - radius + thickness, y + radius,
                        x - radius, y + radius - thickness, color);
}

template <typename Target>
void drawSymbolGlyph(Target& canvas, uint8_t symbolIndex, int x, int y, int radius, int waist,
                     uint16_t color)
{
    radius = std::max(1, radius);
    waist = std::max(1, waist);
    switch (static_cast<SymbolGlyph>(symbolIndex % kSymbolGlyphCount)) {
        case SymbolGlyph::Triangle:
            drawTriangle(canvas, x, y, radius, color);
            break;
        case SymbolGlyph::Circle:
            canvas.fillCircle(x, y, std::max(1, radius * 3 / 4), color);
            break;
        case SymbolGlyph::Cross:
            drawCross(canvas, x, y, radius, waist, color);
            break;
        case SymbolGlyph::Square: {
            const int halfSize = std::max(1, radius * 3 / 4);
            canvas.fillRect(x - halfSize, y - halfSize, halfSize * 2 + 1,
                            halfSize * 2 + 1, color);
            break;
        }
    }
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

int baseScaledSize(int pixels)
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

enum class VisualSource : uint8_t {
    Idle,
    AudioField,
    AudioRipple,
    Paint,
    FingerRipple,
};

VisualSource visualSource(const Dot& dot)
{
    const uint8_t fingerRipple = (!dot.rippleFromAudio && dot.rippleEnergy > 0) ? dot.rippleEnergy : 0;
    const uint8_t audioRipple = (dot.rippleFromAudio && dot.rippleEnergy > 0) ? dot.rippleEnergy : 0;
    const uint8_t paint = dot.energy;
    const uint8_t audio = dot.audioEnergy;
    const uint8_t best = std::max(std::max(fingerRipple, paint), std::max(audioRipple, audio));
    if (best == 0) return VisualSource::Idle;
    if (fingerRipple == best) return VisualSource::FingerRipple;
    if (paint == best) return VisualSource::Paint;
    if (audioRipple == best) return VisualSource::AudioRipple;
    return VisualSource::AudioField;
}

uint8_t hintCode(bool showAppearanceHint, bool showHint, InteractionMode mode)
{
    if (showAppearanceHint) return 3;
    if (!showHint) return 0;
    switch (mode) {
        case InteractionMode::Ripple: return 1;
        case InteractionMode::Paint: return 2;
        case InteractionMode::AudioReactive: return 4;
    }
    return 0;
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
    _dirtyBandTotal = 0;
    _dirtyBandSamples = 0;
    _lastFrameMs = 0;
    _sceneInitialized = false;
    _fullFrameReady = false;
    _displayedLevels = {};
    _displayedColorIndexes = {};
    _displayedLevelsValid = false;
    _displayedHint = 0;
    _colorSelectionMode = false;
    _ringNeedsFullRefresh = false;
    _appearanceNeedsFullRefresh = false;
    _selectedHue = 79;
    _displayedSelectorHue = _selectedHue;
    _dotShape = DotShape::SymbolMix;
    _shapeScalePercent = 100;
    _appearanceMode = false;

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
        ringSegment.colors[0] = display.color565(
            scaleChannel(mixWithWhite(hueRgb.red, kThemeSaturation), kRingOpacity),
            scaleChannel(mixWithWhite(hueRgb.green, kThemeSaturation), kRingOpacity),
            scaleChannel(mixWithWhite(hueRgb.blue, kThemeSaturation), kRingOpacity));
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
        _selectionColors[hueSlot] = display.color565(scaleChannel(rgb.red, kSelectorOpacity),
                                                     scaleChannel(rgb.green, kSelectorOpacity),
                                                     scaleChannel(rgb.blue, kSelectorOpacity));
        _selectionGlowInnerColors[hueSlot] = display.color565(
            scaleChannel(rgb.red, kSelectorGlowInnerOpacity),
            scaleChannel(rgb.green, kSelectorGlowInnerOpacity),
            scaleChannel(rgb.blue, kSelectorGlowInnerOpacity));
        _selectionGlowOuterColors[hueSlot] = display.color565(
            scaleChannel(rgb.red, kSelectorGlowOuterOpacity),
            scaleChannel(rgb.green, kSelectorGlowOuterOpacity),
            scaleChannel(rgb.blue, kSelectorGlowOuterOpacity));
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
    for (std::size_t accent = 0; accent < _symbolPalettes.size(); ++accent) {
        const Rgb8 rgb = kSymbolColors[accent];
        const bool brightHighlight = accent == 3;
        for (std::size_t level = 0; level < _symbolPalettes[accent].size(); ++level) {
            const uint8_t energy = static_cast<uint8_t>(
                level * 255u / (_symbolPalettes[accent].size() - 1));
            GlowColors& colors = _symbolPalettes[accent][level];
            colors.outer = display.color565(
                scaleChannel(rgb.red, energy / (brightHighlight ? 24 : 18)),
                scaleChannel(rgb.green, energy / (brightHighlight ? 24 : 18)),
                scaleChannel(rgb.blue, energy / (brightHighlight ? 18 : 13)));
            colors.middle = display.color565(
                scaleChannel(rgb.red, energy / (brightHighlight ? 8 : 6)),
                scaleChannel(rgb.green, energy / (brightHighlight ? 8 : 6)),
                scaleChannel(rgb.blue, energy / (brightHighlight ? 7 : 5)));
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

void Renderer::updateAppearance(DotShape dotShape, uint8_t shapeScalePercent, bool appearanceMode)
{
    shapeScalePercent = std::clamp<uint8_t>(shapeScalePercent, kMinShapeScale, kMaxShapeScale);
    if (dotShape == _dotShape && shapeScalePercent == _shapeScalePercent &&
        appearanceMode == _appearanceMode) {
        return;
    }

    _dotShape = dotShape;
    _shapeScalePercent = shapeScalePercent;
    _appearanceMode = appearanceMode;
    _appearanceNeedsFullRefresh = true;
    _fullFrameReady = false;
}

void Renderer::render(const Engine& engine, uint32_t nowMs, InteractionMode mode, uint32_t modeNoticeUntilMs,
                      RenderScene scene, bool colorSelectionMode, uint16_t selectedHue,
                      DotShape dotShape, uint8_t shapeScalePercent, bool appearanceMode)
{
    updateColorSelection(colorSelectionMode, selectedHue);
    updateAppearance(dotShape, shapeScalePercent, appearanceMode);
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
        _dirtyBandTotal = 0;
        _dirtyBandSamples = 0;
        mclog::tagInfo("GlowField", "benchmark scene={}", sceneName(scene));
    }

    const int64_t frameStartedUs = esp_timer_get_time();
    const bool showHint = scene == RenderScene::Interactive && !colorSelectionMode && !appearanceMode &&
                          static_cast<int32_t>(modeNoticeUntilMs - nowMs) > 0;
    const bool showAppearanceHint = scene == RenderScene::Interactive && appearanceMode;
    if (scene == RenderScene::Interactive) {
        renderInteractiveDirty(engine, mode, showHint, showAppearanceHint);
    } else {
        // Benchmark scenes deliberately force complete submissions so their
        // numbers continue to expose the panel's full-screen bandwidth.
        renderFullFrame(engine, mode, showHint, showAppearanceHint, scene);
    }

    const uint32_t elapsedUs = static_cast<uint32_t>(esp_timer_get_time() - frameStartedUs);
    ++_frameCount;
    _frameTimeTotalUs += elapsedUs;
    _maxFrameTimeUs = std::max(_maxFrameTimeUs, elapsedUs);
    reportStats(nowMs, scene);
}

int Renderer::shapeSize(int pixels) const
{
    return std::max(1, (baseScaledSize(pixels) * _shapeScalePercent + 50) / 100);
}

uint8_t Renderer::levelForDot(const Dot& dot, std::size_t index, RenderScene scene) const
{
    if (scene == RenderScene::IdleDots) return 0;
    if (scene == RenderScene::EnergizedDots) return static_cast<uint8_t>(1 + (index % 15));
    return static_cast<uint8_t>(
        std::max(std::max(dot.energy, dot.rippleEnergy), dot.audioEnergy) >> 4);
}

uint8_t Renderer::visualKeyForDot(const Dot& dot, RenderScene scene) const
{
    const VisualSource source = scene == RenderScene::Interactive ? visualSource(dot)
                                                                  : VisualSource::Idle;
    if (source == VisualSource::FingerRipple || source == VisualSource::AudioRipple) {
        if (dot.rippleUsesSymbolPalette) {
            return static_cast<uint8_t>(0x80u | ((dot.rippleSymbolColorIndex & 0x03u) << 2) |
                                        (dot.rippleSymbolIndex & 0x03u));
        }
        return static_cast<uint8_t>(dot.rippleColorIndex & 0x1Fu);
    }
    if (source == VisualSource::Paint) {
        if (_dotShape == DotShape::SymbolMix) {
            const bool energyUsesSymbols = dot.energy > 0 && dot.energyUsesSymbolPalette;
            const uint8_t symbolIndex = energyUsesSymbols ? dot.energySymbolIndex : dot.symbolIndex;
            const uint8_t colorIndex = energyUsesSymbols ? dot.energySymbolColorIndex
                                                         : dot.symbolColorIndex;
            return static_cast<uint8_t>(0x40u | ((colorIndex & 0x03u) << 2) |
                                        (symbolIndex & 0x03u));
        }
        return static_cast<uint8_t>(dot.colorIndex & 0x1Fu);
    }
    if (source == VisualSource::AudioField) {
        if (dot.audioUsesSymbolPalette) {
            return static_cast<uint8_t>(0xC0u | ((dot.audioColorIndex & 0x03u) << 2) |
                                        (dot.audioSymbolIndex & 0x03u));
        }
        return static_cast<uint8_t>(0x20u | (dot.audioColorIndex & 0x1Fu));
    }
    if (_dotShape == DotShape::SymbolMix) {
        return static_cast<uint8_t>(0x40u | ((dot.symbolColorIndex & 0x03u) << 2) |
                                    (dot.symbolIndex & 0x03u));
    }
    return static_cast<uint8_t>(dot.colorIndex & 0x1Fu);
}

void Renderer::drawDot(LGFX_Device& target, const Dot& dot, uint8_t level, uint16_t idleColor) const
{
    if (level == 0) {
        if (_dotShape == DotShape::SymbolMix) {
            drawSymbolGlyph(target, dot.symbolIndex, dot.x, dot.y, shapeSize(7), shapeSize(3),
                            idleColor);
        } else {
            drawGlyph(target, _dotShape, dot.x, dot.y, shapeSize(7), shapeSize(3), idleColor);
        }
        return;
    }
    const VisualSource source = visualSource(dot);
    bool useSymbolPalette = false;
    uint8_t colorIndex = dot.colorIndex;
    uint8_t symbolIndex = dot.symbolIndex;
    switch (source) {
        case VisualSource::FingerRipple:
        case VisualSource::AudioRipple:
            useSymbolPalette = dot.rippleUsesSymbolPalette;
            colorIndex = useSymbolPalette ? dot.rippleSymbolColorIndex : dot.rippleColorIndex;
            symbolIndex = dot.rippleSymbolIndex;
            break;
        case VisualSource::Paint:
            useSymbolPalette = _dotShape == DotShape::SymbolMix;
            colorIndex = useSymbolPalette
                             ? (dot.energyUsesSymbolPalette ? dot.energySymbolColorIndex
                                                            : dot.symbolColorIndex)
                             : dot.colorIndex;
            symbolIndex = dot.energyUsesSymbolPalette ? dot.energySymbolIndex : dot.symbolIndex;
            break;
        case VisualSource::AudioField:
            useSymbolPalette = dot.audioUsesSymbolPalette;
            colorIndex = dot.audioColorIndex;
            symbolIndex = dot.audioSymbolIndex;
            break;
        case VisualSource::Idle:
            useSymbolPalette = _dotShape == DotShape::SymbolMix;
            colorIndex = useSymbolPalette ? dot.symbolColorIndex : dot.colorIndex;
            symbolIndex = dot.symbolIndex;
            break;
    }
    const GlowColors& colors = useSymbolPalette
                                   ? _symbolPalettes[colorIndex % _symbolPalettes.size()][level]
                                   : _palettes[colorIndex % _palettes.size()][level];
    target.fillCircle(dot.x, dot.y, shapeSize(7 + level / 3), colors.outer);
    target.fillCircle(dot.x, dot.y, shapeSize(5 + level / 5), colors.middle);
    if (useSymbolPalette) {
        const uint8_t glyphIndex = useSymbolPalette ? symbolIndex : dot.symbolIndex;
        drawSymbolGlyph(target, glyphIndex, dot.x, dot.y, shapeSize(6 + level / 4),
                        shapeSize(2 + level / 6), colors.inner);
        drawSymbolGlyph(target, glyphIndex, dot.x, dot.y, shapeSize(5 + level / 5),
                        shapeSize(2 + level / 10), colors.core);
    } else {
        drawGlyph(target, _dotShape, dot.x, dot.y, shapeSize(6 + level / 4),
                  shapeSize(2 + level / 6), colors.inner);
        drawGlyph(target, _dotShape, dot.x, dot.y, shapeSize(5 + level / 5),
                  shapeSize(2 + level / 10), colors.core);
    }
}

void Renderer::drawHint(LGFX_Device& target, InteractionMode mode) const
{
    const int x = _width / 2;
    const int y = _height - 15;
    if (mode == InteractionMode::AudioReactive) {
        constexpr std::array<int, 5> kBarHeights = {5, 11, 7, 13, 6};
        for (std::size_t i = 0; i < kBarHeights.size(); ++i) {
            const uint16_t color = _dotShape == DotShape::SymbolMix
                                       ? _symbolPalettes[i % _symbolPalettes.size()].back().core
                                       : _palettes[static_cast<std::size_t>(_selectedHue) *
                                                   kHueSlots / 360u]
                                             .back()
                                             .core;
            const int barX = x - 10 + static_cast<int>(i) * 5;
            const int height = kBarHeights[i];
            target.fillRect(barX, y - height / 2, 2, height, color);
        }
        return;
    }

    const bool rippleMode = mode == InteractionMode::Ripple;
    if (_dotShape == DotShape::SymbolMix) {
        if (rippleMode) {
            target.drawCircle(x, y, 3, _symbolPalettes[0].back().core);
            target.drawCircle(x, y, 7, _symbolPalettes[2].back().core);
            target.drawCircle(x, y, 11, _symbolPalettes[3].back().core);
        } else {
            drawSymbolGlyph(target, 0, x - 6, y - 6, 4, 1,
                            _symbolPalettes[0].back().core);
            drawSymbolGlyph(target, 1, x + 6, y - 6, 4, 1,
                            _symbolPalettes[1].back().core);
            drawSymbolGlyph(target, 2, x - 6, y + 6, 4, 2,
                            _symbolPalettes[2].back().core);
            drawSymbolGlyph(target, 3, x + 6, y + 6, 4, 1,
                            _symbolPalettes[3].back().core);
        }
        return;
    }
    const std::size_t hueSlot = static_cast<std::size_t>(_selectedHue) * kHueSlots / 360u;
    const uint16_t hintColor = _palettes[hueSlot].back().core;
    if (rippleMode) {
        target.drawCircle(x, y, 3, hintColor);
        target.drawCircle(x, y, 7, hintColor);
        target.drawCircle(x, y, 11, hintColor);
    } else {
        drawGlyph(target, _dotShape, x, y, 10, 3, hintColor);
    }
}

void Renderer::drawAppearanceHint(LGFX_Device& target) const
{
    const int y = _height - 18;
    const int glyphX = _width / 2 - 32;
    const int lineStart = _width / 2 - 12;
    const int lineEnd = _width / 2 + 38;
    const std::size_t hueSlot = static_cast<std::size_t>(_selectedHue) * kHueSlots / 360u;
    const uint16_t color = _dotShape == DotShape::SymbolMix
                               ? _symbolPalettes[2].back().core
                               : _palettes[hueSlot].back().core;
    const uint16_t track = GetHAL().getDisplay().color565(35, 45, 48);

    if (_dotShape == DotShape::SymbolMix) {
        drawSymbolGlyph(target, 0, glyphX - 6, y - 6, 4, 1,
                        _symbolPalettes[0].back().core);
        drawSymbolGlyph(target, 1, glyphX + 6, y - 6, 4, 1,
                        _symbolPalettes[1].back().core);
        drawSymbolGlyph(target, 2, glyphX - 6, y + 6, 4, 2,
                        _symbolPalettes[2].back().core);
        drawSymbolGlyph(target, 3, glyphX + 6, y + 6, 4, 1,
                        _symbolPalettes[3].back().core);
    } else {
        drawGlyph(target, _dotShape, glyphX, y, 9, 3, color);
    }
    target.fillRect(lineStart, y - 1, lineEnd - lineStart + 1, 3, track);
    const int thumbX = lineStart + static_cast<int>(_shapeScalePercent - kMinShapeScale) *
                                       (lineEnd - lineStart) /
                                       (kMaxShapeScale - kMinShapeScale);
    target.fillCircle(thumbX, y, 5, color);
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
    if (selectedY + kSelectorGlowRadius >= top && selectedY - kSelectorGlowRadius < bottom) {
        target.fillCircle(selectedX, selectedY, kSelectorGlowRadius,
                          _selectionGlowOuterColors[hueSlot]);
        target.fillCircle(selectedX, selectedY, 13, _selectionGlowInnerColors[hueSlot]);
        target.fillCircle(selectedX, selectedY, 10, _selectionColors[hueSlot]);
    }
}

void Renderer::renderFullFrame(const Engine& engine, InteractionMode mode, bool showHint,
                               bool showAppearanceHint, RenderScene scene)
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
            if (showHint) drawHint(display, mode);
            if (showAppearanceHint) drawAppearanceHint(display);
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

void Renderer::renderInteractiveDirty(const Engine& engine, InteractionMode mode, bool showHint,
                                      bool showAppearanceHint)
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
    if (_appearanceNeedsFullRefresh) markRange(0, _height - 1);
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
        markRange(oldY - kSelectorGlowRadius - 1, oldY + kSelectorGlowRadius + 1);
        markRange(newY - kSelectorGlowRadius - 1, newY + kSelectorGlowRadius + 1);
    }

    for (std::size_t i = 0; i < engine.dotCount(); ++i) {
        const Dot& dot = engine.dots()[i];
        const uint8_t level = levelForDot(dot, i, RenderScene::Interactive);
        const uint8_t colorIndex = visualKeyForDot(dot, RenderScene::Interactive);
        if (!_displayedLevelsValid || level != _displayedLevels[i] ||
            colorIndex != _displayedColorIndexes[i]) {
            markRange(dot.y - kMaxDotRadius, dot.y + kMaxDotRadius);
        }
        _displayedLevels[i] = level;
        _displayedColorIndexes[i] = colorIndex;
    }

    const uint8_t hint = hintCode(showAppearanceHint, showHint, mode);
    if (!_displayedLevelsValid || hint != _displayedHint) {
        markRange(_height - 27, _height - 3);
    }
    _displayedHint = hint;
    _displayedLevelsValid = true;

    std::size_t dirtyCount = 0;
    for (std::size_t i = 0; i < bandCount; ++i) {
        if (dirty[i]) ++dirtyCount;
    }
    if (dirtyCount * 10 >= bandCount * 7) {
        dirty.fill(true);
        dirtyCount = bandCount;
    }
    _dirtyBandTotal += dirtyCount;
    ++_dirtyBandSamples;

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
        if (showHint && _height - 27 < bottom && _height - 3 >= top) drawHint(display, mode);
        if (showAppearanceHint && _height - 31 < bottom && _height - 3 >= top) {
            drawAppearanceHint(display);
        }
        if (_colorSelectionMode) drawColorRing(display, top, bottom);
        display.clearClipRect();
        display.endWrite();
        display.display(0, top, _width, bottom - top);
        first = last + 1;
    }
    _ringNeedsFullRefresh = false;
    _appearanceNeedsFullRefresh = false;
    _displayedSelectorHue = _selectedHue;
    GetHAL().feedTheDog();
}

float Renderer::dirtyBandsAverage() const
{
    if (_dirtyBandSamples == 0) return 0.0f;
    return static_cast<float>(_dirtyBandTotal) / static_cast<float>(_dirtyBandSamples);
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
    const float dirtyBandsAvg = _dirtyBandSamples == 0
                                    ? 0.0f
                                    : static_cast<float>(_dirtyBandTotal) /
                                          static_cast<float>(_dirtyBandSamples);
    mclog::tagInfo("GlowField",
                   "scene={} {}x{} dots={} fps={:.1f} avg={:.2f}ms max={:.2f}ms dirtyBandsAvg={:.1f}",
                   sceneName(scene), _width, _height, _dotCount, fps, averageMs,
                   static_cast<float>(_maxFrameTimeUs) / 1000.0f, dirtyBandsAvg);

    _statsStartedMs = nowMs;
    _frameCount = 0;
    _frameTimeTotalUs = 0;
    _maxFrameTimeUs = 0;
    _dirtyBandTotal = 0;
    _dirtyBandSamples = 0;
}

}  // namespace glow_field
