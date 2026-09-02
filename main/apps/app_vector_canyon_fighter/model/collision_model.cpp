#include "collision_model.h"

#include <algorithm>
#include <cmath>
#include <mooncake_log.h>

namespace vector_canyon_fighter {
namespace {

constexpr float kShipHalfWidth = 0.24f;
constexpr float kShipFloorClearance = 0.20f;
constexpr float kWarningClearance = 0.48f;
constexpr uint32_t kLogIntervalFrames = 60;

}  // namespace

CollisionStatus CollisionModel::evaluate(const FlightState& flight, const TerrainStream& terrain) const
{
    CollisionStatus status;
    float worstLateral = 99.0f, worstFloor = 99.0f;
    float worstFlrCenter = 0.0f, worstFlrZ = 0.0f, worstFlrFloor = 0.0f, worstFlrHFloor = 0.0f;
    float worstLatCenter = 0.0f, worstLatZ = 0.0f, worstLatHW = 0.0f;

    for (const auto& slice : terrain.slices()) {
        if (slice.z > 4.8f) continue;

        const float lateralClearance = slice.halfWidth - std::fabs(flight.lateralOffset - slice.center) - kShipHalfWidth;
        const float highestFloor = slice.floor + std::fabs(slice.floorTilt) + std::max(slice.floorCrown, 0.0f);
        const float floorClearance = flight.altitude - highestFloor - kShipFloorClearance;

        if (floorClearance < worstFloor) {
            worstFloor = floorClearance;
            worstFlrCenter = slice.center; worstFlrZ = slice.z;
            worstFlrFloor = slice.floor;   worstFlrHFloor = highestFloor;
        }
        if (lateralClearance < worstLateral) {
            worstLateral = lateralClearance;
            worstLatCenter = slice.center; worstLatZ = slice.z; worstLatHW = slice.halfWidth;
        }
        status.clearance = std::min(status.clearance, std::min(lateralClearance, floorClearance));
    }

    status.warning = status.clearance < kWarningClearance;
    status.collided = status.clearance < 0.0f;

    ++_frameCount;
    if (status.collided || _frameCount >= kLogIntervalFrames) {
        _frameCount = 0;
        mclog::tagInfo("COL",
            "alt={:.2f} lat={:.2f} | LAT: ctr={:.2f} hw={:.2f} z={:.2f} clr={:.2f} | FLR: ctr={:.2f} z={:.2f} flr={:.2f} hf={:.2f} clr={:.2f}{}",
            flight.altitude, flight.lateralOffset,
            worstLatCenter, worstLatHW, worstLatZ, worstLateral,
            worstFlrCenter, worstFlrZ, worstFlrFloor, worstFlrHFloor, worstFloor,
            status.collided ? " <COLL>" : (status.warning ? " <WARN>" : ""));
    }

    return status;
}

CollisionStatus CollisionModel::evaluate(const FlightState& flight,
                                         const ExplicitCanyonStream& terrain) const
{
    const CollisionStatus status = evaluateExplicitCanyonCollision(flight, terrain);
    const bool collisionStarted = status.collided && !_wasCollided;
    _wasCollided = status.collided;
    ++_frameCount;
    if (collisionStarted || _frameCount >= kLogIntervalFrames) {
        _frameCount = 0;
        const CanyonBoundary boundary = terrain.boundaryAt(terrain.playerWorldS());
        mclog::tagInfo(
            "COL",
            "s={:.2f} alt={:.2f} lat={:.2f} | L: w={:.2f} clr={:.2f} | R: w={:.2f} clr={:.2f} | floor={:.2f} warn={:.2f}{}",
            terrain.playerWorldS(), flight.altitude, flight.lateralOffset,
            boundary.leftWidth, status.leftClearance, boundary.rightWidth, status.rightClearance,
            status.floorClearance, status.warningClearance,
            status.collided ? " <COLL>" : (status.warning ? " <WARN>" : ""));
    }
    return status;
}

}  // namespace vector_canyon_fighter
