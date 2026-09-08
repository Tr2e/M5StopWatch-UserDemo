#pragma once

#include "../controller/game_flow.h"
#include "../controller/garage_selection.h"
#include "../model/car_catalog.h"
#include "../model/overpass_track.h"
#include "render_budget.h"
#include "pencil_scene.h"
#include "../input/racer_input.h"

#include <cstdint>
#include <array>

namespace lets_and_go {

class GarageRenderer {
public:
    void open(int width, int height);
    void close();
    void render(const GameFlow& flow, const GarageSelection& selection,
                uint32_t screenElapsedMs, PencilDetail detail,
                const RacerInputStatus& inputStatus = {});

private:
    const CarWireframe& showcaseMesh(CarId car);

    CarWireframe _showcaseMesh{};
    OverpassTrack _track{};
    PencilTrack _trackPreview{};
    CarId _cachedCar = CarId::CycloneMagnum;
    bool _meshCached = false;
    int _width = 0;
    int _height = 0;
};

static_assert(sizeof(GarageRenderer) <= 10000u,
              "garage renderer cache exceeded its reviewed resident budget");

}  // namespace lets_and_go
