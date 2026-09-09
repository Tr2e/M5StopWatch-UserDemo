#include "hardware_racer_input_provider.h"

#include "racer_input_logic.h"
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mooncake_log.h>

namespace lets_and_go {

HardwareRacerInputProvider::~HardwareRacerInputProvider()
{
    close();
}

void HardwareRacerInputProvider::open()
{
    if (_opened) return;
    _axes.open();
    _actions.open();
    _sequence = 0;
    _lastDiagnosticMs = 0;
    _exitChord.reset();
    _screenInput.reset();
    _navigationMode = RacerNavigationMode::None;
    _opened = true;
    _sampling.store(true, std::memory_order_release);
    _samplingExited.store(false, std::memory_order_release);
    const BaseType_t created = xTaskCreatePinnedToCore(
        [](void* context) {
            static_cast<HardwareRacerInputProvider*>(context)->samplingTask();
        }, "racer_input", 4096, this, 2, nullptr, 1);
    if (created != pdPASS) {
        _sampling.store(false, std::memory_order_release);
        _samplingExited.store(true, std::memory_order_release);
        mclog::tagError("Let's & Go!!", "input task unavailable; using frame polling");
    }
}

RacerInput HardwareRacerInputProvider::sample(uint32_t nowMs)
{
    if (!_opened) return {};
    std::lock_guard<std::mutex> lock(_mutex);
    if (!_sampling.load(std::memory_order_acquire)) poll(nowMs);
    return _screenInput.consume();
}

void HardwareRacerInputProvider::samplingTask()
{
    TickType_t wake = xTaskGetTickCount();
    while (_sampling.load(std::memory_order_acquire)) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            poll(static_cast<uint32_t>(esp_timer_get_time() / 1000));
        }
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(10));
    }
    _samplingExited.store(true, std::memory_order_release);
    vTaskDelete(nullptr);
}

void HardwareRacerInputProvider::poll(uint32_t nowMs)
{
    using namespace vector_canyon_fighter;
    const FlightAxisSample axes = _axes.sampleAxes(nowMs);
    const FlightActionSample actions = _actions.sampleActions(nowMs);
    RawRacerInput raw;
    raw.steer = axes.steer;
    raw.viewAxis = axes.pitch;
    raw.axesValid = axes.valid;
    raw.actionsValid = actions.valid;
    raw.redClicked = actions.actions.wasPressed(FlightAction::ThrottleDown);
    raw.blueClicked = actions.actions.wasPressed(FlightAction::ThrottleUp);
    raw.redHeld = actions.actions.isHeld(FlightAction::ThrottleDown);
    raw.blueHeld = actions.actions.isHeld(FlightAction::ThrottleUp);
    raw.redHoldStarted = actions.actions.wasPressed(FlightAction::ToggleImmersive);
    raw.chordStarted = _exitChord.update(raw.redHeld, raw.blueHeld, nowMs);
    _screenInput.publish(raw, ++_sequence, nowMs);
    if (_lastDiagnosticMs == 0u || nowMs - _lastDiagnosticMs >= 5000u) {
        _lastDiagnosticMs = nowMs;
        const auto status = _axes.axisStatus(nowMs);
        mclog::tagInfo("RacerInput", "seq={} axis={} ready={} errors={} x={} y={} buttons={} red={} blue={}",
                       _sequence, axes.valid, static_cast<int>(status.readiness),
                       status.consecutiveErrors, static_cast<int>(axes.steer * 100),
                       static_cast<int>(axes.pitch * 100), actions.valid,
                       raw.redHeld, raw.blueHeld);
    }
    if (raw.redClicked || raw.blueClicked || raw.chordStarted) {
        mclog::tagInfo("RacerInput", "redClick={} blueClick={} exit={}",
                       raw.redClicked, raw.blueClicked, raw.chordStarted);
    }
}

RacerInputStatus HardwareRacerInputProvider::status(uint32_t nowMs) const
{
    using namespace vector_canyon_fighter;
    std::lock_guard<std::mutex> lock(_mutex);
    const FlightAxisStatus axes = _axes.axisStatus(nowMs);
    const FlightActionStatus actions = _actions.actionStatus(nowMs);
    RacerInputStatus result;
    result.axesConnected = axes.connected;
    result.actionsConfigured = actions.connected;
    result.calibrationProgress = axes.calibrationProgress;
    result.lastValidSampleMs = axes.lastValidSampleMs;
    result.consecutiveErrors = axes.consecutiveErrors;
    if (axes.readiness == InputReadiness::Fault) {
        result.readiness = RacerInputReadiness::Fault;
    } else if (axes.readiness == InputReadiness::Calibrating) {
        result.readiness = RacerInputReadiness::Calibrating;
    } else if (axes.isReady() && actions.isReady()) {
        result.readiness = RacerInputReadiness::Ready;
    }
    return result;
}

void HardwareRacerInputProvider::requestCalibration(uint32_t nowMs)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _screenInput.changeScreen(_navigationMode);
    _axes.requestAxisCalibration(nowMs);
}

void HardwareRacerInputProvider::setNavigationMode(RacerNavigationMode mode)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _navigationMode = mode;
    _screenInput.changeScreen(mode);
}

void HardwareRacerInputProvider::presentScreen()
{
    std::lock_guard<std::mutex> lock(_mutex);
    _screenInput.presentScreen();
}

void HardwareRacerInputProvider::close()
{
    if (!_opened) return;
    _sampling.store(false, std::memory_order_release);
    while (!_samplingExited.load(std::memory_order_acquire)) {
        vTaskDelay(1);
    }
    _actions.close();
    _axes.close();
    _exitChord.reset();
    _screenInput.reset();
    _opened = false;
}

}  // namespace lets_and_go
