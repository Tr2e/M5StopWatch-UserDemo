#pragma once

#include "model/flight_model.h"
#include "model/terrain_stream.h"

#include <cstdint>

namespace vector_canyon_fighter {

class Renderer {
public:
    void open(int width, int height);
    void close();
    void render(const FlightState& flight, const TerrainStream& terrain);

private:
    int _width = 0;
    int _height = 0;
};

}  // namespace vector_canyon_fighter
