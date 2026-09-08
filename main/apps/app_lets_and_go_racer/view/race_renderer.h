#pragma once

#include "../controller/game_flow.h"
#include "../controller/race_controller.h"
#include "../controller/results_selection.h"
#include "../model/car_catalog.h"
#include "render_budget.h"
#include "pencil_scene.h"
#include "car_surface_raster.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace lets_and_go {

struct RaceSurfaceMesh {
    static constexpr std::size_t kMaximumPanels=1536u;
    std::array<CarPanel,kMaximumPanels> panels{};
    std::size_t count=0;
};
struct RaceSurfaceCache {
    std::array<RaceSurfaceMesh,kCarCount> meshes{};
    CarSurfaceRaster<112,112> raster{};
    CarSurfaceDetail detail=CarSurfaceDetail::Medium;
};
static_assert(sizeof(RaceSurfaceCache)<=420000u,"race surface working-set budget");

class RaceRenderer {
public:
    void open(int width, int height);
    void close();
    void render(const GameFlow& flow, const RaceController& race,
                const ResultsSelection& results,
                uint32_t screenElapsedMs, bool pausedForInputLoss,
                PencilDetail detail);

private:
    std::unique_ptr<RaceSurfaceCache> _surface;
    PencilTrack _trackGeometry{};
    PencilOcclusion _occlusion{};
    std::array<int16_t, 32u> _mapX{};
    std::array<int16_t, 32u> _mapY{};
    int _width = 0;
    int _height = 0;
};

static_assert(sizeof(RaceRenderer) <= 40000u,
              "race renderer cache exceeded its reviewed resident budget");

}  // namespace lets_and_go
