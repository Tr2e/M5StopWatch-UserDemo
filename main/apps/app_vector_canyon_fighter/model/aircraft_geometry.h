#pragma once

#include <array>
#include <algorithm>
#include <cstddef>
#include <cmath>

namespace vector_canyon_fighter {

struct AircraftCollisionStation {
    float forwardLead = 0.0f;
    float halfWidth = 0.0f;
};

// The aircraft is a screen-space third-person model, but these stations define
// the world-space footprint it represents. The wing station is deliberately
// placed where a 0.24-unit half-span projects to the rendered ~47 px wing span.
inline constexpr std::array<AircraftCollisionStation, 3> kAircraftCollisionStations = {{
    {1.10f, 0.10f},  // tail / engine cluster
    {1.85f, 0.24f},  // main wing
    {2.45f, 0.07f},  // nose
}};

inline constexpr std::size_t kAircraftWingStationIndex = 1;
inline constexpr float kAircraftMaximumHalfWidth = 0.24f;
inline constexpr float kAircraftFloorClearance = 0.15f;
inline constexpr float kAircraftNominalScreenHalfSpan = 46.5f;
inline constexpr int kAircraftScreenCenterY = 320;
inline constexpr int kAircraftCourseCueY = 278;
inline constexpr float kAircraftNominalScreenBottomY =
    static_cast<float>(kAircraftScreenCenterY) + 26.5f;
inline constexpr float kChaseCameraPitchFollow = 0.12f;

struct AircraftScreenOffset {
    float x = 0.0f;
    float y = 0.0f;
};

// Perspective projection for the screen-space aircraft model. Pitch rotates
// the actual 3D geometry before projection; roll then rotates the projected
// silhouette around the fixed third-person chase datum.
inline AircraftScreenOffset projectAircraftPose(float x, float y, float z,
                                                 float pitchDegrees, float rollDegrees)
{
    constexpr float kDegreesToRadians = 0.01745329252f;
    constexpr float kFocalLength = 62.0f;
    constexpr float kDepthOffset = 5.7f;
    constexpr float kViewSlope = 0.52f;
    const float pitch = pitchDegrees * kDegreesToRadians;
    const float pitchedY = y * std::cos(pitch) + z * std::sin(pitch);
    const float pitchedZ = z * std::cos(pitch) - y * std::sin(pitch);
    const float depth = pitchedZ + kDepthOffset;
    const float localX = kFocalLength * x / depth;
    const float localY = -kFocalLength * (pitchedY + pitchedZ * kViewSlope) / depth;
    const float roll = rollDegrees * kDegreesToRadians;
    return {
        localX * std::cos(roll) - localY * std::sin(roll),
        localX * std::sin(roll) + localY * std::cos(roll),
    };
}

struct AircraftGroundShadow {
    int centerY = 0;
    int radiusX = 0;
    int radiusY = 0;
    float brightness = 0.0f;
};

// A compact visual altitude cue, deliberately independent of the much longer
// collision envelope. The footprint stays immediately below the aircraft and
// never expands into a screen-spanning ground polygon.
inline AircraftGroundShadow makeAircraftGroundShadow(float floorClearance)
{
    const float normalized = std::clamp(floorClearance / 1.20f, 0.0f, 1.0f);
    return {
        static_cast<int>(std::lround(kAircraftNominalScreenBottomY + 8.0f + normalized * 24.0f)),
        static_cast<int>(std::lround(16.0f - normalized * 6.0f)),
        static_cast<int>(std::lround(4.0f - normalized * 2.0f)),
        0.16f + (1.0f - normalized) * 0.12f,
    };
}

static_assert(kAircraftCollisionStations[kAircraftWingStationIndex].halfWidth ==
                  kAircraftMaximumHalfWidth,
              "The wing station must own the maximum collision span");

}  // namespace vector_canyon_fighter
