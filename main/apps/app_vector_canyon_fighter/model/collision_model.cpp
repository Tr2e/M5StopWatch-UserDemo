#include "collision_model.h"

#include <algorithm>
#include <cmath>

namespace vector_canyon_fighter {
namespace {

constexpr float kShipHalfWidth = 0.24f;
constexpr float kShipFloorClearance = 0.28f;
constexpr float kWarningClearance = 0.48f;

}  // namespace

CollisionStatus CollisionModel::evaluate(const FlightState& flight, const TerrainStream& terrain) const
{
    CollisionStatus status;
    for (const auto& slice : terrain.slices()) {
        if (slice.z > 4.8f) continue;

        const float lateralClearance = slice.halfWidth - std::fabs(flight.lateralOffset - slice.center) - kShipHalfWidth;
        const float floorClearance = flight.altitude - slice.floor - kShipFloorClearance;
        status.clearance = std::min(status.clearance, std::min(lateralClearance, floorClearance));
    }
    status.warning = status.clearance < kWarningClearance;
    status.collided = status.clearance < 0.0f;
    return status;
}

}  // namespace vector_canyon_fighter
