#pragma once

#include "../controller/game_flow.h"
#include "../controller/garage_selection.h"
#include "../model/car_catalog.h"
#include "../model/overpass_track.h"

#include <cstdint>
#include <array>

namespace lets_and_go {

struct TrackPreviewGeometry {
    static constexpr std::size_t kSegments = 80u;
    std::array<TrackVec3, kSegments + 1u> left{};
    std::array<TrackVec3, kSegments + 1u> right{};
    std::array<TrackLayer, kSegments> layer{};
};

class GarageRenderer {
public:
    void open(int width, int height);
    void close();
    void render(const GameFlow& flow, const GarageSelection& selection,
                uint32_t screenElapsedMs);

private:
    const CarWireframe& showcaseMesh(CarId car);

    CarWireframe _showcaseMesh{};
    OverpassTrack _track{};
    TrackPreviewGeometry _trackPreview{};
    CarId _cachedCar = CarId::CycloneMagnum;
    bool _meshCached = false;
    int _width = 0;
    int _height = 0;
};

}  // namespace lets_and_go
