#include "glow_field_engine.h"

#include <algorithm>
#include <cmath>

namespace glow_field {
namespace {

constexpr int kGridSpacingX = 22;
constexpr int kGridSpacingY = 19;
constexpr int kTouchRadius = 62;
constexpr int kTouchRadiusSquared = kTouchRadius * kTouchRadius;
constexpr int kPathStep = kGridSpacingX / 2;
constexpr uint32_t kSimulationStepMs = 16;
constexpr uint32_t kDecayPerSecond = 180;
constexpr uint32_t kRippleDurationMs = 1050;
constexpr int kRippleHalfWidth = 17;

}  // namespace

void Engine::reset(int width, int height)
{
    _dots = {};
    _dotCount = 0;
    _ripples = {};
    _nextRipple = 0;
    _lastUpdateMs = 0;
    _simulationAccumulatorMs = 0;
    _decayRemainder = 0;
    _touching = false;
    _maxRippleRadius = static_cast<int>(std::sqrt(static_cast<float>(width * width + height * height))) +
                       kGridSpacingX;

    const int centerX = width / 2;
    const int centerY = height / 2;
    const int visibleRadius = std::min(width, height) / 2 - 5;
    const int visibleRadiusSquared = visibleRadius * visibleRadius;

    int row = 0;
    const int startX = (width % kGridSpacingX) / 2;
    const int startY = (height % kGridSpacingY) / 2;
    for (int y = startY; y < height && _dotCount < kMaxDots; y += kGridSpacingY, ++row) {
        const int rowOffset = (row & 1) ? kGridSpacingX / 2 : 0;
        for (int x = startX + rowOffset; x < width && _dotCount < kMaxDots; x += kGridSpacingX) {
            const int dx = x - centerX;
            const int dy = y - centerY;
            if (dx * dx + dy * dy > visibleRadiusSquared) continue;

            Dot& dot = _dots[_dotCount++];
            dot.x = static_cast<int16_t>(x);
            dot.y = static_cast<int16_t>(y);
            dot.hueIndex = static_cast<uint8_t>((x / kGridSpacingX + row) & 0x0F);
            dot.visible = true;
        }
    }
}

void Engine::clear()
{
    for (std::size_t i = 0; i < _dotCount; ++i) {
        _dots[i].energy = 0;
    }
    _ripples = {};
    _nextRipple = 0;
    _decayRemainder = 0;
    _touching = false;
}

void Engine::update(uint32_t nowMs)
{
    if (_lastUpdateMs == 0) {
        _lastUpdateMs = nowMs;
        updateRipples(nowMs);
        return;
    }

    const uint32_t elapsedMs = std::min<uint32_t>(nowMs - _lastUpdateMs, 100);
    _lastUpdateMs = nowMs;
    _simulationAccumulatorMs += elapsedMs;
    bool stepped = false;
    while (_simulationAccumulatorMs >= kSimulationStepMs) {
        _simulationAccumulatorMs -= kSimulationStepMs;
        stepSimulation();
        stepped = true;
    }
    if (stepped) updateRipples(nowMs);
}

void Engine::stepSimulation()
{
    // Keep the fractional decay shared by all dots. This preserves the intended
    // 180 energy units per second without rounding every main-loop iteration up
    // to one unit, so the trail lifetime is independent of loop frequency.
    _decayRemainder += kSimulationStepMs * kDecayPerSecond;
    const uint16_t decay = static_cast<uint16_t>(_decayRemainder / 1000u);
    _decayRemainder %= 1000u;
    if (decay > 0) {
        for (std::size_t i = 0; i < _dotCount; ++i) {
            Dot& dot = _dots[i];
            dot.energy = dot.energy > decay ? static_cast<uint8_t>(dot.energy - decay) : 0;
        }
    }
}

void Engine::triggerRipple(int x, int y, uint32_t nowMs)
{
    Ripple& ripple = _ripples[_nextRipple];
    ripple.x = static_cast<int16_t>(x);
    ripple.y = static_cast<int16_t>(y);
    ripple.startedMs = nowMs;
    ripple.active = true;
    _nextRipple = (_nextRipple + 1) % kMaxRipples;

    // A compact flash anchors the click while the ring starts expanding.
    injectPoint(x, y, nowMs);
}

void Engine::beginTouch(int x, int y, uint32_t nowMs)
{
    _touching = true;
    _lastTouchX = x;
    _lastTouchY = y;
    injectPoint(x, y, nowMs);
}

void Engine::moveTouch(int x, int y, uint32_t nowMs)
{
    if (!_touching) {
        beginTouch(x, y, nowMs);
        return;
    }

    const int dx = x - _lastTouchX;
    const int dy = y - _lastTouchY;
    const int distance = static_cast<int>(std::sqrt(static_cast<float>(dx * dx + dy * dy)));
    const int steps = std::max(1, (distance + kPathStep - 1) / kPathStep);
    for (int step = 1; step <= steps; ++step) {
        injectPoint(_lastTouchX + dx * step / steps, _lastTouchY + dy * step / steps, nowMs);
    }

    _lastTouchX = x;
    _lastTouchY = y;
}

void Engine::endTouch()
{
    _touching = false;
}

void Engine::injectPoint(int x, int y, uint32_t nowMs)
{
    for (std::size_t i = 0; i < _dotCount; ++i) {
        Dot& dot = _dots[i];
        const int dx = static_cast<int>(dot.x) - x;
        const int dy = static_cast<int>(dot.y) - y;
        const int distanceSquared = dx * dx + dy * dy;
        if (distanceSquared > kTouchRadiusSquared) continue;

        const int falloff = kTouchRadiusSquared - distanceSquared;
        const int curved = (falloff * falloff) / kTouchRadiusSquared;
        const int injected = 42 + curved * 213 / kTouchRadiusSquared;
        dot.energy = static_cast<uint8_t>(std::max<int>(dot.energy, injected));
        dot.hueIndex = static_cast<uint8_t>(((nowMs / 180u) + i) & 0x0F);
    }
}

void Engine::updateRipples(uint32_t nowMs)
{
    for (Ripple& ripple : _ripples) {
        if (!ripple.active) continue;

        const uint32_t ageMs = nowMs - ripple.startedMs;
        if (ageMs >= kRippleDurationMs) {
            ripple.active = false;
            continue;
        }

        const int radius = static_cast<int>(ageMs * static_cast<uint32_t>(_maxRippleRadius) /
                                            kRippleDurationMs);
        const int innerRadius = std::max(0, radius - kRippleHalfWidth);
        const int outerRadius = radius + kRippleHalfWidth;
        const int innerSquared = innerRadius * innerRadius;
        const int outerSquared = outerRadius * outerRadius;
        const int radiusSquared = radius * radius;
        const int bandSquared = std::max(1, outerSquared - radiusSquared);
        const int baseEnergy = 72 + static_cast<int>((kRippleDurationMs - ageMs) * 183u /
                                                     kRippleDurationMs);

        for (std::size_t i = 0; i < _dotCount; ++i) {
            Dot& dot = _dots[i];
            const int dx = static_cast<int>(dot.x) - ripple.x;
            const int dy = static_cast<int>(dot.y) - ripple.y;
            const int distanceSquared = dx * dx + dy * dy;
            if (distanceSquared < innerSquared || distanceSquared > outerSquared) continue;

            // Brightest at the wave front, softer at both edges of the band.
            // Squared distances keep the per-frame ripple pass free of sqrt.
            const int frontDistance = std::abs(distanceSquared - radiusSquared);
            const int energy = baseEnergy * std::max(0, bandSquared - frontDistance) / bandSquared;
            dot.energy = static_cast<uint8_t>(std::max<int>(dot.energy, std::max(30, energy)));
            dot.hueIndex = static_cast<uint8_t>(((ripple.startedMs / 180u) + i) & 0x0F);
        }
    }
}

}  // namespace glow_field
