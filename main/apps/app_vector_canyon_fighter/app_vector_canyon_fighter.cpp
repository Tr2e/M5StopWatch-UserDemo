#include "app_vector_canyon_fighter.h"

#include <hal/hal.h>
#include <mooncake_log.h>

#include "input/flight_input.h"

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
    GetHAL().stopLvglUpdate();

    const auto& display = GetHAL().getDisplay();
    _renderer.open(display.width(), display.height());
    _flightModel.reset();
    _terrain.reset(kTerrainSeed);
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

    vector_canyon_fighter::FlightInput cruiseInput;
    int simulatedSteps = 0;
    while (_simulationAccumulator >= kSimulationStepSeconds && simulatedSteps < 3) {
        _flightModel.step(cruiseInput, kSimulationStepSeconds);
        _simulationAccumulator -= kSimulationStepSeconds;
        ++simulatedSteps;
    }
    if (simulatedSteps == 3) _simulationAccumulator = 0.0f;
    _terrain.update(_flightModel.state().forwardDistance);

    if (_lastFrameMs != 0 && nowMs - _lastFrameMs < kFrameIntervalMs) return;
    _lastFrameMs = nowMs;
    _renderer.render(_flightModel.state(), _terrain);
}

void AppVectorCanyonFighter::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");
    _renderer.close();
    _keys.reset();
    GetHAL().startLvglUpdate();
}
