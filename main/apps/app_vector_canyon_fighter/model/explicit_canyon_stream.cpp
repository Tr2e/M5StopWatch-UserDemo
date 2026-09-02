#include "explicit_canyon_stream.h"

#include <algorithm>
#include <cmath>

namespace vector_canyon_fighter {
namespace {

constexpr float kEventCycleLength = 44.0f;
constexpr float kRouteControlSpacing = 4.5f;
constexpr float kRouteArcIntegrationStep = 0.34f;
constexpr float kRouteDerivativeStep = 0.02f;

struct Vec2 {
    float x = 0.0f;
    float z = 0.0f;
};

Vec2 operator-(Vec2 left, Vec2 right) { return {left.x - right.x, left.z - right.z}; }
Vec2 operator*(Vec2 value, float scale) { return {value.x * scale, value.z * scale}; }

float length(Vec2 value) { return std::sqrt(value.x * value.x + value.z * value.z); }

Vec2 normalize(Vec2 value)
{
    const float magnitude = length(value);
    return magnitude > 0.00001f ? value * (1.0f / magnitude) : Vec2{0.0f, 1.0f};
}

uint32_t mixBits(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ (value >> 16);
}

float random01(uint32_t seed, uint32_t index, uint32_t salt)
{
    return static_cast<float>(mixBits(seed ^ (index * 0x9e3779b9u) ^ salt) & 0xffffu) / 65535.0f;
}

float signedControlNoise(int index, uint32_t seed)
{
    const uint32_t bits = mixBits(static_cast<uint32_t>(index) * 0x85ebca6bu ^ seed);
    return static_cast<float>(bits & 0xffffu) / 32767.5f - 1.0f;
}

float routeControlX(int index, uint32_t seed)
{
    const float i = static_cast<float>(index);
    const float ramp = std::clamp(i, 0.0f, 4.0f) * 0.25f;
    const float phase = static_cast<float>(seed & 255u) * 0.007f;
    const float broadTurn = std::sin(i * 0.58f + phase) * 1.46f;
    const float longTurn = std::sin(i * 0.23f - phase * 0.61f) * 0.72f;
    const float variation = signedControlNoise(index, seed ^ 0xa511e9b3u) * 0.18f;
    return ramp * (broadTurn + longTurn + variation);
}

float catmullRom(float p0, float p1, float p2, float p3, float t)
{
    const float t2 = t * t;
    const float t3 = t2 * t;
    return 0.5f * ((2.0f * p1) + (-p0 + p2) * t +
                   (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                   (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

Vec2 routePointAtParameter(float parameterZ, uint32_t seed)
{
    const float controlPosition = std::max(parameterZ, 0.0f) / kRouteControlSpacing;
    const int index = static_cast<int>(std::floor(controlPosition));
    const float t = controlPosition - static_cast<float>(index);
    return {
        catmullRom(routeControlX(index - 1, seed), routeControlX(index, seed),
                   routeControlX(index + 1, seed), routeControlX(index + 2, seed), t),
        std::max(parameterZ, 0.0f),
    };
}

float routeArcLengthAtParameter(float parameterZ, uint32_t seed)
{
    if (parameterZ <= 0.0f) return 0.0f;
    const int stepCount = std::max(1, static_cast<int>(std::ceil(parameterZ / kRouteArcIntegrationStep)));
    const float step = parameterZ / static_cast<float>(stepCount);
    Vec2 previous = routePointAtParameter(0.0f, seed);
    float arcLength = 0.0f;
    for (int index = 1; index <= stepCount; ++index) {
        const Vec2 next = routePointAtParameter(step * static_cast<float>(index), seed);
        arcLength += length(next - previous);
        previous = next;
    }
    return arcLength;
}

float routeParameterAtArcLength(float worldS, uint32_t seed)
{
    if (worldS <= 0.0f) return 0.0f;
    float low = 0.0f;
    float high = worldS;
    for (int iteration = 0; iteration < 14; ++iteration) {
        const float middle = (low + high) * 0.5f;
        if (routeArcLengthAtParameter(middle, seed) < worldS) {
            low = middle;
        } else {
            high = middle;
        }
    }
    return (low + high) * 0.5f;
}

float compactShoulderBump(float normalizedDistance)
{
    const float x = std::clamp(1.0f - std::abs(normalizedDistance), 0.0f, 1.0f);
    return x * x * x * (10.0f + x * (-15.0f + 6.0f * x));
}

CanyonShoulderEvent makeStreamEvent(uint32_t seed, uint32_t eventIndex)
{
    const uint32_t cycle = eventIndex / 2u;
    const bool left = (eventIndex & 1u) == 0u;
    const float centerBase = static_cast<float>(cycle) * kEventCycleLength + (left ? 10.0f : 28.0f);
    const float centerJitter = (random01(seed, eventIndex, 0x68bc21ebu) - 0.5f) * 1.20f;
    const float halfLength = 5.55f + random01(seed, eventIndex, 0x02e5be93u) * 0.75f;
    const float amplitude = 0.88f + random01(seed, eventIndex, 0xb5297a4du) * 0.27f;
    return {left ? CanyonSide::Left : CanyonSide::Right, centerBase + centerJitter, halfLength, amplitude};
}

CanyonBoundary makeBoundary(float worldS, uint32_t seed)
{
    CanyonBoundary boundary{};
    const int baseCycle = std::max(0, static_cast<int>(std::floor(worldS / kEventCycleLength)) - 1);
    for (int cycleOffset = 0; cycleOffset < 3; ++cycleOffset) {
        const uint32_t cycle = static_cast<uint32_t>(baseCycle + cycleOffset);
        for (uint32_t eventInCycle = 0; eventInCycle < 2; ++eventInCycle) {
            const CanyonShoulderEvent event = makeStreamEvent(seed, cycle * 2u + eventInCycle);
            const float normalizedDistance = (worldS - event.centerWorldS) / event.halfLength;
            const float intrusion = event.amplitude * compactShoulderBump(normalizedDistance);
            if (event.side == CanyonSide::Left) {
                boundary.leftWidth -= intrusion;
            } else {
                boundary.rightWidth -= intrusion;
            }
        }
    }
    return boundary;
}

float deformedLateral(std::size_t profilePoint, const CanyonBoundary& boundary)
{
    const CanyonProfileSample& sample = kExplicitCanyonProfile[profilePoint];
    const float leftIntrusion = kExplicitCanyonFloorHalfWidth - boundary.leftWidth;
    const float rightIntrusion = kExplicitCanyonFloorHalfWidth - boundary.rightWidth;
    if (profilePoint < static_cast<std::size_t>(CanyonProfilePoint::LeftFloorEdge)) {
        return sample.lateral + leftIntrusion;
    }
    if (profilePoint > static_cast<std::size_t>(CanyonProfilePoint::RightFloorEdge)) {
        return sample.lateral - rightIntrusion;
    }
    if (profilePoint <= static_cast<std::size_t>(CanyonProfilePoint::FloorCenter)) {
        return sample.lateral * boundary.leftWidth / kExplicitCanyonFloorHalfWidth;
    }
    return sample.lateral * boundary.rightWidth / kExplicitCanyonFloorHalfWidth;
}

}  // namespace

void ExplicitCanyonStream::reset(uint32_t seed)
{
    _seed = seed;
    _playerWorldS = 0.0f;
    rebuildSlices(0);
}

void ExplicitCanyonStream::update(float flightForwardDistance)
{
    _playerWorldS = std::max(0.0f, flightForwardDistance * kForwardDistanceScale);
    const uint32_t wantedFirstSegment = static_cast<uint32_t>(std::floor(_playerWorldS / kSliceSpacing));
    if (wantedFirstSegment < _firstSegment) {
        rebuildSlices(wantedFirstSegment);
        return;
    }

    bool recycled = false;
    while (_firstSegment < wantedFirstSegment) {
        for (std::size_t slice = 1; slice < _slices.size(); ++slice) _slices[slice - 1] = _slices[slice];
        ++_firstSegment;
        _slices.back() = makeSlice(_firstSegment + static_cast<uint32_t>(_slices.size() - 1));
        recycled = true;
    }
    if (recycled) refreshEventWindow();
}

CanyonWorldPoint ExplicitCanyonStream::worldPoint(std::size_t slice, std::size_t profilePoint) const
{
    if (slice >= _slices.size() || profilePoint >= kExplicitCanyonProfile.size()) return {};
    const ExplicitCanyonSlice& source = _slices[slice];
    const CanyonBoundary boundary{source.leftWidth, source.rightWidth};
    const float lateral = deformedLateral(profilePoint, boundary);
    const float normalX = source.tangentZ;
    const float normalZ = -source.tangentX;
    return {
        source.centerX + normalX * lateral,
        kExplicitCanyonProfile[profilePoint].height,
        source.centerZ + normalZ * lateral,
    };
}

CanyonBoundary ExplicitCanyonStream::boundaryAt(float worldS) const
{
    return makeBoundary(std::max(worldS, 0.0f), _seed);
}

CanyonRouteFrame ExplicitCanyonStream::routeFrameAt(float worldS) const
{
    const float clampedWorldS = std::max(worldS, 0.0f);
    if (clampedWorldS >= _slices.front().worldS && clampedWorldS <= _slices.back().worldS) {
        const float localPosition = std::max(0.0f, (clampedWorldS - _slices.front().worldS) / kSliceSpacing);
        const std::size_t local = static_cast<std::size_t>(std::floor(localPosition));
        if (local + 1 < _slices.size()) {
            const ExplicitCanyonSlice& from = _slices[local];
            const ExplicitCanyonSlice& to = _slices[local + 1];
            const float blend = std::clamp((clampedWorldS - from.worldS) / kSliceSpacing, 0.0f, 1.0f);
            const Vec2 tangent = normalize({from.tangentX * (1.0f - blend) + to.tangentX * blend,
                                            from.tangentZ * (1.0f - blend) + to.tangentZ * blend});
            return {
                clampedWorldS,
                from.centerX * (1.0f - blend) + to.centerX * blend,
                from.centerZ * (1.0f - blend) + to.centerZ * blend,
                tangent.x,
                tangent.z,
            };
        }
        const ExplicitCanyonSlice& back = _slices.back();
        return {back.worldS, back.centerX, back.centerZ, back.tangentX, back.tangentZ};
    }
    return calculateRouteFrame(clampedWorldS);
}

CanyonShoulderEvent ExplicitCanyonStream::eventAtIndex(uint32_t eventIndex) const
{
    return makeStreamEvent(_seed, eventIndex);
}

ExplicitCanyonSlice ExplicitCanyonStream::makeSlice(uint32_t segment) const
{
    const float worldS = static_cast<float>(segment) * kSliceSpacing;
    const CanyonRouteFrame route = calculateRouteFrame(worldS);
    const CanyonBoundary boundary = makeBoundary(worldS, _seed);
    return {segment, worldS, route.centerX, route.centerZ, route.tangentX, route.tangentZ,
            boundary.leftWidth, boundary.rightWidth};
}

CanyonRouteFrame ExplicitCanyonStream::calculateRouteFrame(float worldS) const
{
    const float parameter = routeParameterAtArcLength(worldS, _seed);
    const Vec2 center = routePointAtParameter(parameter, _seed);
    const Vec2 previous = routePointAtParameter(std::max(0.0f, parameter - kRouteDerivativeStep), _seed);
    const Vec2 next = routePointAtParameter(parameter + kRouteDerivativeStep, _seed);
    const Vec2 tangent = normalize(next - previous);
    return {worldS, center.x, center.z, tangent.x, tangent.z};
}

void ExplicitCanyonStream::rebuildSlices(uint32_t firstSegment)
{
    _firstSegment = firstSegment;
    for (std::size_t slice = 0; slice < _slices.size(); ++slice) {
        _slices[slice] = makeSlice(_firstSegment + static_cast<uint32_t>(slice));
    }
    refreshEventWindow();
}

void ExplicitCanyonStream::refreshEventWindow()
{
    _eventWindow = {};
    const float startWorldS = _slices.front().worldS;
    const float endWorldS = _slices.back().worldS;
    const int firstCycle = std::max(0, static_cast<int>(std::floor(startWorldS / kEventCycleLength)) - 1);
    const int lastCycle = static_cast<int>(std::floor(endWorldS / kEventCycleLength)) + 1;
    for (int cycle = firstCycle; cycle <= lastCycle; ++cycle) {
        for (uint32_t eventInCycle = 0; eventInCycle < 2; ++eventInCycle) {
            const CanyonShoulderEvent event = makeStreamEvent(_seed, static_cast<uint32_t>(cycle) * 2u + eventInCycle);
            if (event.centerWorldS + event.halfLength < startWorldS ||
                event.centerWorldS - event.halfLength > endWorldS) {
                continue;
            }
            if (_eventWindow.count < _eventWindow.events.size()) {
                _eventWindow.events[_eventWindow.count++] = event;
            } else {
                _eventWindow.overflow = true;
            }
        }
    }
}

}  // namespace vector_canyon_fighter
