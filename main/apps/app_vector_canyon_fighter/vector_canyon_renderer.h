#pragma once

#include "model/flight_model.h"
#include "model/collision_model.h"
#include "model/terrain_stream.h"

#include <cstdint>

namespace vector_canyon_fighter {

class Renderer {
public:
    void open(int width, int height);
    void close();
    // calibrationProgress: 0.0–1.0 while calibrating, <0 during normal gameplay.
    void render(const FlightState& flight, const TerrainStream& terrain, const CollisionStatus& collision, float calibrationProgress);

private:
    int _width = 0;
    int _height = 0;
};

}  // namespace vector_canyon_fighter
