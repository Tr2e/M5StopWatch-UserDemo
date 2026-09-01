#pragma once

#include "vector_canyon_renderer.h"
#include "input/input_provider.h"
#include "model/flight_model.h"
#include "model/collision_model.h"
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
    std::unique_ptr<vector_canyon_fighter::InputProvider> _inputProvider;
    vector_canyon_fighter::Renderer _renderer;
    vector_canyon_fighter::FlightModel _flightModel;
    vector_canyon_fighter::CollisionModel _collisionModel;
    vector_canyon_fighter::CollisionStatus _collisionStatus;
    vector_canyon_fighter::TerrainStream _terrain;
    uint32_t _lastFrameMs = 0;
    uint32_t _lastSimulationMs = 0;
    uint32_t _performanceWindowStartedMs = 0;
    uint32_t _renderTimeTotalMs = 0;
    uint32_t _renderTimeMaxMs = 0;
    uint16_t _renderedFrames = 0;
    uint16_t _boostedFrames = 0;
    uint16_t _simulationClampCount = 0;
    float _simulationAccumulator = 0.0f;
};
