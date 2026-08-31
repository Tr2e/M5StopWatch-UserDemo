#pragma once

#include "flight_model.h"
#include "terrain_stream.h"

namespace vector_canyon_fighter {

struct CollisionStatus {
    float clearance = 1.0f;
    bool warning = false;
    bool collided = false;
};

class CollisionModel {
public:
    CollisionStatus evaluate(const FlightState& flight, const TerrainStream& terrain) const;
};

}  // namespace vector_canyon_fighter
