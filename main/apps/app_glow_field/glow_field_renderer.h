#pragma once

#include "glow_field_engine.h"

#include <M5GFX.h>
#include <array>
#include <cstdint>

namespace glow_field {

enum class RenderScene : uint8_t {
    Interactive,
    Solid,
    IdleDots,
    EnergizedDots,
};

class Renderer {
public:
    void open(int width, int height, std::size_t dotCount);
    void close();
    void render(const Engine& engine, uint32_t nowMs, bool rippleMode, uint32_t modeNoticeUntilMs,
                RenderScene scene, bool colorSelectionMode, uint16_t selectedHue);

private:
    struct GlowColors {
        uint16_t outer = 0;
        uint16_t middle = 0;
        uint16_t inner = 0;
        uint16_t core = 0;
    };

    static constexpr std::size_t kRingSegments = 120;
    static constexpr std::size_t kHueSlots = 24;
    static constexpr std::size_t kRingRadialBands = 1;

    struct RingSegment {
        std::array<int16_t, kRingRadialBands + 1> startX = {};
        std::array<int16_t, kRingRadialBands + 1> startY = {};
        std::array<int16_t, kRingRadialBands + 1> endX = {};
        std::array<int16_t, kRingRadialBands + 1> endY = {};
        std::array<uint16_t, kRingRadialBands> colors = {};
        int16_t minY = 0;
        int16_t maxY = 0;
    };

    std::array<std::array<GlowColors, 16>, kHueSlots> _palettes = {};
    std::array<RingSegment, kRingSegments> _ringSegments = {};
    uint32_t _statsStartedMs = 0;
    uint32_t _frameCount = 0;
    uint64_t _frameTimeTotalUs = 0;
    uint32_t _maxFrameTimeUs = 0;
    RenderScene _activeScene = RenderScene::Interactive;
    bool _sceneInitialized = false;
    bool _fullFrameReady = false;
    int _width = 0;
    int _height = 0;
    std::size_t _dotCount = 0;
    uint32_t _lastFrameMs = 0;
    std::array<uint8_t, Engine::kMaxDots> _displayedLevels = {};
    std::array<uint8_t, Engine::kMaxDots> _displayedColorIndexes = {};
    bool _displayedLevelsValid = false;
    uint8_t _displayedHint = 0;
    bool _colorSelectionMode = false;
    bool _ringNeedsFullRefresh = false;
    uint16_t _selectedHue = 79;
    uint16_t _displayedSelectorHue = 79;

    void reportStats(uint32_t nowMs, RenderScene scene);
    void buildPalettes();
    void updateColorSelection(bool enabled, uint16_t selectedHue);
    uint8_t levelForDot(const Dot& dot, std::size_t index, RenderScene scene) const;
    uint8_t colorIndexForDot(const Dot& dot, RenderScene scene) const;
    void drawDot(LGFX_Device& target, const Dot& dot, uint8_t level, uint16_t idleColor) const;
    void drawHint(LGFX_Device& target, bool rippleMode) const;
    void drawColorRing(LGFX_Device& target, int top, int bottom) const;
    void renderFullFrame(const Engine& engine, bool rippleMode, bool showHint, RenderScene scene);
    void renderInteractiveDirty(const Engine& engine, bool rippleMode, bool showHint);
};

}  // namespace glow_field
