#pragma once

#include "model/explicit_canyon_types.h"

#include <algorithm>
#include <cmath>

namespace vector_canyon_fighter {

inline constexpr float kExplicitCanyonNearPlane = 0.20f;
inline constexpr float kExplicitCanyonPrincipalYRatio = 0.347f;
inline constexpr float kExplicitCanyonFocalWidthRatio = 0.827f;
inline constexpr float kExplicitCanyonCameraLift = 0.45f;
inline constexpr float kExplicitCanyonNeutralPitchDegrees = -3.1f;

struct CanyonCameraVector {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct CanyonCameraPoint {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct CanyonScreenPoint {
    float x = 0.0f;
    float y = 0.0f;
};

struct CanyonCamera {
    CanyonCameraVector position;
    CanyonCameraVector right;
    CanyonCameraVector up;
    CanyonCameraVector forward;
    float principalX = 0.0f;
    float principalY = 0.0f;
    float focalLength = 0.0f;
};

inline float canyonDot(CanyonCameraVector left, CanyonCameraVector right)
{
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

inline CanyonCameraVector canyonCross(CanyonCameraVector left, CanyonCameraVector right)
{
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x,
    };
}

inline CanyonCameraVector canyonNormalize(CanyonCameraVector value)
{
    const float magnitude = std::sqrt(canyonDot(value, value));
    if (magnitude <= 0.00001f) return {0.0f, 0.0f, 1.0f};
    return {value.x / magnitude, value.y / magnitude, value.z / magnitude};
}

inline CanyonCamera makeExplicitCanyonCamera(const CanyonRouteFrame& route, float flightAltitude,
                                             float flightPitchDegrees, int width, int height)
{
    constexpr float kDegreesToRadians = 0.01745329252f;
    const float pitch = (kExplicitCanyonNeutralPitchDegrees + flightPitchDegrees) * kDegreesToRadians;
    const float horizontalScale = std::cos(pitch);
    const CanyonCameraVector worldUp{0.0f, 1.0f, 0.0f};
    const CanyonCameraVector forward = canyonNormalize(
        {route.tangentX * horizontalScale, std::sin(pitch), route.tangentZ * horizontalScale});
    const CanyonCameraVector right = canyonNormalize(canyonCross(worldUp, forward));
    const CanyonCameraVector up = canyonNormalize(canyonCross(forward, right));
    return {
        {route.centerX, flightAltitude + kExplicitCanyonCameraLift, route.centerZ},
        right,
        up,
        forward,
        static_cast<float>(width) * 0.5f,
        static_cast<float>(height) * kExplicitCanyonPrincipalYRatio,
        static_cast<float>(width) * kExplicitCanyonFocalWidthRatio,
    };
}

inline CanyonCamera makeExplicitCanyonTopDebugCamera(const CanyonRouteFrame& route, int width, int height)
{
    const CanyonCameraVector worldUp{0.0f, 1.0f, 0.0f};
    const CanyonCameraVector forward =
        canyonNormalize({route.tangentX * 0.68f, -0.58f, route.tangentZ * 0.68f});
    const CanyonCameraVector right = canyonNormalize(canyonCross(worldUp, forward));
    const CanyonCameraVector up = canyonNormalize(canyonCross(forward, right));
    return {
        {route.centerX - route.tangentX * 5.0f, 14.0f, route.centerZ - route.tangentZ * 5.0f},
        right,
        up,
        forward,
        static_cast<float>(width) * 0.5f,
        static_cast<float>(height) * 0.52f,
        static_cast<float>(width) * 0.60f,
    };
}

inline CanyonCameraPoint explicitCanyonToCamera(const CanyonCamera& camera, CanyonWorldPoint world)
{
    const CanyonCameraVector relative{
        world.x - camera.position.x,
        world.y - camera.position.y,
        world.z - camera.position.z,
    };
    return {
        canyonDot(relative, camera.right),
        canyonDot(relative, camera.up),
        canyonDot(relative, camera.forward),
    };
}

inline bool clipExplicitCanyonSegmentToNear(CanyonCameraPoint& from, CanyonCameraPoint& to)
{
    const bool fromVisible = from.z >= kExplicitCanyonNearPlane;
    const bool toVisible = to.z >= kExplicitCanyonNearPlane;
    if (!fromVisible && !toVisible) return false;
    if (fromVisible && toVisible) return true;

    CanyonCameraPoint& behind = fromVisible ? to : from;
    const CanyonCameraPoint& ahead = fromVisible ? from : to;
    const float denominator = ahead.z - behind.z;
    if (std::abs(denominator) <= 0.00001f) return false;
    const float blend = std::clamp((kExplicitCanyonNearPlane - behind.z) / denominator, 0.0f, 1.0f);
    behind.x += (ahead.x - behind.x) * blend;
    behind.y += (ahead.y - behind.y) * blend;
    behind.z = kExplicitCanyonNearPlane;
    return true;
}

inline bool projectExplicitCanyonPoint(const CanyonCamera& camera, CanyonCameraPoint point,
                                       CanyonScreenPoint& screen)
{
    if (point.z < kExplicitCanyonNearPlane) return false;
    screen.x = camera.principalX + camera.focalLength * point.x / point.z;
    screen.y = camera.principalY - camera.focalLength * point.y / point.z;
    return std::isfinite(screen.x) && std::isfinite(screen.y);
}

inline bool isExplicitCanyonStructuralRail(std::size_t profileIndex)
{
    return profileIndex == 0 || profileIndex == 3 || profileIndex == 7 || profileIndex == 12 ||
           profileIndex == 17 || profileIndex == 21 || profileIndex == 24;
}

inline bool isExplicitCanyonMidRail(std::size_t profileIndex)
{
    return profileIndex == 5 || profileIndex == 9 || profileIndex == 15 || profileIndex == 19;
}

inline float explicitCanyonSmoothUnit(float value)
{
    const float t = std::clamp(value, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

inline float explicitCanyonRailLodWeight(std::size_t profileIndex, float relativeDepth)
{
    if (isExplicitCanyonStructuralRail(profileIndex)) return 1.0f;
    if (isExplicitCanyonMidRail(profileIndex)) {
        return 1.0f - explicitCanyonSmoothUnit((relativeDepth - 20.0f) / 5.0f);
    }
    return 1.0f - explicitCanyonSmoothUnit((relativeDepth - 9.0f) / 5.0f);
}

static_assert(sizeof(CanyonCameraPoint) == 12, "Camera-space points must remain three floats");
static_assert(sizeof(CanyonScreenPoint) == 8, "Screen points must remain two floats");
static_assert(sizeof(CanyonCamera) == 60, "Camera matrix exceeded its reviewed stack budget");

}  // namespace vector_canyon_fighter
