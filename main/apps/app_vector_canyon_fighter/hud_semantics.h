#pragma once

#include "input/flight_input.h"
#include "model/collision_model.h"
#include "model/explicit_canyon_types.h"

#include <algorithm>
#include <cmath>

namespace vector_canyon_fighter {

enum class HudInputAlert : uint8_t {
    None,
    AxisLost,
    ActionLost,
};

enum class HudAvoidanceDirection : uint8_t {
    None,
    Left,
    Right,
    Up,
};

inline constexpr HudAvoidanceDirection canyonHudAvoidanceDirection(
    CollisionHazard hazard)
{
    switch (hazard) {
        case CollisionHazard::LeftWall: return HudAvoidanceDirection::Right;
        case CollisionHazard::RightWall: return HudAvoidanceDirection::Left;
        case CollisionHazard::Floor: return HudAvoidanceDirection::Up;
        case CollisionHazard::None: break;
    }
    return HudAvoidanceDirection::None;
}

inline constexpr char canyonHudHazardCode(CollisionHazard hazard)
{
    switch (hazard) {
        case CollisionHazard::LeftWall: return 'L';
        case CollisionHazard::RightWall: return 'R';
        case CollisionHazard::Floor: return 'F';
        case CollisionHazard::None: break;
    }
    return '-';
}

inline constexpr float canyonHudHazardThreshold(CollisionHazard hazard)
{
    return hazard == CollisionHazard::Floor
        ? kCollisionFloorWarningClearance
        : kCollisionWallWarningClearance;
}

inline float canyonHudWarningClearance(const CollisionStatus& status)
{
    return status.warningHazard == CollisionHazard::Floor
        ? status.floorClearance
        : status.warningClearance;
}

inline float canyonHudImpactClearance(const CollisionStatus& status)
{
    switch (status.impactHazard) {
        case CollisionHazard::LeftWall: return status.leftClearance;
        case CollisionHazard::RightWall: return status.rightClearance;
        case CollisionHazard::Floor: return status.floorClearance;
        case CollisionHazard::None: break;
    }
    return status.clearance;
}

inline float canyonHudProximitySeverity(float clearance, CollisionHazard hazard)
{
    const float threshold = canyonHudHazardThreshold(hazard);
    return std::clamp(1.0f - clearance / threshold, 0.0f, 1.0f);
}

inline int canyonHudClearanceIndex(float clearance)
{
    return std::clamp(static_cast<int>(std::lround(clearance * 100.0f)),
                      0, 99);
}

inline float canyonHudHeadingFloatDegrees(const CanyonRouteFrame& route,
                                          float localYawDegrees)
{
    constexpr float kRadiansToDegrees = 57.2957795131f;
    float heading = std::fmod(
        std::atan2(route.tangentX, route.tangentZ) * kRadiansToDegrees +
            localYawDegrees,
        360.0f);
    return heading < 0.0f ? heading + 360.0f : heading;
}

inline int canyonHudHeadingDegrees(const CanyonRouteFrame& route,
                                   float localYawDegrees)
{
    const int rounded = static_cast<int>(std::lround(
        canyonHudHeadingFloatDegrees(route, localYawDegrees)));
    return rounded == 360 ? 0 : rounded;
}

inline constexpr const char* canyonHudViewModeLabel(bool aircraftVisible)
{
    return aircraftVisible ? "EXT" : "HUD";
}

inline constexpr HudInputAlert canyonHudInputAlert(const InputStatus& status)
{
    if (!status.axesConnected ||
        status.readiness == InputReadiness::Disconnected ||
        status.readiness == InputReadiness::Fault) {
        return HudInputAlert::AxisLost;
    }
    if (!status.actionsConnected ||
        status.readiness == InputReadiness::Degraded) {
        return HudInputAlert::ActionLost;
    }
    return HudInputAlert::None;
}

}  // namespace vector_canyon_fighter
