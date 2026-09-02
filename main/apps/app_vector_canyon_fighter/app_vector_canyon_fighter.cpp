#include "app_vector_canyon_fighter.h"

#include <hal/hal.h>
#include <mooncake_log.h>

#include <algorithm>

#include "input/flight_input.h"
#include "input/imu_button_input_provider.h"

namespace {
constexpr uint32_t kFrameIntervalMs = 33;
constexpr uint32_t kPerformanceWindowMs = 5000;
#if !(VECTOR_CANYON_EXPLICIT_TERRAIN && VECTOR_CANYON_EXPLICIT_STATIC_BASELINE)
constexpr float kSimulationStepSeconds = 1.0f / 60.0f;
constexpr int kMaxSimulationSteps = 5;
constexpr uint32_t kTerrainSeed = 0xC4A71001u;
#endif
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
#if VECTOR_CANYON_EXPLICIT_TERRAIN && VECTOR_CANYON_EXPLICIT_STATIC_BASELINE
    _inputProvider.reset();
#else
    _inputProvider = std::make_unique<vector_canyon_fighter::ImuButtonInputProvider>();
    _inputProvider->open();
#endif
    GetHAL().stopLvglUpdate();

    const auto& display = GetHAL().getDisplay();
    _renderer.open(display.width(), display.height());
    _flightModel.reset();
#if VECTOR_CANYON_EXPLICIT_TERRAIN && VECTOR_CANYON_EXPLICIT_STATIC_BASELINE
    _terrain.resetStraightBaseline();
#else
    _terrain.reset(kTerrainSeed);
#endif
    _collisionStatus = {};
#if VECTOR_CANYON_EXPLICIT_TERRAIN && VECTOR_CANYON_EXPLICIT_STATIC_BASELINE
    _calibrationPhase = false;
#else
    _calibrationPhase = true;
#endif
    _lastFrameMs = 0;
    _lastSimulationMs = 0;
    _performanceWindowStartedMs = GetHAL().millis();
    _renderTimeTotalMs = 0;
    _renderTimeMaxMs = 0;
    _renderedFrames = 0;
    _boostedFrames = 0;
    _simulationClampCount = 0;
    _simulationAccumulator = 0.0f;
}

void AppVectorCanyonFighter::onRunning()
{
    GetHAL().updateButtonStates();
    if (_keys && _keys->update(false) == input::KeyEvent::GoHome) {
        close();
        return;
    }

#if VECTOR_CANYON_EXPLICIT_TERRAIN && VECTOR_CANYON_EXPLICIT_STATIC_BASELINE
    const uint32_t nowMs = GetHAL().millis();
    if (_lastFrameMs != 0 && nowMs - _lastFrameMs < kFrameIntervalMs) return;
    _lastFrameMs = nowMs;
    const uint32_t renderStartedMs = GetHAL().millis();
    _renderer.renderExplicitBaseline(_flightModel.state(), _terrain);
    const uint32_t renderTimeMs = GetHAL().millis() - renderStartedMs;
    _renderTimeTotalMs += renderTimeMs;
    _renderTimeMaxMs = std::max(_renderTimeMaxMs, renderTimeMs);
    ++_renderedFrames;

    const uint32_t performanceElapsedMs = GetHAL().millis() - _performanceWindowStartedMs;
    if (performanceElapsedMs >= kPerformanceWindowMs && _renderedFrames > 0) {
        const uint32_t fpsTenths = static_cast<uint32_t>(_renderedFrames) * 10000u / performanceElapsedMs;
        const uint32_t averageRenderMs = _renderTimeTotalMs / _renderedFrames;
        mclog::tagInfo(getAppInfo().name, "M4 static fps={}.{} render={}ms max={}ms",
                       fpsTenths / 10, fpsTenths % 10, averageRenderMs, _renderTimeMaxMs);
        _performanceWindowStartedMs = GetHAL().millis();
        _renderTimeTotalMs = 0;
        _renderTimeMaxMs = 0;
        _renderedFrames = 0;
    }
    return;
#else
    const uint32_t nowMs = GetHAL().millis();
    if (_lastSimulationMs == 0) _lastSimulationMs = nowMs;
    const uint32_t elapsedMs = nowMs - _lastSimulationMs;
    _lastSimulationMs = nowMs;
    _simulationAccumulator += static_cast<float>(elapsedMs) / 1000.0f;

    auto flightInput = _inputProvider ? _inputProvider->sample(nowMs) : vector_canyon_fighter::FlightInput{};
    const float calibProgress = _inputProvider ? _inputProvider->calibrationProgress(nowMs) : 0.0f;

    // ── Calibration phase: wait for IMU to settle before (re)starting ──────
    if (_calibrationPhase) {
        _simulationAccumulator = 0.0f;
        if (_lastFrameMs == 0 || nowMs - _lastFrameMs >= kFrameIntervalMs) {
            _lastFrameMs = nowMs;
            _renderer.render(_flightModel.state(), _terrain, _collisionStatus, calibProgress);
        }
        if (_inputProvider && _inputProvider->isCalibrated()) {
            _flightModel.reset();
            _terrain.reset(kTerrainSeed);
            _collisionStatus = {};
            _calibrationPhase = false;
            _lastSimulationMs = GetHAL().millis();
        }
        return;
    }

    // ── Collision + restart intent → enter calibration ──────────────────────
    if (_flightModel.state().collided && flightInput.pausePressed) {
        if (_inputProvider) _inputProvider->startCalibration(nowMs);
        _calibrationPhase = true;
        flightInput.pausePressed = false;
        return;
    }

    // ── Normal simulation ────────────────────────────────────────────────────
    int simulatedSteps = 0;
    while (_simulationAccumulator >= kSimulationStepSeconds && simulatedSteps < kMaxSimulationSteps) {
        _flightModel.step(flightInput, kSimulationStepSeconds);
        _simulationAccumulator -= kSimulationStepSeconds;
        ++simulatedSteps;
    }
    if (simulatedSteps == kMaxSimulationSteps && _simulationAccumulator >= kSimulationStepSeconds) {
        _simulationAccumulator = 0.0f;
        ++_simulationClampCount;
    }
    _terrain.update(_flightModel.state().forwardDistance);
    _collisionStatus = _collisionModel.evaluate(_flightModel.state(), _terrain);
    _flightModel.setCollided(_collisionStatus.collided);

    if (_lastFrameMs != 0 && nowMs - _lastFrameMs < kFrameIntervalMs) return;
    _lastFrameMs = nowMs;
    const uint32_t renderStartedMs = GetHAL().millis();
    _renderer.render(_flightModel.state(), _terrain, _collisionStatus, -1.0f);
    const uint32_t renderTimeMs = GetHAL().millis() - renderStartedMs;
    _renderTimeTotalMs += renderTimeMs;
    _renderTimeMaxMs = std::max(_renderTimeMaxMs, renderTimeMs);
    ++_renderedFrames;
    if (_flightModel.state().boostAmount > 0.8f) ++_boostedFrames;

    const uint32_t performanceElapsedMs = GetHAL().millis() - _performanceWindowStartedMs;
    if (performanceElapsedMs >= kPerformanceWindowMs && _renderedFrames > 0) {
        const uint32_t fpsTenths = static_cast<uint32_t>(_renderedFrames) * 10000u / performanceElapsedMs;
        const uint32_t averageRenderMs = _renderTimeTotalMs / _renderedFrames;
        mclog::tagInfo(getAppInfo().name, "perf fps={}.{} render={}ms max={}ms boost={}/{} clamps={}",
                       fpsTenths / 10, fpsTenths % 10, averageRenderMs, _renderTimeMaxMs, _boostedFrames,
                       _renderedFrames, _simulationClampCount);
        _performanceWindowStartedMs = GetHAL().millis();
        _renderTimeTotalMs = 0;
        _renderTimeMaxMs = 0;
        _renderedFrames = 0;
        _boostedFrames = 0;
        _simulationClampCount = 0;
    }
#endif
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
