#include "../main/apps/app_vector_canyon_fighter/hud_semantics.h"
#include "../main/apps/app_vector_canyon_fighter/model/flight_model.h"

#include <iostream>

namespace {

using namespace vector_canyon_fighter;

bool check(bool condition, const char* message)
{
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}

bool validateSpeedSemantics()
{
    FlightState state{};
    state.speed = 72.0f;
    bool valid = check(effectiveFlightForwardSpeed(state) == 72.0f,
                       "cruise HUD speed changed without boost");
    state.boostAmount = 1.0f;
    valid &= check(effectiveFlightForwardSpeed(state) == 116.0f,
                   "HUD speed does not include the model boost gain");
    return valid;
}

bool validateHeadingSemantics()
{
    CanyonRouteFrame route{};
    bool valid = check(canyonHudHeadingDegrees(route, 0.0f) == 0,
                       "+Z route must be heading 000");
    valid &= check(std::abs(canyonHudHeadingFloatDegrees(route, 0.25f) - 0.25f) <
                       0.001f,
                   "continuous heading lost its sub-degree precision");
    route.tangentX = 1.0f;
    route.tangentZ = 0.0f;
    valid &= check(canyonHudHeadingDegrees(route, 0.0f) == 90,
                   "+X route must be heading 090");
    route.tangentX = 0.0f;
    route.tangentZ = -1.0f;
    valid &= check(canyonHudHeadingDegrees(route, 0.0f) == 180,
                   "-Z route must be heading 180");
    route.tangentX = -1.0f;
    route.tangentZ = 0.0f;
    valid &= check(canyonHudHeadingDegrees(route, 0.0f) == 270,
                   "-X route must be heading 270");
    valid &= check(canyonHudHeadingDegrees(route, -8.0f) == 262 &&
                       canyonHudHeadingDegrees(route, 92.0f) == 2,
                   "local yaw did not wrap around the heading scale");
    route.tangentX = 0.0f;
    route.tangentZ = 1.0f;
    valid &= check(std::abs(canyonHudHeadingFloatDegrees(route, -0.25f) -
                            359.75f) < 0.001f &&
                       canyonHudHeadingDegrees(route, -0.25f) == 0,
                   "continuous heading did not wrap smoothly through north");
    return valid;
}

bool validateModeAndInputSemantics()
{
    bool valid = check(canyonHudViewModeLabel(true)[0] == 'E' &&
                           canyonHudViewModeLabel(false)[0] == 'H',
                       "view labels do not reflect exterior/HUD modes");
    InputStatus status{};
    valid &= check(canyonHudInputAlert(status) == HudInputAlert::AxisLost,
                   "disconnected axes did not produce an axis alert");
    status.axesConnected = true;
    status.readiness = InputReadiness::Degraded;
    valid &= check(canyonHudInputAlert(status) == HudInputAlert::ActionLost,
                   "degraded action input did not produce an action alert");
    status.actionsConnected = true;
    status.readiness = InputReadiness::Ready;
    valid &= check(canyonHudInputAlert(status) == HudInputAlert::None,
                   "ready input produced a false HUD alert");
    return valid;
}

}  // namespace

int main()
{
    bool valid = validateSpeedSemantics();
    valid &= validateHeadingSemantics();
    valid &= validateModeAndInputSemantics();
    return valid ? 0 : 1;
}
