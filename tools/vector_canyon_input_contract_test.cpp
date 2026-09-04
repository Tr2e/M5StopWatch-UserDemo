#include "../main/apps/app_vector_canyon_fighter/input/input_provider.h"
#include "../main/apps/app_vector_canyon_fighter/model/flight_model.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <type_traits>

namespace {

using namespace vector_canyon_fighter;

bool check(bool condition, const char* message)
{
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}

class FakeInputProvider final : public InputProvider {
public:
    explicit FakeInputProvider(InputStatus configuredStatus)
        : _status(configuredStatus)
    {
    }

    void open() override { _open = true; }

    FlightInput sample(uint32_t nowMs) override
    {
        FlightInput result = _next;
        result.valid = _open && _status.isReady();
        result.sequence = ++_sequence;
        _next.actions.clearPressed();
        if (result.valid) _status.lastValidSampleMs = nowMs;
        return result;
    }

    InputStatus status(uint32_t) const override
    {
        InputStatus result = _status;
        if (!_open) {
            result.readiness = InputReadiness::Disconnected;
            result.axesConnected = false;
            result.actionsConnected = false;
        }
        return result;
    }

    void requestCalibration(uint32_t) override
    {
        if (!_status.calibrationSupported) return;
        _status.readiness = InputReadiness::Calibrating;
        _status.calibrationProgress = 0.0f;
    }

    void close() override { _open = false; }

    FlightInput& next() { return _next; }

private:
    InputStatus _status;
    FlightInput _next;
    uint32_t _sequence = 0;
    bool _open = false;
};

InputStatus readyStatus(FlightAxisSource axes, FlightActionSource actions)
{
    InputStatus status;
    status.axisSource = axes;
    status.actionSource = actions;
    status.readiness = InputReadiness::Ready;
    status.axesConnected = axes != FlightAxisSource::None;
    status.actionsConnected = actions != FlightActionSource::None;
    status.calibrationProgress = 1.0f;
    return status;
}

bool validateActionEdgesAndHolds()
{
    FlightActions actions;
    actions.setPressed(FlightAction::ToggleImmersive);
    actions.setPressed(FlightAction::Reset);
    actions.setHeld(FlightAction::Boost, true);
    bool valid = check(actions.wasPressed(FlightAction::ToggleImmersive) &&
                           actions.wasPressed(FlightAction::Reset),
                       "P0 independent pressed actions were not retained");
    valid &= check(actions.isHeld(FlightAction::Boost),
                   "P0 held boost state was not retained");
    actions.clearPressed();
    valid &= check(!actions.wasPressed(FlightAction::ToggleImmersive) &&
                       !actions.wasPressed(FlightAction::Reset) &&
                       actions.isHeld(FlightAction::Boost),
                   "P0 clearing edge actions also cleared continuous holds");
    return valid;
}

bool validateIndependentDeviceSources()
{
    FakeInputProvider provider(
        readyStatus(FlightAxisSource::Joystick2, FlightActionSource::DualButton));
    provider.open();
    provider.next().steer = 0.65f;
    provider.next().pitch = -0.25f;
    provider.next().actions.setPressed(FlightAction::ToggleImmersive);
    provider.next().actions.setHeld(FlightAction::Boost, true);

    const FlightInput first = provider.sample(100u);
    const FlightInput second = provider.sample(133u);
    const InputStatus status = provider.status(133u);
    bool valid = check(first.valid && first.steer == 0.65f && first.pitch == -0.25f,
                       "P0 provider did not publish normalized continuous axes");
    valid &= check(first.actions.wasPressed(FlightAction::ToggleImmersive) &&
                       !second.actions.wasPressed(FlightAction::ToggleImmersive),
                   "P0 provider edge action was not a one-sample event");
    valid &= check(first.actions.isHeld(FlightAction::Boost) &&
                       second.actions.isHeld(FlightAction::Boost),
                   "P0 held action did not survive across samples");
    valid &= check(status.axisSource == FlightAxisSource::Joystick2 &&
                       status.actionSource == FlightActionSource::DualButton &&
                       status.axesConnected && status.actionsConnected &&
                       status.lastValidSampleMs == 133u,
                   "P0 axis and action device status cannot be reported independently");
    provider.close();
    valid &= check(provider.status(166u).readiness == InputReadiness::Disconnected,
                   "P0 closed provider did not report a disconnected state");
    return valid;
}

bool validateContinuousModelContract()
{
    FlightModel model;
    model.reset();
    FlightInput input;
    input.valid = true;
    input.steer = 0.5f;
    input.pitch = -0.4f;
    input.actions.setPressed(FlightAction::Pause);
    input.actions.setHeld(FlightAction::Boost, true);
    for (int step = 0; step < 60; ++step) model.step(input, 1.0f / 60.0f);

    bool valid = check(!model.state().paused,
                       "P0 FlightModel consumed an app-level edge action repeatedly");
    valid &= check(model.state().lateralOffset > 0.0f && model.state().altitude < 0.5f,
                   "P0 normalized axes no longer drive the generic flight model");
    valid &= check(model.state().boostAmount > 0.95f,
                   "P0 held boost state no longer drives continuous thrust");

    model.togglePaused();
    const float pausedDistance = model.state().forwardDistance;
    model.step(input, 1.0f / 60.0f);
    valid &= check(model.state().paused &&
                       std::abs(model.state().forwardDistance - pausedDistance) < 0.0001f,
                   "P0 app-level pause command did not freeze simulation");
    return valid;
}

}  // namespace

int main()
{
    static_assert(std::is_abstract_v<InputProvider>,
                  "P0 provider contract must remain an abstract hardware boundary");
    static_assert(sizeof(FlightInput) <= 24,
                  "P0 control frame exceeded its fixed per-sample budget");
    static_assert(sizeof(InputStatus) <= 24,
                  "P0 status snapshot exceeded its fixed per-provider budget");

    bool valid = validateActionEdgesAndHolds();
    valid &= validateIndependentDeviceSources();
    valid &= validateContinuousModelContract();
    return valid ? 0 : 1;
}
