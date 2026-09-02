#include "app_vector_canyon_fighter.h"

#include <hal/hal.h>
#include <mooncake_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <algorithm>

#include "input/flight_input.h"
#include "input/input_provider_factory.h"
#include "explicit_canyon_preview_schedule.h"

namespace {
constexpr uint32_t kFrameIntervalMs = 33;
constexpr uint32_t kPerformanceWindowMs = 5000;
constexpr uint32_t kTerrainSeed = 0xC4A71001u;
#if !(VECTOR_CANYON_EXPLICIT_TERRAIN && VECTOR_CANYON_EXPLICIT_PREVIEW)
constexpr float kSimulationStepSeconds = 1.0f / 60.0f;
constexpr int kMaxSimulationSteps = 5;
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
#if VECTOR_CANYON_EXPLICIT_TERRAIN && VECTOR_CANYON_EXPLICIT_PREVIEW
    _inputProvider.reset();
#else
    _inputProvider = vector_canyon_fighter::makeDefaultFlightInputProvider();
    _inputProvider->open();
#endif
    GetHAL().stopLvglUpdate();

    const auto& display = GetHAL().getDisplay();
    _renderer.open(display.width(), display.height());
    _flightModel.reset();
#if VECTOR_CANYON_EXPLICIT_TERRAIN && VECTOR_CANYON_EXPLICIT_PREVIEW
#if VECTOR_CANYON_EXPLICIT_STATIC_BASELINE
    _terrain.resetStraightBaseline();
#elif VECTOR_CANYON_EXPLICIT_EVENT_STREAM
    _terrain.reset(kTerrainSeed);
#else
    _terrain.resetCurvedBaseline(kTerrainSeed);
#endif
#else
    _terrain.reset(kTerrainSeed);
#endif
    _collisionStatus = {};
#if VECTOR_CANYON_EXPLICIT_TERRAIN && VECTOR_CANYON_EXPLICIT_PREVIEW
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
    _previewForwardDistance = 0.0f;
    _previewStartedMs = GetHAL().millis();
}

void AppVectorCanyonFighter::onRunning()
{
    GetHAL().updateButtonStates();
    if (_keys && _keys->update(false) == input::KeyEvent::GoHome) {
        close();
        return;
    }

#if VECTOR_CANYON_EXPLICIT_TERRAIN && VECTOR_CANYON_EXPLICIT_PREVIEW
    const uint32_t nowMs = GetHAL().millis();
#if VECTOR_CANYON_EXPLICIT_EVENT_STREAM
    if (_lastSimulationMs == 0) _lastSimulationMs = nowMs;
    const uint32_t rawElapsedMs = nowMs - _lastSimulationMs;
    _lastSimulationMs = nowMs;
    const uint32_t elapsedMs = std::min(rawElapsedMs, static_cast<uint32_t>(250));
    if (rawElapsedMs > elapsedMs) ++_simulationClampCount;
    const uint32_t previewElapsedMs = nowMs - _previewStartedMs;
    const bool previewBoosted = vector_canyon_fighter::isExplicitCanyonPreviewBoosted(previewElapsedMs);
    const float previewSpeed = vector_canyon_fighter::explicitCanyonPreviewSpeed(previewElapsedMs);
    _previewForwardDistance += previewSpeed * static_cast<float>(elapsedMs) * 0.001f;
    _terrain.update(_previewForwardDistance);
#else
    constexpr bool previewBoosted = false;
#endif
    if (_lastFrameMs != 0 && nowMs - _lastFrameMs < kFrameIntervalMs) return;
    _lastFrameMs = nowMs;
    const uint32_t renderStartedMs = GetHAL().millis();
    _renderer.renderExplicitPreview(_flightModel.state(), _terrain);
    const uint32_t renderTimeMs = GetHAL().millis() - renderStartedMs;
    _renderTimeTotalMs += renderTimeMs;
    _renderTimeMaxMs = std::max(_renderTimeMaxMs, renderTimeMs);
    ++_renderedFrames;
    if (previewBoosted) ++_boostedFrames;

    const uint32_t performanceElapsedMs = GetHAL().millis() - _performanceWindowStartedMs;
    if (performanceElapsedMs >= kPerformanceWindowMs && _renderedFrames > 0) {
        const uint32_t fpsTenths = static_cast<uint32_t>(_renderedFrames) * 10000u / performanceElapsedMs;
        const uint32_t averageRenderMs = _renderTimeTotalMs / _renderedFrames;
        const uint32_t stackWatermark = static_cast<uint32_t>(uxTaskGetStackHighWaterMark(nullptr));
        mclog::tagInfo(getAppInfo().name,
                       "M6 stream fps={}.{} render={}ms max={}ms boost={}/{} clamps={} first={} events={} stack={}",
                       fpsTenths / 10, fpsTenths % 10, averageRenderMs, _renderTimeMaxMs,
                       _boostedFrames, _renderedFrames, _simulationClampCount, _terrain.firstSegment(),
                       _terrain.eventWindow().count, stackWatermark);
        _performanceWindowStartedMs = GetHAL().millis();
        _renderTimeTotalMs = 0;
        _renderTimeMaxMs = 0;
        _renderedFrames = 0;
        _boostedFrames = 0;
        _simulationClampCount = 0;
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
            _lastFrameMs = 0;
            _performanceWindowStartedMs = _lastSimulationMs;
            _renderTimeTotalMs = 0;
            _renderTimeMaxMs = 0;
            _renderedFrames = 0;
            _boostedFrames = 0;
            _simulationClampCount = 0;
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
        const uint32_t stackWatermark = static_cast<uint32_t>(uxTaskGetStackHighWaterMark(nullptr));
        mclog::tagInfo(getAppInfo().name, "M7 game fps={}.{} render={}ms max={}ms boost={}/{} clamps={} stack={}",
                       fpsTenths / 10, fpsTenths % 10, averageRenderMs, _renderTimeMaxMs, _boostedFrames,
                       _renderedFrames, _simulationClampCount, stackWatermark);
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
