#pragma once

#include "glow_field_engine.h"

#include <array>
#include <cstdint>

namespace glow_field {

class Renderer {
public:
    void open(int width, int height, std::size_t dotCount);
    void render(const Engine& engine, uint32_t nowMs, bool rippleMode, uint32_t modeNoticeUntilMs);

private:
    struct GlowColors {
        uint16_t outer = 0;
        uint16_t middle = 0;
        uint16_t inner = 0;
        uint16_t core = 0;
    };

    std::array<GlowColors, 16> _palette = {};
    uint32_t _statsStartedMs = 0;
    uint32_t _frameCount = 0;
    uint64_t _frameTimeTotalUs = 0;
    uint32_t _maxFrameTimeUs = 0;
    int _width = 0;
    int _height = 0;
    std::size_t _dotCount = 0;
    uint32_t _lastFrameMs = 0;

    void reportStats(uint32_t nowMs);
};

}  // namespace glow_field
