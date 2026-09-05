#pragma once

#include "model/flight_model.h"
#include "model/collision_model.h"
#include "model/explicit_canyon_stream.h"
#include "input/flight_input.h"
#include "render_budget_controller.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace vector_canyon_fighter {

struct CanyonCamera;

class Renderer {
public:
    void open(int width, int height);
    void close();
    // calibrationProgress: 0.0–1.0 while calibrating, <0 during normal gameplay.
    void render(const FlightState& flight, const ExplicitCanyonStream& terrain,
                const CollisionStatus& collision, float calibrationProgress,
                const InputStatus& inputStatus, bool aircraftVisible = true,
                TerrainRenderDetail terrainDetail = TerrainRenderDetail::High);
    void renderExplicitPreview(const FlightState& flight, const ExplicitCanyonStream& terrain);

private:
    struct ProjectedCanyonPoint {
        int16_t x = 0;
        int16_t y = 0;
    };
    static_assert(sizeof(ProjectedCanyonPoint) == 4, "Explicit projected points must remain packed XY pairs");

    bool drawExplicitTerrain(const CanyonCamera& camera, const ExplicitCanyonStream& terrain,
                             uint16_t terrainPrimary, uint16_t terrainMid,
                             uint16_t terrainSecondary,
                             TerrainRenderDetail terrainDetail);
    void renderGame(const FlightState& flight, const ExplicitCanyonStream& terrain,
                    const CollisionStatus& collision, float calibrationProgress,
                    const InputStatus& inputStatus, bool aircraftVisible,
                    TerrainRenderDetail terrainDetail);

    std::array<ProjectedCanyonPoint, ExplicitCanyonStream::kSliceCount * ExplicitCanyonStream::kProfileCount>
        _explicitTerrainPoints = {};
    std::array<ProjectedCanyonPoint, ExplicitCanyonStream::kFarSliceCount * ExplicitCanyonStream::kProfileCount>
        _explicitFarTerrainPoints = {};
    static_assert(sizeof(_explicitTerrainPoints) == 3400,
                  "The 34 x 25 explicit projection cache must remain exactly 3400 bytes");
    static_assert(sizeof(_explicitFarTerrainPoints) == 200,
                  "The sparse 2 x 25 far projection cache must remain exactly 200 bytes");
    int _width = 0;
    int _height = 0;
};

static_assert(sizeof(Renderer) <= 3700,
              "Explicit projection cache exceeded the reviewed heap budget");

}  // namespace vector_canyon_fighter
