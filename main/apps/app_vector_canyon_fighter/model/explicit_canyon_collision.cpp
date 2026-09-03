#include "collision_model.h"
#include "aircraft_geometry.h"

#include <algorithm>
#include <limits>

namespace vector_canyon_fighter {
namespace {

constexpr float kWarningClearance = 0.48f;
constexpr float kFloorWarningClearance = 0.10f;
constexpr float kWarningLookAhead = 4.8f;
constexpr int kWarningSamples = 4;

void updateMinimum(float candidate, CollisionHazard hazard,
                   float& minimum, CollisionHazard& minimumHazard)
{
    if (candidate < minimum) {
        minimum = candidate;
        minimumHazard = hazard;
    }
}

}  // namespace

CollisionStatus evaluateExplicitCanyonCollision(const FlightState& flight,
                                                const ExplicitCanyonStream& terrain)
{
    CollisionStatus status;
    status.leftClearance = std::numeric_limits<float>::max();
    status.rightClearance = std::numeric_limits<float>::max();
    const float wallOutset = explicitCanyonWallOutsetAtHeight(flight.altitude);
    for (const AircraftCollisionStation& station : kAircraftCollisionStations) {
        const CanyonBoundary boundary =
            terrain.boundaryAt(terrain.playerWorldS() + station.forwardLead);
        status.leftClearance = std::min(
            status.leftClearance,
            boundary.leftWidth + wallOutset + flight.lateralOffset - station.halfWidth);
        status.rightClearance = std::min(
            status.rightClearance,
            boundary.rightWidth + wallOutset - flight.lateralOffset - station.halfWidth);
    }
    status.floorClearance = flight.altitude - kAircraftFloorClearance;
    status.clearance = std::min({status.leftClearance, status.rightClearance, status.floorClearance});
    status.warningClearance = std::min(status.leftClearance, status.rightClearance);
    CollisionHazard closestWall = status.leftClearance <= status.rightClearance
                                      ? CollisionHazard::LeftWall
                                      : CollisionHazard::RightWall;
    status.warningHazard = closestWall;

    for (int sample = 1; sample <= kWarningSamples; ++sample) {
        const float worldS = terrain.playerWorldS() + kAircraftCollisionStations.back().forwardLead +
                             kWarningLookAhead * static_cast<float>(sample) /
                                 static_cast<float>(kWarningSamples);
        const CanyonBoundary boundary = terrain.boundaryAt(worldS);
        const float left = boundary.leftWidth + wallOutset + flight.lateralOffset -
                           kAircraftMaximumHalfWidth;
        const float right = boundary.rightWidth + wallOutset - flight.lateralOffset -
                            kAircraftMaximumHalfWidth;
        updateMinimum(left, CollisionHazard::LeftWall,
                      status.warningClearance, status.warningHazard);
        updateMinimum(right, CollisionHazard::RightWall,
                      status.warningClearance, status.warningHazard);
    }
    const float wallWarningMargin = status.warningClearance - kWarningClearance;
    const float floorWarningMargin = status.floorClearance - kFloorWarningClearance;
    status.warning = wallWarningMargin < 0.0f || floorWarningMargin < 0.0f;
    if (status.warning && floorWarningMargin < wallWarningMargin) {
        status.warningHazard = CollisionHazard::Floor;
    }
    status.collided = status.clearance < 0.0f;
    if (status.collided) {
        status.impactHazard = CollisionHazard::LeftWall;
        float impactClearance = status.leftClearance;
        updateMinimum(status.rightClearance, CollisionHazard::RightWall,
                      impactClearance, status.impactHazard);
        updateMinimum(status.floorClearance, CollisionHazard::Floor,
                      impactClearance, status.impactHazard);
    }
    return status;
}

}  // namespace vector_canyon_fighter
