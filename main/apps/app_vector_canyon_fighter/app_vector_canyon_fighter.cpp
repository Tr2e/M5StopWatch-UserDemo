#include "app_vector_canyon_fighter.h"

#include <hal/hal.h>
#include <mooncake_log.h>

#include "input/flight_input.h"
#include "input/imu_button_input_provider.h"

namespace {
constexpr uint32_t kFrameIntervalMs = 33;
constexpr float kSimulationStepSeconds = 1.0f / 60.0f;
constexpr uint32_t kTerrainSeed = 0xC4A71001u;
}

AppVectorCanyonFighter::AppVectorCanyonFighter()
{
    setAppInfo().name = "Vector Run";
}

void AppVectorCanyonFighter::onCreate()
{
    mclog::tagInfo(getAppInfo().name, "on create");
}

void AppVectorCanyonFighter::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");
    _keys = std::make_unique<input::KeyManager>();
    _inputProvider = std::make_unique<vector_canyon_fighter::ImuButtonInputProvider>();
    _inputProvider->open();
    GetHAL().stopLvglUpdate();

    const auto& display = GetHAL().getDisplay();
    _renderer.open(display.width(), display.height());
    _flightModel.reset();
    _terrain.reset(kTerrainSeed);
    _collisionStatus = {};
    _lastFrameMs = 0;
    _lastSimulationMs = 0;
    _simulationAccumulator = 0.0f;
}

void AppVectorCanyonFighter::onRunning()
{
    GetHAL().updateButtonStates();
    if (_keys && _keys->update(false) == input::KeyEvent::GoHome) {
        close();
        return;
    }

    const uint32_t nowMs = GetHAL().millis();
    if (_lastSimulationMs == 0) _lastSimulationMs = nowMs;
    const uint32_t elapsedMs = nowMs - _lastSimulationMs;
    _lastSimulationMs = nowMs;
    _simulationAccumulator += static_cast<float>(elapsedMs) / 1000.0f;

    const auto flightInput = _inputProvider ? _inputProvider->sample(nowMs) : vector_canyon_fighter::FlightInput{};
    const bool wasCollided = _flightModel.state().collided;
    int simulatedSteps = 0;
    while (_simulationAccumulator >= kSimulationStepSeconds && simulatedSteps < 3) {
        _flightModel.step(flightInput, kSimulationStepSeconds);
        _simulationAccumulator -= kSimulationStepSeconds;
        ++simulatedSteps;
    }
    if (simulatedSteps == 3) _simulationAccumulator = 0.0f;
    if (wasCollided && !_flightModel.state().collided) _terrain.reset(kTerrainSeed);
    _terrain.update(_flightModel.state().forwardDistance);
    _collisionStatus = _collisionModel.evaluate(_flightModel.state(), _terrain);
    _flightModel.setCollided(_collisionStatus.collided);

    if (_lastFrameMs != 0 && nowMs - _lastFrameMs < kFrameIntervalMs) return;
    _lastFrameMs = nowMs;
    _renderer.render(_flightModel.state(), _terrain, _collisionStatus, flightInput.valid);
}

void AppVectorCanyonFighter::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");
    _renderer.close();
    if (_inputProvider) _inputProvider->close();
    _inputProvider.reset();
    _keys.reset();
    GetHAL().startLvglUpdate();
}
