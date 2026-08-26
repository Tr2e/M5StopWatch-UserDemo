#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace glow_field {

struct Dot {
    int16_t x = 0;
    int16_t y = 0;
    uint8_t energy = 0;
    uint8_t hueIndex = 0;
    bool visible = false;
};

class Engine {
public:
    static constexpr std::size_t kMaxDots = 512;
    static constexpr std::size_t kMaxRipples = 4;

    void reset(int width, int height);
    void clear();
    void update(uint32_t nowMs);
    void triggerRipple(int x, int y, uint32_t nowMs);
    void beginTouch(int x, int y, uint32_t nowMs);
    void moveTouch(int x, int y, uint32_t nowMs);
    void endTouch();

    const std::array<Dot, kMaxDots>& dots() const { return _dots; }
    std::size_t dotCount() const { return _dotCount; }
    bool touching() const { return _touching; }

private:
    struct Ripple {
        int16_t x = 0;
        int16_t y = 0;
        uint32_t startedMs = 0;
        bool active = false;
    };

    void injectPoint(int x, int y, uint32_t nowMs);
    void updateRipples(uint32_t nowMs);

    std::array<Dot, kMaxDots> _dots = {};
    std::array<Ripple, kMaxRipples> _ripples = {};
    std::size_t _dotCount = 0;
    std::size_t _nextRipple = 0;
    uint32_t _lastUpdateMs = 0;
    int _maxRippleRadius = 0;
    int _lastTouchX = 0;
    int _lastTouchY = 0;
    bool _touching = false;
};

}  // namespace glow_field
