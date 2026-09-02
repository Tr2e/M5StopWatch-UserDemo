#pragma once

#include "flight_model.h"
#include "explicit_canyon_stream.h"
#include "terrain_stream.h"

namespace vector_canyon_fighter {

struct CollisionStatus {
    float clearance = 1.0f;
    float leftClearance = 1.0f;
    float rightClearance = 1.0f;
    float floorClearance = 1.0f;
    float warningClearance = 1.0f;
    bool warning = false;
    bool collided = false;
};

CollisionStatus evaluateExplicitCanyonCollision(const FlightState& flight,
                                                const ExplicitCanyonStream& terrain);

class CollisionModel {
public:
    CollisionStatus evaluate(const FlightState& flight, const TerrainStream& terrain) const;
    CollisionStatus evaluate(const FlightState& flight, const ExplicitCanyonStream& terrain) const;

private:
    mutable uint32_t _frameCount = 0;
    mutable bool _wasCollided = false;
};

}  // namespace vector_canyon_fighter
