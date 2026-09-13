#include "external_input.h"

#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mooncake_log.h>

#include <algorithm>

namespace lucky_wheel {

ExternalInput::~ExternalInput()
{
    close();
}

void ExternalInput::open()
{
    if (_opened) return;
    _joystick.open();
    _buttons.open();
    _navigation.reset();
    _selectionPage.store(true, std::memory_order_release);
    _pageGeneration.store(0, std::memory_order_release);
    _sampledGeneration = 0;
    _pendingSteps.store(0, std::memory_order_release);
    _pendingConfirmGeneration.store(0, std::memory_order_release);
    _pendingExit.store(false, std::memory_order_release);
    _readiness.store(vector_canyon_fighter::InputReadiness::Disconnected,
                     std::memory_order_release);
    _redWasHeld = false;
    _blueArmed = false;
    _opened = true;

    _sampling.store(true, std::memory_order_release);
    _taskExited.store(false, std::memory_order_release);
    const BaseType_t created = xTaskCreatePinnedToCore(
        taskEntry, "wheel_input", 4096, this, 2, nullptr, 1);
    if (created != pdPASS) {
        _sampling.store(false, std::memory_order_release);
        _taskExited.store(true, std::memory_order_release);
        mclog::tagError("LuckyWheel", "external input task unavailable; using frame polling");
    }
}

void ExternalInput::changeToWheel()
{
    _selectionPage.store(false, std::memory_order_release);
    _pageGeneration.fetch_add(1, std::memory_order_acq_rel);
    _pendingSteps.store(0, std::memory_order_release);
    _pendingConfirmGeneration.store(0, std::memory_order_release);
    // Red exits from either page, including during the transition.
}

ExternalInputEvents ExternalInput::consume()
{
    ExternalInputEvents events;
    if (!_opened) return events;
    if (!_sampling.load(std::memory_order_acquire)) {
        poll(static_cast<uint32_t>(esp_timer_get_time() / 1000));
    }
    events.exit = _pendingExit.exchange(false, std::memory_order_acq_rel);
    events.selectionSteps = _pendingSteps.exchange(0, std::memory_order_acq_rel);
    events.confirm = _pendingConfirmGeneration.exchange(0, std::memory_order_acq_rel) ==
        _pageGeneration.load(std::memory_order_acquire) + 1;
    return events;
}

vector_canyon_fighter::InputReadiness ExternalInput::readiness() const
{
    return _readiness.load(std::memory_order_acquire);
}

void ExternalInput::taskEntry(void* context)
{
    static_cast<ExternalInput*>(context)->samplingTask();
}

void ExternalInput::samplingTask()
{
    TickType_t wake = xTaskGetTickCount();
    while (_sampling.load(std::memory_order_acquire)) {
        poll(static_cast<uint32_t>(esp_timer_get_time() / 1000));
        // A slow frame cannot hide a complete button press or joystick flick.
        if (xTaskGetTickCount() - wake >= pdMS_TO_TICKS(10)) wake = xTaskGetTickCount();
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(10));
    }
    _taskExited.store(true, std::memory_order_release);
    vTaskDelete(nullptr);
}

void ExternalInput::poll(uint32_t nowMs)
{
    using namespace vector_canyon_fighter;

    // GPIO is sampled first, and neither this call nor sampleAxes performs I²C
    // on this task. The Joystick2 driver's separate task owns the bus.
    const auto buttons = _buttons.sampleActions(nowMs);
    const bool redHeld = buttons.valid &&
        buttons.actions.isHeld(FlightAction::ThrottleDown);
    const bool blueHeld = buttons.valid &&
        buttons.actions.isHeld(FlightAction::ThrottleUp);
    if (redHeld && !_redWasHeld) _pendingExit.store(true, std::memory_order_release);
    _redWasHeld = redHeld;

    const uint32_t generation = _pageGeneration.load(std::memory_order_acquire);
    if (generation != _sampledGeneration) {
        _sampledGeneration = generation;
        _navigation.reset();
        _blueArmed = false;
    }

    if (!blueHeld && !buttons.actions.wasPressed(FlightAction::ThrottleUp)) {
        _blueArmed = true;
    } else if (buttons.valid && _blueArmed && !redHeld &&
               buttons.actions.wasPressed(FlightAction::ThrottleUp)) {
        if (generation == _pageGeneration.load(std::memory_order_acquire)) {
            _pendingConfirmGeneration.store(generation + 1, std::memory_order_release);
        }
        _blueArmed = false;
    }

    const auto axes = _joystick.sampleAxes(nowMs);
    _readiness.store(_joystick.axisStatus(nowMs).readiness, std::memory_order_release);
    if (!_selectionPage.load(std::memory_order_acquire) ||
        generation != _pageGeneration.load(std::memory_order_acquire)) {
        return;
    }
    // The roller is vertical: pushing up selects a smaller number.
    const auto step = _navigation.update(-axes.pitch, axes.valid, nowMs);
    if (step == launcher_input::NavigationStep::None) return;
    const int delta = step == launcher_input::NavigationStep::Next ? 1 : -1;
    int pending = _pendingSteps.load(std::memory_order_relaxed);
    while (!_pendingSteps.compare_exchange_weak(
        pending, std::clamp(pending + delta, -4, 4),
        std::memory_order_acq_rel, std::memory_order_relaxed)) {}
}

void ExternalInput::close()
{
    if (!_opened) return;
    _sampling.store(false, std::memory_order_release);
    while (!_taskExited.load(std::memory_order_acquire)) vTaskDelay(1);
    _buttons.close();
    _joystick.close();
    _pendingSteps.store(0, std::memory_order_release);
    _pendingConfirmGeneration.store(0, std::memory_order_release);
    _pendingExit.store(false, std::memory_order_release);
    _opened = false;
}

}  // namespace lucky_wheel
