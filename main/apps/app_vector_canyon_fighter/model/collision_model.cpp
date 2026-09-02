#include "collision_model.h"

#include <mooncake_log.h>

namespace vector_canyon_fighter {
namespace {

constexpr uint32_t kLogIntervalFrames = 60;

}  // namespace

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
