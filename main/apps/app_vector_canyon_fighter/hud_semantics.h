#pragma once

#include "input/flight_input.h"
#include "model/explicit_canyon_types.h"

#include <cmath>

namespace vector_canyon_fighter {

enum class HudInputAlert : uint8_t {
    None,
    AxisLost,
    ActionLost,
};

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
