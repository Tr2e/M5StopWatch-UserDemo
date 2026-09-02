#pragma once

#include "model/flight_model.h"
#include "model/collision_model.h"
#include "model/terrain_stream.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace vector_canyon_fighter {

class Renderer {
public:
    void open(int width, int height);
    void close();
    // calibrationProgress: 0.0–1.0 while calibrating, <0 during normal gameplay.
    void render(const FlightState& flight, const TerrainStream& terrain, const CollisionStatus& collision, float calibrationProgress);

private:
    struct LegacyProjectedTerrainRow {
        std::array<int16_t, TerrainStream::kColumnCount> x;
        std::array<int16_t, TerrainStream::kColumnCount> y;
    };
    static_assert(sizeof(LegacyProjectedTerrainRow) == TerrainStream::kColumnCount * sizeof(int16_t) * 2,
                  "Legacy projection rows must remain tightly packed");

    bool drawLegacyTerrain(const FlightState& flight, const TerrainStream& terrain, int centerX,
                           uint16_t terrainPrimary, uint16_t terrainMid, uint16_t terrainSecondary,
                           int& vanishingX);

    std::array<LegacyProjectedTerrainRow, TerrainStream::kSliceCount + 1> _legacyTerrainRows = {};
    std::array<std::size_t, TerrainStream::kSliceCount + 1> _legacyTerrainSourceRows = {};
    int _width = 0;
    int _height = 0;
};

}  // namespace vector_canyon_fighter
