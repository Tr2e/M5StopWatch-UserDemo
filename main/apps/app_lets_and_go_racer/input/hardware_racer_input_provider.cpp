#include "hardware_racer_input_provider.h"

#include "racer_input_logic.h"

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
    _exitChord.reset();
    _opened = true;
}

RacerInput HardwareRacerInputProvider::sample(uint32_t nowMs)
{
    using namespace vector_canyon_fighter;
    if (!_opened) return {};
    const FlightAxisSample axes = _axes.sampleAxes(nowMs);
    const FlightActionSample actions = _actions.sampleActions(nowMs);
    RawRacerInput raw;
    raw.steer = axes.steer;
    raw.axesValid = axes.valid;
    raw.actionsValid = actions.valid;
    raw.redClicked = actions.actions.wasPressed(FlightAction::ThrottleDown);
    raw.blueClicked = actions.actions.wasPressed(FlightAction::ThrottleUp);
    raw.redHeld = actions.actions.isHeld(FlightAction::ThrottleDown);
    raw.blueHeld = actions.actions.isHeld(FlightAction::ThrottleUp);
    raw.redHoldStarted = actions.actions.wasPressed(FlightAction::ToggleImmersive);
    raw.chordStarted = _exitChord.update(raw.redHeld, raw.blueHeld, nowMs);
    return mapRacerInput(raw, ++_sequence);
}

RacerInputStatus HardwareRacerInputProvider::status(uint32_t nowMs) const
{
    using namespace vector_canyon_fighter;
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
    _axes.requestAxisCalibration(nowMs);
}

void HardwareRacerInputProvider::close()
{
    if (!_opened) return;
    _actions.close();
    _axes.close();
    _exitChord.reset();
    _opened = false;
}

}  // namespace lets_and_go
