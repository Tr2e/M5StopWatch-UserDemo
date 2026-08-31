#pragma once

#include "vector_canyon_renderer.h"
#include "model/flight_model.h"
#include "model/terrain_stream.h"

#include <apps/common/key_manager/key_manager.h>
#include <memory>
#include <mooncake.h>

class AppVectorCanyonFighter : public mooncake::AppAbility {
public:
    AppVectorCanyonFighter();
    void onCreate() override;
    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    std::unique_ptr<input::KeyManager> _keys;
    vector_canyon_fighter::Renderer _renderer;
    vector_canyon_fighter::FlightModel _flightModel;
    vector_canyon_fighter::TerrainStream _terrain;
    uint32_t _lastFrameMs = 0;
    uint32_t _lastSimulationMs = 0;
    float _simulationAccumulator = 0.0f;
};
