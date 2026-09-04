#include "composite_input_provider.h"

#include <algorithm>

namespace vector_canyon_fighter {

CompositeInputProvider::CompositeInputProvider(
    std::unique_ptr<FlightAxisProvider> axes,
    std::unique_ptr<FlightActionProvider> actions)
    : _axes(std::move(axes)), _actions(std::move(actions))
{
}

CompositeInputProvider::~CompositeInputProvider()
{
    close();
}

void CompositeInputProvider::open()
{
    if (_opened) return;
    if (_axes) _axes->open();
    if (_actions) _actions->open();
    _sequence = 0;
    _throttle = 0.62f;
    _throttleOverridden = false;
    _opened = true;
}

FlightInput CompositeInputProvider::sample(uint32_t nowMs)
{
    const FlightAxisSample axes = _axes ? _axes->sampleAxes(nowMs)
                                         : FlightAxisSample{};
    const FlightActionSample actions = _actions
        ? _actions->sampleActions(nowMs)
        : FlightActionSample{};

    FlightInput result;
    result.sequence = ++_sequence;
    result.valid = _opened && axes.valid;
    if (result.valid) {
        result.steer = std::clamp(axes.steer, -1.0f, 1.0f);
        result.pitch = std::clamp(axes.pitch, -1.0f, 1.0f);
        if (!_throttleOverridden) {
            _throttle = std::clamp(axes.throttle, 0.0f, 1.0f);
        }
    }
    // An action-source fault must not invalidate healthy flight axes. It
    // safely produces no button state and is reported as Degraded in status().
    if (_opened && actions.valid) {
        result.actions = actions.actions;
        if (actions.actions.wasPressed(FlightAction::ThrottleDown)) {
            _throttle = std::max(0.0f, _throttle - 0.08f);
            _throttleOverridden = true;
        }
        if (actions.actions.wasPressed(FlightAction::ThrottleUp)) {
            _throttle = std::min(1.0f, _throttle + 0.08f);
            _throttleOverridden = true;
        }
    }
    if (result.valid) result.throttle = _throttle;
    return result;
}

InputStatus CompositeInputProvider::status(uint32_t nowMs) const
{
    const FlightAxisStatus axes = _axes ? _axes->axisStatus(nowMs)
                                         : FlightAxisStatus{};
    const FlightActionStatus actions = _actions
        ? _actions->actionStatus(nowMs)
        : FlightActionStatus{};

    InputStatus result;
    result.axisSource = axes.source;
    result.actionSource = actions.source;
    result.axesConnected = _opened && axes.connected;
    result.actionsConnected = _opened && actions.connected;
    result.calibrationSupported = axes.calibrationSupported;
    result.calibrationProgress = axes.calibrationProgress;
    result.lastValidSampleMs = axes.lastValidSampleMs;
    result.consecutiveErrors = std::max(
        axes.consecutiveErrors, actions.consecutiveErrors);
    if (!_opened || !axes.connected) {
        result.readiness = InputReadiness::Disconnected;
    } else if (axes.readiness == InputReadiness::Calibrating) {
        result.readiness = InputReadiness::Calibrating;
    } else if (!axes.isReady()) {
        result.readiness = InputReadiness::Fault;
    } else if (!actions.isReady()) {
        result.readiness = InputReadiness::Degraded;
    } else {
        result.readiness = InputReadiness::Ready;
    }
    return result;
}

void CompositeInputProvider::requestCalibration(uint32_t nowMs)
{
    if (_axes) _axes->requestAxisCalibration(nowMs);
}

void CompositeInputProvider::close()
{
    if (!_opened) return;
    if (_actions) _actions->close();
    if (_axes) _axes->close();
    _opened = false;
}

}  // namespace vector_canyon_fighter
