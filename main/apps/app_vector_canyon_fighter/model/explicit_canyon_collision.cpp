#include "collision_model.h"

#include <algorithm>

namespace vector_canyon_fighter {
namespace {

constexpr float kShipHalfWidth = 0.24f;
constexpr float kShipFloorClearance = 0.20f;
constexpr float kWarningClearance = 0.48f;
constexpr float kFloorWarningClearance = 0.10f;
constexpr float kWarningLookAhead = 4.8f;
constexpr int kWarningSamples = 4;

}  // namespace

CollisionStatus evaluateExplicitCanyonCollision(const FlightState& flight,
                                                const ExplicitCanyonStream& terrain)
{
    CollisionStatus status;
    const CanyonBoundary current = terrain.boundaryAt(terrain.playerWorldS());
    status.leftClearance = current.leftWidth + flight.lateralOffset - kShipHalfWidth;
    status.rightClearance = current.rightWidth - flight.lateralOffset - kShipHalfWidth;
    status.floorClearance = flight.altitude - kShipFloorClearance;
    status.clearance = std::min({status.leftClearance, status.rightClearance, status.floorClearance});
    status.warningClearance = std::min(status.leftClearance, status.rightClearance);

    for (int sample = 1; sample <= kWarningSamples; ++sample) {
        const float worldS = terrain.playerWorldS() +
                             kWarningLookAhead * static_cast<float>(sample) /
                                 static_cast<float>(kWarningSamples);
        const CanyonBoundary boundary = terrain.boundaryAt(worldS);
        const float left = boundary.leftWidth + flight.lateralOffset - kShipHalfWidth;
        const float right = boundary.rightWidth - flight.lateralOffset - kShipHalfWidth;
        status.warningClearance = std::min(status.warningClearance, std::min(left, right));
    }
    status.warning = status.warningClearance < kWarningClearance ||
                     status.floorClearance < kFloorWarningClearance;
    status.collided = status.clearance < 0.0f;
    return status;
}

}  // namespace vector_canyon_fighter
