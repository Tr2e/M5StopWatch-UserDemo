#pragma once

#include "../controller/game_flow.h"
#include "../controller/garage_selection.h"
#include "../controller/garage_view_controller.h"
#include "../model/car_catalog.h"
#include "../model/car_display_mesh.h"
#include "../model/overpass_track.h"
#include "render_budget.h"
#include "pencil_scene.h"
#include "car_surface_raster.h"
#include "garage_inspection_cache.h"
#include "../input/racer_input.h"

#include <cstdint>
#include <array>
#include <memory>

namespace lets_and_go {

struct ReferenceGarageSurfaceCache {
    CarDisplayMesh mesh{};
    CarSurfaceRaster<352,288> raster{};
    PencilOcclusion trackSurfaces{};
};
class ReferenceGarageRenderer {
public:
    void open(int width, int height);
    void close();
    void render(const GameFlow& flow, const GarageSelection& selection,
                uint32_t screenElapsedMs, PencilDetail detail,
                const RacerInputStatus& inputStatus = {},const GarageViewState& view = {}, bool deviceControls = false,
                int inspectionPercent = 100);
    int inspectionPercent() const { return _inspectionPercent; }

private:
    const CarDisplayMesh& showcaseMesh(CarId car, PencilDetail detail);

    std::unique_ptr<ReferenceGarageSurfaceCache> _surface;
    OverpassTrack _track{};
    PencilTrack _trackPreview{};
    CarId _cachedCar = CarId::CycloneMagnum;
    PencilDetail _cachedDetail = PencilDetail::High;
    bool _meshCached = false;
    int _width = 0;
    int _height = 0;
    int _inspectionPercent = 100;
};

static_assert(sizeof(ReferenceGarageRenderer) <= 3500u,
              "garage renderer cache exceeded its reviewed resident budget");

}  // namespace lets_and_go
