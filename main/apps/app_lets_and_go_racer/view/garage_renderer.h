#pragma once

#include "../controller/game_flow.h"
#include "../controller/garage_selection.h"
#include "../model/car_catalog.h"
#include "../model/car_display_mesh.h"
#include "../model/overpass_track.h"
#include "render_budget.h"
#include "pencil_scene.h"
#include "car_surface_raster.h"
#include "../input/racer_input.h"

#include <cstdint>
#include <array>
#include <memory>

namespace lets_and_go {

struct GarageSurfaceCache {
    CarDisplayMesh mesh{};
    CarSurfaceRaster<352,288> raster{};
    PencilOcclusion trackSurfaces{};
};
static_assert(sizeof(GarageSurfaceCache)<=592000u,"garage surface working-set budget");

class GarageRenderer {
public:
    void open(int width, int height);
    void close();
    void render(const GameFlow& flow, const GarageSelection& selection,
                uint32_t screenElapsedMs, PencilDetail detail,
                const RacerInputStatus& inputStatus = {});

private:
    const CarDisplayMesh& showcaseMesh(CarId car, PencilDetail detail);

    std::unique_ptr<GarageSurfaceCache> _surface;
    OverpassTrack _track{};
    PencilTrack _trackPreview{};
    CarId _cachedCar = CarId::CycloneMagnum;
    PencilDetail _cachedDetail = PencilDetail::High;
    bool _meshCached = false;
    int _width = 0;
    int _height = 0;
};

static_assert(sizeof(GarageRenderer) <= 3500u,
              "garage renderer cache exceeded its reviewed resident budget");

}  // namespace lets_and_go
