#pragma once

#include <algorithm>
#include <cmath>

namespace vector_canyon_fighter {

struct HudLayoutPoint {
    float x = 0.0f;
    float y = 0.0f;
};

struct HudBoundedCue {
    HudLayoutPoint point;
    float directionX = 0.0f;
    float directionY = -1.0f;
    bool constrained = false;
};

inline int normalizeHudDegrees(int degrees)
{
    degrees %= 360;
    return degrees < 0 ? degrees + 360 : degrees;
}

inline float hudHeadingTickX(float tickDegrees, float headingDegrees,
                             float centerX, float pixelsPerFiveDegrees = 22.0f)
{
    return centerX + (tickDegrees - headingDegrees) *
                         (pixelsPerFiveDegrees / 5.0f);
}

inline float hudTapeTickY(float tickValue, float currentValue,
                          float unitsPerTick, float pixelsPerTick,
                          float datumY)
{
    return datumY - (tickValue - currentValue) *
                        (pixelsPerTick / unitsPerTick);
}

inline bool hudRectInsideCircularArea(float x, float y, float rectWidth,
                                      float rectHeight, int width, int height,
                                      float edgeInset)
{
    const float centerX = static_cast<float>(width) * 0.5f;
    const float centerY = static_cast<float>(height) * 0.5f;
    const float radius = static_cast<float>(std::min(width, height)) * 0.5f -
                         edgeInset;
    const float corners[4][2] = {
        {x, y}, {x + rectWidth, y},
        {x, y + rectHeight}, {x + rectWidth, y + rectHeight},
    };
    for (const auto& corner : corners) {
        const float deltaX = corner[0] - centerX;
        const float deltaY = corner[1] - centerY;
        if (deltaX * deltaX + deltaY * deltaY > radius * radius) return false;
    }
    return true;
}

inline HudBoundedCue constrainHudCueToRect(
    HudLayoutPoint raw, HudLayoutPoint origin,
    float minX, float maxX, float minY, float maxY)
{
    const float deltaX = raw.x - origin.x;
    const float deltaY = raw.y - origin.y;
    const float length = std::sqrt(deltaX * deltaX + deltaY * deltaY);
    const float directionX = length > 0.0001f ? deltaX / length : 0.0f;
    const float directionY = length > 0.0001f ? deltaY / length : -1.0f;
    float scale = 1.0f;
    if (deltaX < 0.0f && raw.x < minX) {
        scale = std::min(scale, (minX - origin.x) / deltaX);
    } else if (deltaX > 0.0f && raw.x > maxX) {
        scale = std::min(scale, (maxX - origin.x) / deltaX);
    }
    if (deltaY < 0.0f && raw.y < minY) {
        scale = std::min(scale, (minY - origin.y) / deltaY);
    } else if (deltaY > 0.0f && raw.y > maxY) {
        scale = std::min(scale, (maxY - origin.y) / deltaY);
    }
    scale = std::clamp(scale, 0.0f, 1.0f);
    return {
        {
            origin.x + deltaX * scale,
            origin.y + deltaY * scale,
        },
        directionX,
        directionY,
        scale < 0.9999f,
    };
}

inline bool hudPitchLabelVisible(float x, float y, bool aircraftVisible,
                                 int width, int height)
{
    constexpr float kHorizontalInset = 78.0f;
    constexpr float kTopInset = 84.0f;
    constexpr float kBottomInset = 84.0f;
    constexpr float kThirdPersonBottom = 218.0f;
    constexpr float kCircularInset = 31.0f;
    const float maximumY = aircraftVisible
        ? kThirdPersonBottom
        : static_cast<float>(height) - kBottomInset;
    if (x < kHorizontalInset || x > static_cast<float>(width) - kHorizontalInset ||
        y < kTopInset || y > maximumY) {
        return false;
    }
    const float centerX = static_cast<float>(width) * 0.5f;
    const float centerY = static_cast<float>(height) * 0.5f;
    const float safeRadius = static_cast<float>(std::min(width, height)) * 0.5f -
                             kCircularInset;
    const float deltaX = x - centerX;
    const float deltaY = y - centerY;
    return deltaX * deltaX + deltaY * deltaY <= safeRadius * safeRadius;
}

inline bool hudRouteCueVisible(const HudBoundedCue& cue,
                               HudLayoutPoint origin,
                               bool aircraftVisible)
{
    if (aircraftVisible || cue.constrained) return true;
    constexpr float kFirstPersonDatumClearance = 14.0f;
    const float deltaX = cue.point.x - origin.x;
    const float deltaY = cue.point.y - origin.y;
    return deltaX * deltaX + deltaY * deltaY >=
           kFirstPersonDatumClearance * kFirstPersonDatumClearance;
}

}  // namespace vector_canyon_fighter
