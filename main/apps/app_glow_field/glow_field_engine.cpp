#include "glow_field_engine.h"

#include <algorithm>
#include <cmath>

namespace glow_field {
namespace {

constexpr int kGridSpacingX = 29;
constexpr int kGridSpacingY = 25;
constexpr int kTouchRadius = 68;
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

uint8_t activeSymbolColor(uint32_t hash, bool allowYellow)
{
    const uint32_t roll = hash % 100u;
    if (roll < 34u) return 0;  // cyber cyan
    if (roll < 68u) return 1;  // neon magenta
    if (roll < 90u || !allowYellow) return 2;  // proton green
    return 3;  // electric yellow highlight
}

uint8_t paintSymbolColor(uint32_t hash)
{
    const uint32_t roll = hash % 100u;
    if (roll < 30u) return 0;
    if (roll < 58u) return 1;
    if (roll < 82u) return 2;
    return 3;  // stable yellow sparks in touch Paint
}

uint8_t rippleSymbolColor(int distance, int maxRadius, uint32_t hash, bool mutateSymbols)
{
    if (distance <= kImpactRadius) {
        // Yellow is most effective as a compact warm spark against the
        // magenta impact core. It participates in both touch and K1 ripples.
        const uint32_t yellowChance = mutateSymbols ? 16u : 24u;
        if (((hash >> 16) % 100u) < yellowChance) return 3;
        return 1;
    }
    if (distance * 3 >= maxRadius * 2) {
        // The far field reads primarily as a clean cyan expansion, with a
        // little green and yellow variation so it does not become a hard ring.
        const uint32_t roll = (hash >> 11) % 100u;
        if (roll < 68u) return 0;
        if (roll < 88u) return 2;
        return 3;
    }
    const uint32_t roll = (hash >> 8) % 100u;
    if (roll < 30u) return 0;
    if (roll < 58u) return 1;
    if (roll < 84u) return 2;
    return 3;
}

constexpr float kPi = 3.14159265358979323846f;
constexpr uint8_t kAudioYellowCapPercent = 18;
constexpr uint8_t kHueSlots = 24;

uint8_t assignAudioBand(int x, int y, int centerX, int centerY, int fieldRadius, uint32_t hash)
{
    const int dist = approximateDistance(x - centerX, y - centerY);
    const float radial = std::min(1.0f, static_cast<float>(dist) /
                                            static_cast<float>(std::max(1, fieldRadius)));
    const float jitter = (static_cast<float>(hash % 1000u) / 1000.0f - 0.5f) * 0.32f;
    const float r = std::clamp(radial + jitter, 0.0f, 1.0f);
    const uint32_t roll = (hash >> 11) % 100u;
    const uint32_t diag = mixHash(static_cast<uint32_t>(x + y) * 0x85EBCA6Bu);
    const bool midCluster = ((diag >> 8) % 100u) < 38u;

    uint8_t group = 1;
    if (r < 0.34f) {
        if (roll < 62u) group = 0;
        else if (roll < 88u) group = 1;
        else group = midCluster ? 2 : 1;
    } else if (r < 0.68f) {
        if (midCluster && roll < 40u) group = 2;
        else if (roll < 18u) group = 0;
        else if (roll < 72u) group = 1;
        else group = 2;
    } else {
        if (roll < 22u) group = 1;
        else if (roll < 48u && midCluster) group = 2;
        else if (roll < 58u) group = 2;
        else group = 3;
    }
    if (group == 3 && ((hash >> 5) % 100u) > 70u) group = 1;

    const uint32_t localRoll = (hash >> 3) % 100u;
    const uint8_t local = localRoll < 28u ? 0 : localRoll < 52u ? 1
                                               : localRoll < 72u ? 2
                                               : localRoll < 88u ? 3
                                                                 : 4;
    return static_cast<uint8_t>(group * 5 + local);
}

uint8_t audioFieldSymbolColor(uint8_t group, float energy, uint32_t hash)
{
    switch (group) {
        case 0:
            return energy > 0.72f && ((hash >> 16) % 100u) < 16u ? 3 : 1;
        case 1:
            return ((hash >> 9) % 100u) < 48u ? 2 : 1;
        case 2:
            return ((hash >> 9) % 100u) < 55u ? 2 : 0;
        default:
            return ((hash >> 12) % 100u) < 14u ? 3 : 0;
    }
}

uint8_t audioHueColor(uint8_t baseColorIndex, uint8_t group)
{
    static constexpr int8_t kOffsets[] = {0, 1, 2, 4};
    return static_cast<uint8_t>((baseColorIndex + kOffsets[group % 4]) % kHueSlots);
}

uint8_t quantizeAudioEnergy(uint8_t previous, int target)
{
    target = std::clamp(target, 0, 255);
    if (target == 0) return previous <= 18 ? 0 : static_cast<uint8_t>(std::max(0, previous - 18));
    if (std::abs(target - static_cast<int>(previous)) < 10 && (target >> 4) == (previous >> 4)) {
        return previous;
    }
    return static_cast<uint8_t>((target >> 4) << 4);
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
    _touchSymbolMix = false;
    _audioSparks = {};
    _lastAppliedAudioSequence = 0;
    _audioGainStartedMs = 0;
    _audioGainDurationMs = 0;
    _lastSparkMs = 0;
    _lastBloomMs = 0;
    _lastAudioFieldMapMs = 0;
    _audioMutationUntilMs = 0;
    _lastAudioRippleX = 0;
    _lastAudioRippleY = 0;
    _audioGain = 0;
    _audioGainFrom = 0;
    _audioGainTarget = 0;
    _lastAudioRippleColor = 1;
    _audioBaseColorIndex = 5;
    _audioActiveDotRatio = 0.0f;
    _audioEnabled = false;
    _audioVisualPaused = false;
    _audioNeedsClear = false;
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
            dot.symbolColorIndex = activeSymbolColor(symbolSeed >> 8, false);
            dot.energySymbolIndex = dot.symbolIndex;
            dot.energySymbolColorIndex = dot.symbolColorIndex;
            dot.rippleSymbolIndex = dot.symbolIndex;
            dot.rippleSymbolColorIndex = dot.symbolColorIndex;
            const uint32_t audioSeed = mixHash(symbolSeed ^ 0xC2B2AE35u);
            dot.audioBandIndex = assignAudioBand(x, y, centerX, centerY, visibleRadius, audioSeed);
            dot.audioSymbolIndex = dot.symbolIndex;
            dot.audioColorIndex = dot.symbolColorIndex;
            dot.audioEnergy = 0;
            dot.audioUsesSymbolPalette = false;
            dot.rippleFromAudio = false;
            dot.visible = true;
        }
    }
}

void Engine::clear()
{
    for (std::size_t i = 0; i < _dotCount; ++i) {
        _dots[i].energy = 0;
        _dots[i].rippleEnergy = 0;
        _dots[i].audioEnergy = 0;
        _dots[i].rippleFromAudio = false;
    }
    _ripples = {};
    _audioSparks = {};
    _nextRipple = 0;
    _decayRemainder = 0;
    _touching = false;
    _audioGain = 0;
    _audioGainTarget = 0;
    _audioActiveDotRatio = 0.0f;
    _audioMutationUntilMs = 0;
    _audioEnabled = false;
    _audioVisualPaused = false;
    _audioNeedsClear = false;
}

void Engine::update(uint32_t nowMs)
{
    updateAudioGain(nowMs);
    if (!_audioEnabled && _audioGain == 0 && _audioNeedsClear) {
        clearAudioReaction(true);
    }

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
        if (dot.energy == 0 || !dot.energyMutatesSymbols) continue;
        const uint32_t seed = mixHash(static_cast<uint32_t>(i + 1) * 0x9E3779B9u);
        const uint32_t mutationStep = (nowMs + seed % kSymbolMutationStepMs) /
                                      kSymbolMutationStepMs;
        const uint32_t mutationHash = mixHash(seed ^ (mutationStep * 0xA511E9B3u));
        dot.energySymbolIndex = static_cast<uint8_t>(mutationHash % kSymbolCount);
        dot.energySymbolColorIndex = activeSymbolColor(mutationHash >> 9, true);
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
                         uint16_t maxRadius, bool symbolMix, bool mutateSymbols, bool fromAudio,
                         uint8_t strength, uint16_t travelMs, uint16_t afterglowMs)
{
    const std::size_t slot = acquireRippleSlot(fromAudio);
    if (slot >= kMaxRipples) return;
    Ripple& ripple = _ripples[slot];
    ripple.x = static_cast<int16_t>(x);
    ripple.y = static_cast<int16_t>(y);
    ripple.reflectedX = ripple.x;
    ripple.reflectedY = ripple.y;
    ripple.startedMs = nowMs;
    ripple.seed = mixHash(nowMs ^ (static_cast<uint32_t>(x) << 16) ^
                          static_cast<uint32_t>(y));
    ripple.durationMs = durationMs;
    ripple.maxRadius = maxRadius;
    ripple.travelMs = travelMs == 0 ? static_cast<uint16_t>(kRippleTravelMs) : travelMs;
    ripple.afterglowMs = afterglowMs == 0 ? static_cast<uint16_t>(kRippleAfterglowMs)
                                          : afterglowMs;
    ripple.colorIndex = colorIndex;
    ripple.paletteOffset = static_cast<uint8_t>((ripple.seed >> 24) % kSymbolCount);
    ripple.strength = strength;
    ripple.hasReflection = false;
    ripple.symbolMix = symbolMix;
    ripple.mutateSymbols = symbolMix && mutateSymbols;
    ripple.fromAudio = fromAudio;

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

    // Keep a restrained distance-weighted cluster at the source while the
    // travelling field lights nearby dots on subsequent simulation steps.
    if (!symbolMix && !fromAudio) injectImpact(x, y, colorIndex);
}

void Engine::beginTouch(int x, int y, uint32_t nowMs, uint8_t colorIndex, bool symbolMix)
{
    _touching = true;
    _lastTouchX = x;
    _lastTouchY = y;
    _touchColorIndex = colorIndex;
    _touchSymbolMix = symbolMix;
    injectPoint(x, y, _touchColorIndex, kTouchRadius, nowMs, _touchSymbolMix, false);
}

void Engine::moveTouch(int x, int y, uint32_t nowMs)
{
    if (!_touching) {
        beginTouch(x, y, nowMs, _touchColorIndex, _touchSymbolMix);
        return;
    }

    const int dx = x - _lastTouchX;
    const int dy = y - _lastTouchY;
    const int distance = static_cast<int>(std::sqrt(static_cast<float>(dx * dx + dy * dy)));
    const int steps = std::max(1, (distance + kPathStep - 1) / kPathStep);
    for (int step = 1; step <= steps; ++step) {
        injectPoint(_lastTouchX + dx * step / steps, _lastTouchY + dy * step / steps,
                    _touchColorIndex, kTouchRadius, nowMs, _touchSymbolMix, false);
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
        dot.energyUsesSymbolPalette = symbolMix;
        dot.energyMutatesSymbols = symbolMix && mutateSymbols;
        if (dot.energyUsesSymbolPalette) {
            const uint32_t seed = mixHash(static_cast<uint32_t>(i + 1) * 0x9E3779B9u ^
                                          static_cast<uint32_t>(x * 257 + y));
            const uint32_t mutationStep = (nowMs + seed % kSymbolMutationStepMs) /
                                          kSymbolMutationStepMs;
            const uint32_t mutationHash = mixHash(seed ^ (mutationStep * 0xA511E9B3u));
            if (mutateSymbols) {
                dot.energySymbolIndex = static_cast<uint8_t>(mutationHash % kSymbolCount);
                dot.energySymbolColorIndex = activeSymbolColor(mutationHash >> 9, true);
            } else {
                dot.energySymbolIndex = dot.symbolIndex;
                dot.energySymbolColorIndex = paintSymbolColor(seed >> 7);
            }
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
        _dots[i].rippleFromAudio = false;
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
            const int travelMs = std::max<int>(1, ripple.travelMs);
            const int afterglowMs = std::max<int>(1, ripple.afterglowMs);
            const int arrivalMs = std::max(0, distance * travelMs /
                                                  std::max<int>(1, ripple.maxRadius) +
                                                  jitterMs);
            const int primarySourceAgeMs = static_cast<int>(ageMs) - arrivalMs;

            auto energyAfterArrival = [&](int sourceAgeMs, int sourceDistance,
                                          int strength) -> int {
                if (sourceAgeMs < 0 || sourceAgeMs >= afterglowMs) return 0;
                const int distanceScale = std::max(145, 245 - sourceDistance * 90 /
                                                               std::max<int>(1, ripple.maxRadius));
                const int scatterScale = 184 + static_cast<int>((hash >> 8) & 0x2F);
                int envelope = 255;
                if (sourceAgeMs < static_cast<int>(kRippleAttackMs)) {
                    envelope = 72 + sourceAgeMs * 183 / static_cast<int>(kRippleAttackMs);
                } else {
                    const int remaining = afterglowMs - sourceAgeMs;
                    const int fadeDuration = std::max(1, afterglowMs - static_cast<int>(kRippleAttackMs));
                    envelope = remaining * remaining * 255 / (fadeDuration * fadeDuration);
                }
                return envelope * distanceScale / 255 * scatterScale / 255 * strength / 255;
            };

            int symbolSourceAgeMs = primarySourceAgeMs;
            int energy = energyAfterArrival(primarySourceAgeMs, distance, ripple.strength);
            if (ripple.hasReflection) {
                const int reflectedDx = static_cast<int>(dot.x) - ripple.reflectedX;
                const int reflectedDy = static_cast<int>(dot.y) - ripple.reflectedY;
                const int reflectedDistance = approximateDistance(reflectedDx, reflectedDy);
                const int reflectedArrival = reflectedDistance * travelMs /
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
            uint8_t symbolColorIndex = rippleSymbolColor(distance, ripple.maxRadius, hash,
                                                         ripple.mutateSymbols);
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
                    symbolSourceAgeMs < afterglowMs) {
                    const uint32_t mutationStep = static_cast<uint32_t>(
                        symbolSourceAgeMs - static_cast<int>(kRippleAttackMs)) /
                        kSymbolMutationStepMs;
                    const uint32_t mutationHash = mixHash(
                        hash ^ ((mutationStep + 1u) * 0xA511E9B3u));
                    symbolIndex = static_cast<uint8_t>(mutationHash % kSymbolCount);
                    symbolColorIndex = activeSymbolColor(mutationHash >> 9, true);
                }
            }

            const bool replace = energy > dot.rippleEnergy ||
                                 (energy == dot.rippleEnergy && energy > 0 && !ripple.fromAudio);
            if (replace) {
                dot.rippleEnergy = static_cast<uint8_t>(std::min(255, energy));
                dot.rippleColorIndex = ripple.colorIndex;
                dot.rippleUsesSymbolPalette = ripple.symbolMix;
                dot.rippleSymbolIndex = symbolIndex;
                dot.rippleSymbolColorIndex = symbolColorIndex;
                dot.rippleFromAudio = ripple.fromAudio;
            }
        }
    }
}

std::size_t Engine::acquireRippleSlot(bool fromAudio)
{
    if (fromAudio) {
        std::size_t audioCount = 0;
        std::size_t oldestAudio = kMaxRipples;
        std::size_t firstInactive = kMaxRipples;
        uint32_t oldestStarted = 0;
        for (std::size_t i = 0; i < kMaxRipples; ++i) {
            if (!_ripples[i].active) {
                if (firstInactive == kMaxRipples) firstInactive = i;
                continue;
            }
            if (!_ripples[i].fromAudio) continue;
            ++audioCount;
            if (oldestAudio == kMaxRipples || _ripples[i].startedMs < oldestStarted) {
                oldestAudio = i;
                oldestStarted = _ripples[i].startedMs;
            }
        }
        if (audioCount >= AudioReactiveParams::maxAudioRipples && oldestAudio < kMaxRipples) {
            return oldestAudio;
        }
        if (firstInactive < kMaxRipples) {
            _nextRipple = (firstInactive + 1) % kMaxRipples;
            return firstInactive;
        }
        // Music must never evict a touch ripple. If all slots are occupied,
        // recycle an existing audio ripple or drop this onset.
        return oldestAudio;
    }

    for (std::size_t i = 0; i < kMaxRipples; ++i) {
        const std::size_t slot = (_nextRipple + i) % kMaxRipples;
        if (!_ripples[slot].active) {
            _nextRipple = (slot + 1) % kMaxRipples;
            return slot;
        }
    }

    const std::size_t slot = _nextRipple;
    _nextRipple = (_nextRipple + 1) % kMaxRipples;
    return slot;
}

bool Engine::audioOutputActive() const
{
    return _audioEnabled || _audioGain > 0 || _audioGain != _audioGainTarget;
}

void Engine::updateAudioGain(uint32_t nowMs)
{
    if (_audioGain == _audioGainTarget) return;
    if (_audioGainDurationMs == 0) {
        _audioGain = _audioGainTarget;
        return;
    }
    const uint32_t elapsedMs = nowMs - _audioGainStartedMs;
    if (elapsedMs >= _audioGainDurationMs) {
        _audioGain = _audioGainTarget;
        return;
    }
    const int delta = static_cast<int>(_audioGainTarget) - static_cast<int>(_audioGainFrom);
    _audioGain = static_cast<uint8_t>(_audioGainFrom + delta * static_cast<int>(elapsedMs) /
                                                           static_cast<int>(_audioGainDurationMs));
}

void Engine::setAudioReactionEnabled(bool enabled, uint32_t nowMs)
{
    if (_audioEnabled == enabled) return;
    _audioEnabled = enabled;
    _audioGainStartedMs = nowMs;
    _audioGainFrom = _audioGain;
    if (enabled) {
        _audioGainTarget = _audioVisualPaused ? 0 : 255;
        _audioGainDurationMs = AudioReactiveParams::fadeInMs;
        _audioNeedsClear = false;
    } else {
        _audioGainTarget = 0;
        _audioGainDurationMs = AudioReactiveParams::fadeOutMs;
        _audioNeedsClear = true;
        _audioSparks = {};
        for (Ripple& ripple : _ripples) {
            if (ripple.fromAudio) ripple.active = false;
        }
    }
}

void Engine::setAudioVisualPaused(bool paused, uint32_t nowMs)
{
    if (_audioVisualPaused == paused) return;
    _audioVisualPaused = paused;
    if (!_audioEnabled) return;
    _audioGainStartedMs = nowMs;
    _audioGainFrom = _audioGain;
    _audioGainTarget = paused ? 0 : 255;
    _audioGainDurationMs = paused ? AudioReactiveParams::pauseFadeMs
                                  : AudioReactiveParams::fadeInMs;
    if (paused) {
        _audioSparks = {};
        for (Ripple& ripple : _ripples) {
            if (ripple.fromAudio) ripple.active = false;
        }
    }
}

void Engine::clearAudioReaction(bool immediate)
{
    if (!immediate) {
        setAudioReactionEnabled(false, _lastUpdateMs);
        return;
    }
    for (std::size_t i = 0; i < _dotCount; ++i) {
        _dots[i].audioEnergy = 0;
        _dots[i].rippleFromAudio = _dots[i].rippleFromAudio && _dots[i].rippleEnergy > 0;
        if (_dots[i].rippleFromAudio) {
            _dots[i].rippleEnergy = 0;
            _dots[i].rippleFromAudio = false;
        }
    }
    for (Ripple& ripple : _ripples) {
        if (ripple.fromAudio) ripple.active = false;
    }
    _audioSparks = {};
    _audioGain = 0;
    _audioGainFrom = 0;
    _audioGainTarget = 0;
    _audioActiveDotRatio = 0.0f;
    _audioMutationUntilMs = 0;
    _audioEnabled = false;
    _audioVisualPaused = false;
    _audioNeedsClear = false;
}

void Engine::triggerAudioRipple(int x, int y, uint32_t nowMs, uint8_t strength, bool symbolMix)
{
    const int dx = x - _lastAudioRippleX;
    const int dy = y - _lastAudioRippleY;
    if (_lastAudioRippleX != 0 && dx * dx + dy * dy < 20 * 20) {
        const uint32_t hash = mixHash(nowMs ^ static_cast<uint32_t>(x * 131 + y));
        const float angle = static_cast<float>((hash % 360u)) * kPi / 180.0f;
        const int offset = 18 + static_cast<int>(hash % 16u);
        x = _centerX + static_cast<int>(std::cos(angle) * offset);
        y = _centerY + static_cast<int>(std::sin(angle) * offset);
    }

    uint8_t colorIndex = _audioBaseColorIndex;
    if (symbolMix) {
        colorIndex = strength > 180 ? 1 : (strength > 120 ? 2 : 0);
        if (colorIndex == _lastAudioRippleColor) {
            colorIndex = static_cast<uint8_t>((colorIndex + 1) % 3);
        }
        _lastAudioRippleColor = colorIndex;
    }

    const uint16_t durationMs = static_cast<uint16_t>(
        AudioReactiveParams::audioRippleMinDurationMs +
        (AudioReactiveParams::audioRippleMaxDurationMs -
         AudioReactiveParams::audioRippleMinDurationMs) *
            strength / 255);
    const uint16_t travelMs = static_cast<uint16_t>(durationMs * kRippleTravelMs / kRippleDurationMs);
    const uint16_t afterglowMs = static_cast<uint16_t>(std::max(1, durationMs - travelMs));
    const uint16_t maxRadius = static_cast<uint16_t>(_maxRippleRadius * 3 / 4);
    startRipple(x, y, nowMs, colorIndex, durationMs, maxRadius, symbolMix, false, true,
                std::max<uint8_t>(96, strength), travelMs, afterglowMs);
    _lastAudioRippleX = static_cast<int16_t>(x);
    _lastAudioRippleY = static_cast<int16_t>(y);
}

uint8_t Engine::spawnAudioSparks(const AudioReactiveFrame& frame, uint32_t nowMs, bool symbolMix)
{
    if (frame.trebleTransient <= 0.04f) return 0;
    if (_lastSparkMs != 0 && nowMs - _lastSparkMs < AudioReactiveParams::sparkMinIntervalMs) return 0;
    if (_audioGain < 80 || _audioVisualPaused) return 0;

    const uint32_t seed = mixHash(nowMs ^ (frame.sequence * 0xA511E9B3u));
    const uint8_t clusterCount = frame.trebleTransient > 0.58f ? 2 : 1;
    const uint8_t pointsPerCluster = static_cast<uint8_t>(2 + frame.trebleTransient * 3.5f);
    uint8_t spawned = 0;
    for (uint8_t cluster = 0; cluster < clusterCount; ++cluster) {
        std::size_t center = _dotCount;
        for (uint8_t attempt = 0; attempt < 12 && center == _dotCount; ++attempt) {
            const uint32_t hash = mixHash(seed ^ (static_cast<uint32_t>(cluster + 1) << 12) ^ attempt);
            const std::size_t candidate = hash % std::max<std::size_t>(1, _dotCount);
            if (_dots[candidate].audioBandIndex >= 14) center = candidate;
        }
        if (center == _dotCount) center = mixHash(seed ^ (cluster + 17u)) % std::max<std::size_t>(1, _dotCount);

        for (uint8_t point = 0; point < pointsPerCluster; ++point) {
            std::size_t sparkSlot = kMaxAudioSparks;
            for (std::size_t i = 0; i < kMaxAudioSparks; ++i) {
                if (!_audioSparks[i].active) { sparkSlot = i; break; }
            }
            if (sparkSlot == kMaxAudioSparks) break;

            std::size_t chosen = _dotCount;
            int bestScore = 100000;
            for (std::size_t i = 0; i < _dotCount; ++i) {
                bool occupied = false;
                for (const AudioSpark& active : _audioSparks) {
                    if (active.active && active.index == i) { occupied = true; break; }
                }
                if (occupied || _dots[i].audioBandIndex < 13) continue;
                const int distance = approximateDistance(_dots[i].x - _dots[center].x,
                                                         _dots[i].y - _dots[center].y);
                if (distance > 58) continue;
                const uint32_t hash = mixHash(seed ^ static_cast<uint32_t>(i * 31 + point * 131));
                const int score = distance + static_cast<int>(hash % 18u);
                if (score < bestScore) { bestScore = score; chosen = i; }
            }
            if (chosen == _dotCount) break;

            const uint32_t hash = mixHash(seed ^ static_cast<uint32_t>(chosen));
            AudioSpark& spark = _audioSparks[sparkSlot];
            spark.index = static_cast<uint16_t>(chosen);
            spark.startedMs = nowMs + point * 9u;
            spark.durationMs = static_cast<uint16_t>(
                AudioReactiveParams::sparkMinDurationMs +
                hash % (AudioReactiveParams::sparkMaxDurationMs - AudioReactiveParams::sparkMinDurationMs + 1));
            spark.peakEnergy = static_cast<uint8_t>(185 + std::min(70, static_cast<int>(frame.trebleTransient * 70.0f)));
            spark.colorIndex = symbolMix ? (point == 0 ? 3 : 0)
                                         : audioHueColor(_audioBaseColorIndex, 3);
            spark.symbolIndex = static_cast<uint8_t>(hash % kSymbolCount);
            spark.active = true;
            ++spawned;
        }
    }
    if (spawned > 0) _lastSparkMs = nowMs;
    return spawned;
}

uint8_t Engine::spawnAudioBloom(const AudioReactiveFrame& frame, uint32_t nowMs, bool symbolMix)
{
    if (frame.midTransient <= 0.04f || _audioGain < 80 || _audioVisualPaused) return 0;
    if (_lastBloomMs != 0 && nowMs - _lastBloomMs < AudioReactiveParams::midTransientCooldownMs) return 0;

    const uint32_t seed = mixHash(nowMs ^ (frame.sequence * 0x6C8E9CF5u));
    std::size_t center = _dotCount;
    for (uint8_t attempt = 0; attempt < 12 && center == _dotCount; ++attempt) {
        const std::size_t candidate = mixHash(seed ^ attempt) % std::max<std::size_t>(1, _dotCount);
        const uint8_t group = _dots[candidate].audioBandIndex / 5;
        if (group == 1 || group == 2) center = candidate;
    }
    if (center == _dotCount) center = seed % std::max<std::size_t>(1, _dotCount);

    const uint8_t wanted = static_cast<uint8_t>(4 + frame.midTransient * 5.0f);
    uint8_t spawned = 0;
    for (uint8_t point = 0; point < wanted; ++point) {
        std::size_t sparkSlot = kMaxAudioSparks;
        for (std::size_t i = 0; i < kMaxAudioSparks; ++i) {
            if (!_audioSparks[i].active) { sparkSlot = i; break; }
        }
        if (sparkSlot == kMaxAudioSparks) break;

        std::size_t chosen = _dotCount;
        int bestScore = 100000;
        for (std::size_t i = 0; i < _dotCount; ++i) {
            bool occupied = false;
            for (const AudioSpark& active : _audioSparks) {
                if (active.active && active.index == i) { occupied = true; break; }
            }
            if (occupied) continue;
            const int distance = approximateDistance(_dots[i].x - _dots[center].x,
                                                     _dots[i].y - _dots[center].y);
            if (distance > 52) continue;
            const int score = distance + static_cast<int>(mixHash(seed ^ static_cast<uint32_t>(i * 17 + point)) % 15u);
            if (score < bestScore) { bestScore = score; chosen = i; }
        }
        if (chosen == _dotCount) break;

        const uint32_t hash = mixHash(seed ^ static_cast<uint32_t>(chosen));
        AudioSpark& spark = _audioSparks[sparkSlot];
        spark.index = static_cast<uint16_t>(chosen);
        spark.startedMs = nowMs + point * 12u;
        spark.durationMs = static_cast<uint16_t>(170 + hash % 150u);
        spark.peakEnergy = static_cast<uint8_t>(160 + frame.midTransient * 80.0f);
        spark.colorIndex = symbolMix ? ((point & 1u) ? 1 : 2)
                                     : audioHueColor(_audioBaseColorIndex, 2);
        spark.symbolIndex = static_cast<uint8_t>((hash + point) % kSymbolCount);
        spark.active = true;
        ++spawned;
    }
    if (spawned > 0) {
        _lastBloomMs = nowMs;
        _audioMutationUntilMs = nowMs + static_cast<uint32_t>(180 + frame.midTransient * 220.0f);
    }
    return spawned;
}

void Engine::overlayAudioSparks(uint32_t nowMs)
{
    for (AudioSpark& spark : _audioSparks) {
        if (!spark.active) continue;
        if (nowMs < spark.startedMs) continue;
        const uint32_t ageMs = nowMs - spark.startedMs;
        if (ageMs >= spark.durationMs || spark.index >= _dotCount) {
            spark.active = false;
            continue;
        }
        const int remaining = static_cast<int>(spark.durationMs - ageMs);
        const int energy = spark.peakEnergy * remaining / std::max(1, static_cast<int>(spark.durationMs));
        Dot& dot = _dots[spark.index];
        if (energy > dot.audioEnergy) {
            dot.audioEnergy = static_cast<uint8_t>(std::min(255, energy));
            dot.audioColorIndex = spark.colorIndex;
            dot.audioSymbolIndex = spark.symbolIndex;
        }
    }
}

AudioApplyResult Engine::applyAudioFrame(const AudioReactiveFrame& frame, uint32_t nowMs,
                                         bool symbolMix, uint8_t baseColorIndex)
{
    AudioApplyResult result;
    updateAudioGain(nowMs);
    _audioBaseColorIndex = baseColorIndex;

    if (!_audioEnabled && _audioGain == 0) {
        if (_audioNeedsClear) clearAudioReaction(true);
        return result;
    }

    const float gain = static_cast<float>(_audioGain) / 255.0f;
    const bool newFrame = frame.sequence != _lastAppliedAudioSequence;
    _lastAppliedAudioSequence = frame.sequence;

    const bool shouldMapField = newFrame &&
                                (_lastAudioFieldMapMs == 0 || nowMs - _lastAudioFieldMapMs >= 16);
    if (shouldMapField) {
        _lastAudioFieldMapMs = nowMs;
        const float density = frame.signalActive
                                  ? std::clamp(frame.signalConfidence *
                                                   (0.08f + frame.overallEnergy * 0.18f +
                                                    frame.grooveEnergy * 0.16f +
                                                    frame.slowEnergy * 0.12f +
                                                    frame.groovePulse * 0.08f),
                                               0.0f, 0.60f)
                                  : 0.0f;
        uint32_t yellowActive = 0;
        uint32_t audioActive = 0;
        const uint8_t densityLimit = static_cast<uint8_t>(density * 255.0f);
        const uint8_t centroidOffset = static_cast<uint8_t>(
            std::clamp(frame.spectralCentroid / 19.0f, 0.0f, 1.0f) * 96.0f);

        for (std::size_t i = 0; i < _dotCount; ++i) {
            Dot& dot = _dots[i];
            const uint8_t group = static_cast<uint8_t>(dot.audioBandIndex / 5);
            const uint32_t seed = mixHash(static_cast<uint32_t>(i + 1) * 0x9E3779B9u ^
                                          static_cast<uint32_t>(dot.x * 257 + dot.y));
            const uint8_t stableRank = static_cast<uint8_t>(seed & 0xFFu);
            const uint8_t wave = static_cast<uint8_t>(dot.x * 3 + dot.y * 2 + nowMs / 12u +
                                                      centroidOffset);
            const uint8_t triangle = static_cast<uint8_t>(std::min(
                255, std::abs(static_cast<int>(wave) - 128) * 2));
            const uint8_t flowRank = static_cast<uint8_t>((stableRank * 3u + triangle) / 4u);

            const float band = frame.bands[dot.audioBandIndex];
            const float groupEnergy = frame.groups[group];
            const float activation = std::clamp(band * 0.46f + groupEnergy * 0.27f +
                                                    frame.grooveEnergy * 0.17f +
                                                    frame.groovePulse * 0.10f,
                                                0.0f, 1.0f);
            int energy = 0;
            if (flowRank <= densityLimit) {
                energy = static_cast<int>((52.0f + activation * 183.0f) * gain);
            }
            dot.audioEnergy = quantizeAudioEnergy(dot.audioEnergy, energy);
            dot.audioUsesSymbolPalette = symbolMix;

            if (dot.audioEnergy > 0) {
                ++audioActive;
                if (symbolMix) {
                    uint8_t color = audioFieldSymbolColor(group, groupEnergy, seed);
                    if (color == 3 && audioActive > 0 &&
                        yellowActive * 100 >= audioActive * kAudioYellowCapPercent) {
                        color = group >= 2 ? 0 : 1;
                    }
                    if (color == 3) ++yellowActive;

                    uint8_t symbol = dot.symbolIndex;
                    if (nowMs < _audioMutationUntilMs) {
                        const uint32_t period = AudioReactiveParams::symbolMutationMinMs +
                                                (seed % (AudioReactiveParams::symbolMutationMaxMs -
                                                         AudioReactiveParams::symbolMutationMinMs + 1));
                        const uint32_t mutationStep = (nowMs + (seed % period)) / period;
                        const uint32_t mutationHash = mixHash(seed ^ (mutationStep * 0xA511E9B3u));
                        if ((mutationHash % 100u) < 42u) {
                            symbol = static_cast<uint8_t>(mutationHash % kSymbolCount);
                        }
                    }
                    dot.audioColorIndex = color;
                    dot.audioSymbolIndex = symbol;
                } else {
                    dot.audioColorIndex = audioHueColor(baseColorIndex, group);
                    dot.audioSymbolIndex = 0;
                }
            } else {
                dot.audioColorIndex = symbolMix ? dot.symbolColorIndex : baseColorIndex;
                dot.audioSymbolIndex = dot.symbolIndex;
            }
        }
        _audioActiveDotRatio = _dotCount > 0
                                   ? static_cast<float>(audioActive) / static_cast<float>(_dotCount)
                                   : 0.0f;
    }

    if (newFrame && frame.midTransient > 0.0f) {
        result.bloomsSpawned = spawnAudioBloom(frame, nowMs, symbolMix);
    }
    if (newFrame && frame.trebleTransient > 0.0f) {
        result.sparksSpawned = spawnAudioSparks(frame, nowMs, symbolMix);
    }
    overlayAudioSparks(nowMs);

    if (newFrame && frame.onset && _audioEnabled && !_audioVisualPaused && _audioGain >= 140) {
        const uint8_t strength = static_cast<uint8_t>(std::clamp(
            frame.onsetStrength * 180.0f + frame.groups[0] * 50.0f + frame.groups[1] * 25.0f,
            80.0f, 255.0f));
        const int originRadius = AudioReactiveParams::audioRippleOriginRadius *
                                 (270 - static_cast<int>(strength)) / 255;
        const uint32_t hash = mixHash(nowMs ^ (frame.sequence * 0x9E3779B9u));
        const float angle = static_cast<float>(hash % 360u) * kPi / 180.0f;
        const int radius = std::clamp(originRadius, 0, AudioReactiveParams::audioRippleOriginRadius);
        triggerAudioRipple(_centerX + static_cast<int>(std::cos(angle) * radius),
                           _centerY + static_cast<int>(std::sin(angle) * radius), nowMs, strength,
                           symbolMix);
        result.onsetTriggered = true;
    }

    return result;
}

}  // namespace glow_field
