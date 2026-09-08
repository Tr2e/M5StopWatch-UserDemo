#pragma once

#include "../controller/game_flow.h"
#include "../controller/garage_selection.h"
#include "../model/car_catalog.h"

#include <cstdint>

namespace lets_and_go {

class GarageRenderer {
public:
    void open(int width, int height);
    void close();
    void render(const GameFlow& flow, const GarageSelection& selection,
                uint32_t screenElapsedMs);

private:
    const CarWireframe& showcaseMesh(CarId car);

    CarWireframe _showcaseMesh{};
    CarId _cachedCar = CarId::CycloneMagnum;
    bool _meshCached = false;
    int _width = 0;
    int _height = 0;
};

}  // namespace lets_and_go
