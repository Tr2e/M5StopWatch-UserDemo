#pragma once

#include "model/flight_model.h"
#include "model/collision_model.h"
#include "model/explicit_canyon_stream.h"
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
    void renderExplicitPreview(const FlightState& flight, const ExplicitCanyonStream& terrain);

private:
    struct LegacyProjectedTerrainRow {
        std::array<int16_t, TerrainStream::kColumnCount> x;
        std::array<int16_t, TerrainStream::kColumnCount> y;
    };
    static_assert(sizeof(LegacyProjectedTerrainRow) == TerrainStream::kColumnCount * sizeof(int16_t) * 2,
                  "Legacy projection rows must remain tightly packed");

    struct ProjectedCanyonPoint {
        int16_t x = 0;
        int16_t y = 0;
    };
    static_assert(sizeof(ProjectedCanyonPoint) == 4, "Explicit projected points must remain packed XY pairs");

    bool drawLegacyTerrain(const FlightState& flight, const TerrainStream& terrain, int centerX,
                           uint16_t terrainPrimary, uint16_t terrainMid, uint16_t terrainSecondary,
                           int& vanishingX);
    bool drawExplicitTerrain(const FlightState& flight, const ExplicitCanyonStream& terrain,
                             uint16_t terrainPrimary, uint16_t terrainMid, uint16_t terrainSecondary);

    std::array<LegacyProjectedTerrainRow, TerrainStream::kSliceCount + 1> _legacyTerrainRows = {};
    std::array<std::size_t, TerrainStream::kSliceCount + 1> _legacyTerrainSourceRows = {};
    std::array<ProjectedCanyonPoint, ExplicitCanyonStream::kSliceCount * ExplicitCanyonStream::kProfileCount>
        _explicitTerrainPoints = {};
    static_assert(sizeof(_explicitTerrainPoints) == 3400,
                  "The 34 x 25 explicit projection cache must remain exactly 3400 bytes");
    int _width = 0;
    int _height = 0;
};

static_assert(sizeof(Renderer) <= 7700,
              "Transitional legacy + explicit projection caches exceeded the reviewed heap budget");

}  // namespace vector_canyon_fighter
