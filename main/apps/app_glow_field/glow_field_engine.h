#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace glow_field {

struct Dot {
    int16_t x = 0;
    int16_t y = 0;
    uint8_t energy = 0;
    uint8_t colorIndex = 5;
    uint8_t rippleEnergy = 0;
    uint8_t rippleColorIndex = 5;
    bool visible = false;
};

class Engine {
public:
    static constexpr std::size_t kMaxDots = 512;
    static constexpr std::size_t kMaxRipples = 6;

    void reset(int width, int height);
    void clear();
    void update(uint32_t nowMs);
    void triggerRipple(int x, int y, uint32_t nowMs, uint8_t colorIndex);
    void beginTouch(int x, int y, uint32_t nowMs, uint8_t colorIndex);
    void moveTouch(int x, int y, uint32_t nowMs);
    void endTouch();

    const std::array<Dot, kMaxDots>& dots() const { return _dots; }
    std::size_t dotCount() const { return _dotCount; }
    bool touching() const { return _touching; }

private:
    struct Ripple {
        int16_t x = 0;
        int16_t y = 0;
        int16_t reflectedX = 0;
        int16_t reflectedY = 0;
        uint32_t startedMs = 0;
        uint32_t seed = 0;
        uint16_t durationMs = 0;
        uint16_t maxRadius = 0;
        uint8_t colorIndex = 5;
        bool hasReflection = false;
        bool active = false;
    };

    void startRipple(int x, int y, uint32_t nowMs, uint8_t colorIndex, uint16_t durationMs,
                     uint16_t maxRadius);
    void injectPoint(int x, int y, uint8_t colorIndex, int radius);
    void injectImpact(int x, int y, uint8_t colorIndex);
    void stepSimulation();
    void updateRipples(uint32_t nowMs);

    std::array<Dot, kMaxDots> _dots = {};
    std::array<Ripple, kMaxRipples> _ripples = {};
    std::size_t _dotCount = 0;
    std::size_t _nextRipple = 0;
    uint32_t _lastUpdateMs = 0;
    uint32_t _simulationAccumulatorMs = 0;
    uint32_t _decayRemainder = 0;
    int _maxRippleRadius = 0;
    int _centerX = 0;
    int _centerY = 0;
    int _fieldRadius = 0;
    int _lastTouchX = 0;
    int _lastTouchY = 0;
    uint8_t _touchColorIndex = 5;
    bool _touching = false;
};

}  // namespace glow_field
