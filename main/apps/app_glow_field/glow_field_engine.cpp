#include "glow_field_engine.h"

#include <algorithm>
#include <cmath>

namespace glow_field {
namespace {

constexpr int kGridSpacingX = 29;
constexpr int kGridSpacingY = 25;
constexpr int kTouchRadius = 81;
constexpr int kPathStep = kGridSpacingX / 2;
constexpr uint32_t kSimulationStepMs = 16;
constexpr uint32_t kDecayPerSecond = 180;
constexpr uint32_t kRippleTravelMs = 880;
constexpr uint32_t kRippleAfterglowMs = 680;
constexpr uint32_t kRippleDurationMs = kRippleTravelMs + kRippleAfterglowMs;
constexpr int kImpactRadius = 42;
constexpr uint32_t kRippleAttackMs = 48;
constexpr int kScatterTimeMs = 55;
constexpr int kReflectionStrength = 72;
constexpr int kSymbolCount = static_cast<int>(kSymbolGlyphCount);
constexpr uint32_t kSymbolMutationStepMs = 72;

int approximateDistance(int dx, int dy)
{
    const int absoluteX = std::abs(dx);
    const int absoluteY = std::abs(dy);
    const int larger = std::max(absoluteX, absoluteY);
    const int smaller = std::min(absoluteX, absoluteY);
    // Within a few percent of Euclidean distance and substantially cheaper
    // than running sqrt for every dot of every concurrent ripple.
    return larger + (smaller * 3) / 8;
}

uint32_t mixHash(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7FEB352Du;
    value ^= value >> 15;
    value *= 0x846CA68Bu;
    return value ^ (value >> 16);
}

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
    _maxRippleRadius = std::max(width, height) + kGridSpacingX;
    _centerX = width / 2;
    _centerY = height / 2;
    _fieldRadius = std::min(width, height) / 2 - 5;

    const int centerX = _centerX;
    const int centerY = _centerY;
    const int visibleRadius = _fieldRadius;
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
            dot.colorIndex = 5;
            dot.rippleColorIndex = 5;
            const uint32_t symbolSeed = mixHash(static_cast<uint32_t>(_dotCount) * 0x9E3779B9u ^
                                                static_cast<uint32_t>(x * 257 + y));
            dot.symbolIndex = static_cast<uint8_t>(symbolSeed % kSymbolCount);
            dot.symbolColorIndex = static_cast<uint8_t>((symbolSeed >> 8) % kSymbolCount);
            dot.energySymbolIndex = dot.symbolIndex;
            dot.energySymbolColorIndex = dot.symbolColorIndex;
            dot.rippleSymbolIndex = dot.symbolIndex;
            dot.rippleSymbolColorIndex = dot.symbolColorIndex;
            dot.visible = true;
        }
    }
}

void Engine::clear()
{
    for (std::size_t i = 0; i < _dotCount; ++i) {
        _dots[i].energy = 0;
        _dots[i].rippleEnergy = 0;
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

    // Only button-generated SymbolMix paint points opt into mutation. Their
    // Paint energy envelope remains untouched, while the per-dot phase keeps
    // changes from stepping in lockstep.
    for (std::size_t i = 0; i < _dotCount; ++i) {
        Dot& dot = _dots[i];
        if (dot.energy == 0 || !dot.energyUsesSymbolPalette) continue;
        const uint32_t seed = mixHash(static_cast<uint32_t>(i + 1) * 0x9E3779B9u);
        const uint32_t mutationStep = (nowMs + seed % kSymbolMutationStepMs) /
                                      kSymbolMutationStepMs;
        const uint32_t mutationHash = mixHash(seed ^ (mutationStep * 0xA511E9B3u));
        dot.energySymbolIndex = static_cast<uint8_t>(mutationHash % kSymbolCount);
        dot.energySymbolColorIndex = static_cast<uint8_t>((mutationHash >> 9) % kSymbolCount);
    }
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

void Engine::triggerRipple(int x, int y, uint32_t nowMs, uint8_t colorIndex, bool symbolMix,
                           bool mutateSymbols)
{
    startRipple(x, y, nowMs, colorIndex, kRippleDurationMs,
                static_cast<uint16_t>(_maxRippleRadius), symbolMix, mutateSymbols);
}

void Engine::triggerPaintPoint(int x, int y, uint32_t nowMs, uint8_t colorIndex,
                               bool symbolMix, bool mutateSymbols)
{
    injectPoint(x, y, colorIndex, kTouchRadius, nowMs, symbolMix, mutateSymbols);
}

void Engine::startRipple(int x, int y, uint32_t nowMs, uint8_t colorIndex, uint16_t durationMs,
                         uint16_t maxRadius, bool symbolMix, bool mutateSymbols)
{
    Ripple& ripple = _ripples[_nextRipple];
    ripple.x = static_cast<int16_t>(x);
    ripple.y = static_cast<int16_t>(y);
    ripple.reflectedX = ripple.x;
    ripple.reflectedY = ripple.y;
    ripple.startedMs = nowMs;
    ripple.seed = mixHash(nowMs ^ (static_cast<uint32_t>(x) << 16) ^
                          static_cast<uint32_t>(y));
    ripple.durationMs = durationMs;
    ripple.maxRadius = maxRadius;
    ripple.colorIndex = colorIndex;
    ripple.paletteOffset = static_cast<uint8_t>((ripple.seed >> 24) % kSymbolCount);
    ripple.hasReflection = false;
    ripple.symbolMix = symbolMix;
    ripple.mutateSymbols = symbolMix && mutateSymbols;

    // Mirror a near-edge source across the circular boundary. The weaker image
    // source makes the returning motion visible without drawing a hard second
    // ring or requiring per-pixel clipping/reflection math.
    const int centerDx = x - _centerX;
    const int centerDy = y - _centerY;
    const int centerDistance = approximateDistance(centerDx, centerDy);
    if (centerDistance > _fieldRadius * 2 / 3 && centerDistance > 0) {
        const int boundaryX = _centerX + centerDx * _fieldRadius / centerDistance;
        const int boundaryY = _centerY + centerDy * _fieldRadius / centerDistance;
        ripple.reflectedX = static_cast<int16_t>(2 * boundaryX - x);
        ripple.reflectedY = static_cast<int16_t>(2 * boundaryY - y);
        ripple.hasReflection = true;
    }
    ripple.active = true;
    _nextRipple = (_nextRipple + 1) % kMaxRipples;

    // Keep a restrained distance-weighted cluster at the source while the
    // travelling field lights nearby dots on subsequent simulation steps.
    if (!symbolMix) injectImpact(x, y, colorIndex);
}

void Engine::beginTouch(int x, int y, uint32_t nowMs, uint8_t colorIndex)
{
    _touching = true;
    _lastTouchX = x;
    _lastTouchY = y;
    _touchColorIndex = colorIndex;
    injectPoint(x, y, _touchColorIndex, kTouchRadius, nowMs, false, false);
}

void Engine::moveTouch(int x, int y, uint32_t nowMs)
{
    if (!_touching) {
        beginTouch(x, y, nowMs, _touchColorIndex);
        return;
    }

    const int dx = x - _lastTouchX;
    const int dy = y - _lastTouchY;
    const int distance = static_cast<int>(std::sqrt(static_cast<float>(dx * dx + dy * dy)));
    const int steps = std::max(1, (distance + kPathStep - 1) / kPathStep);
    for (int step = 1; step <= steps; ++step) {
        injectPoint(_lastTouchX + dx * step / steps, _lastTouchY + dy * step / steps,
                    _touchColorIndex, kTouchRadius, nowMs, false, false);
    }

    _lastTouchX = x;
    _lastTouchY = y;
}

void Engine::endTouch()
{
    _touching = false;
}

void Engine::injectPoint(int x, int y, uint8_t colorIndex, int radius, uint32_t nowMs,
                         bool symbolMix, bool mutateSymbols)
{
    const int radiusSquared = radius * radius;
    for (std::size_t i = 0; i < _dotCount; ++i) {
        Dot& dot = _dots[i];
        const int dx = static_cast<int>(dot.x) - x;
        const int dy = static_cast<int>(dot.y) - y;
        const int distanceSquared = dx * dx + dy * dy;
        if (distanceSquared > radiusSquared) continue;

        const int falloff = radiusSquared - distanceSquared;
        const int curved = (falloff * falloff) / radiusSquared;
        const int injected = 42 + curved * 213 / radiusSquared;
        dot.energy = static_cast<uint8_t>(std::max<int>(dot.energy, injected));
        dot.colorIndex = colorIndex;
        dot.energyUsesSymbolPalette = symbolMix && mutateSymbols;
        if (dot.energyUsesSymbolPalette) {
            const uint32_t seed = mixHash(static_cast<uint32_t>(i + 1) * 0x9E3779B9u);
            const uint32_t mutationStep = (nowMs + seed % kSymbolMutationStepMs) /
                                          kSymbolMutationStepMs;
            const uint32_t mutationHash = mixHash(seed ^ (mutationStep * 0xA511E9B3u));
            dot.energySymbolIndex = static_cast<uint8_t>(mutationHash % kSymbolCount);
            dot.energySymbolColorIndex = static_cast<uint8_t>((mutationHash >> 9) % kSymbolCount);
        }
    }
}

void Engine::injectImpact(int x, int y, uint8_t colorIndex)
{
    const int radiusSquared = kImpactRadius * kImpactRadius;
    for (std::size_t i = 0; i < _dotCount; ++i) {
        Dot& dot = _dots[i];
        const int dx = static_cast<int>(dot.x) - x;
        const int dy = static_cast<int>(dot.y) - y;
        const int distanceSquared = dx * dx + dy * dy;
        if (distanceSquared > radiusSquared) continue;
        const int energy = 80 + (radiusSquared - distanceSquared) * 145 / radiusSquared;
        dot.energy = static_cast<uint8_t>(std::max<int>(dot.energy, energy));
        dot.colorIndex = colorIndex;
    }
}

void Engine::updateRipples(uint32_t nowMs)
{
    // Ripple stays transient and is recomputed from time, but each dot retains
    // a soft afterglow after the travelling front reaches it. This produces a
    // coherent energy field instead of several disconnected geometric rings.
    for (std::size_t i = 0; i < _dotCount; ++i) {
        _dots[i].rippleEnergy = 0;
        _dots[i].rippleUsesSymbolPalette = false;
    }

    for (Ripple& ripple : _ripples) {
        if (!ripple.active) continue;

        const uint32_t ageMs = nowMs - ripple.startedMs;
        const uint32_t durationMs = std::max<uint32_t>(1, ripple.durationMs);
        if (ageMs >= durationMs) {
            ripple.active = false;
            continue;
        }

        for (std::size_t i = 0; i < _dotCount; ++i) {
            Dot& dot = _dots[i];
            const int dx = static_cast<int>(dot.x) - ripple.x;
            const int dy = static_cast<int>(dot.y) - ripple.y;
            const int distance = approximateDistance(dx, dy);
            const uint32_t hash = mixHash(ripple.seed ^ static_cast<uint32_t>(i) * 0x9E3779B9u);
            const int jitterMs = static_cast<int>(hash % (kScatterTimeMs * 2 + 1)) -
                                 kScatterTimeMs;
            const int arrivalMs = std::max(0, distance * static_cast<int>(kRippleTravelMs) /
                                                  std::max<int>(1, ripple.maxRadius) +
                                                  jitterMs);
            const int primarySourceAgeMs = static_cast<int>(ageMs) - arrivalMs;

            auto energyAfterArrival = [&](int sourceAgeMs, int sourceDistance,
                                          int strength) -> int {
                if (sourceAgeMs < 0 || sourceAgeMs >= static_cast<int>(kRippleAfterglowMs)) return 0;
                const int distanceScale = std::max(145, 245 - sourceDistance * 90 /
                                                               std::max<int>(1, ripple.maxRadius));
                const int scatterScale = 184 + static_cast<int>((hash >> 8) & 0x2F);
                int envelope = 255;
                if (sourceAgeMs < static_cast<int>(kRippleAttackMs)) {
                    envelope = 72 + sourceAgeMs * 183 / static_cast<int>(kRippleAttackMs);
                } else {
                    const int remaining = static_cast<int>(kRippleAfterglowMs) - sourceAgeMs;
                    const int fadeDuration = static_cast<int>(kRippleAfterglowMs - kRippleAttackMs);
                    envelope = remaining * remaining * 255 / (fadeDuration * fadeDuration);
                }
                return envelope * distanceScale / 255 * scatterScale / 255 * strength / 255;
            };

            int symbolSourceAgeMs = primarySourceAgeMs;
            int energy = energyAfterArrival(primarySourceAgeMs, distance, 255);
            if (ripple.hasReflection) {
                const int reflectedDx = static_cast<int>(dot.x) - ripple.reflectedX;
                const int reflectedDy = static_cast<int>(dot.y) - ripple.reflectedY;
                const int reflectedDistance = approximateDistance(reflectedDx, reflectedDy);
                const int reflectedArrival = reflectedDistance * static_cast<int>(kRippleTravelMs) /
                                             std::max<int>(1, ripple.maxRadius) - jitterMs / 2;
                const int reflectedSourceAgeMs = static_cast<int>(ageMs) - reflectedArrival;
                const int reflectedEnergy = energyAfterArrival(reflectedSourceAgeMs,
                                                               reflectedDistance,
                                                               kReflectionStrength);
                if (reflectedEnergy > energy) {
                    energy = reflectedEnergy;
                    symbolSourceAgeMs = reflectedSourceAgeMs;
                }
            }

            uint8_t symbolIndex = static_cast<uint8_t>(hash % kSymbolCount);
            uint8_t symbolColorIndex = static_cast<uint8_t>(((hash >> 8) + ripple.paletteOffset +
                                                             distance / 58) %
                                                            kSymbolCount);
            if (ripple.symbolMix) {
                if (distance <= kImpactRadius && ageMs < 360u) {
                    const int impactFalloff = (kImpactRadius * kImpactRadius - distance * distance) *
                                              145 / (kImpactRadius * kImpactRadius);
                    const int impactEnergy = std::max(0, 225 - static_cast<int>(ageMs) * 145 / 360) *
                                             (110 + impactFalloff) / 255;
                    energy = std::max(energy, impactEnergy);
                    symbolSourceAgeMs = static_cast<int>(ageMs);
                }

                // Preserve the original stationary fade envelope. During that
                // fade, only the glyph identity changes: each illuminated dot
                // walks its own deterministic pseudo-random symbol/color
                // sequence without injecting extra brightness or movement.
                if (ripple.mutateSymbols &&
                    symbolSourceAgeMs >= static_cast<int>(kRippleAttackMs) &&
                    symbolSourceAgeMs < static_cast<int>(kRippleAfterglowMs)) {
                    const uint32_t mutationStep = static_cast<uint32_t>(
                        symbolSourceAgeMs - static_cast<int>(kRippleAttackMs)) /
                        kSymbolMutationStepMs;
                    const uint32_t mutationHash = mixHash(
                        hash ^ ((mutationStep + 1u) * 0xA511E9B3u));
                    symbolIndex = static_cast<uint8_t>(mutationHash % kSymbolCount);
                    symbolColorIndex = static_cast<uint8_t>((mutationHash >> 9) % kSymbolCount);
                }
            }

            if (energy > dot.rippleEnergy) {
                dot.rippleEnergy = static_cast<uint8_t>(std::min(255, energy));
                dot.rippleColorIndex = ripple.colorIndex;
                dot.rippleUsesSymbolPalette = ripple.symbolMix;
                dot.rippleSymbolIndex = symbolIndex;
                dot.rippleSymbolColorIndex = symbolColorIndex;
            }
        }
    }
}

}  // namespace glow_field
