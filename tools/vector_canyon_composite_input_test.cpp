#include "../main/apps/app_vector_canyon_fighter/input/composite_input_provider.h"

#include <iostream>
#include <memory>

namespace {

using namespace vector_canyon_fighter;

bool check(bool condition, const char* message)
{
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}

class FakeAxes final : public FlightAxisProvider {
public:
    void open() override { opened = true; }
    FlightAxisSample sampleAxes(uint32_t) override { return sample; }
    FlightAxisStatus axisStatus(uint32_t) const override
    {
        FlightAxisStatus result = status;
        result.connected = opened && result.connected;
        return result;
    }
    void requestAxisCalibration(uint32_t) override { calibrationRequested = true; }
    void close() override { opened = false; }

    FlightAxisSample sample{0.4f, -0.3f, 0.7f, true};
    FlightAxisStatus status{FlightAxisSource::Joystick2, InputReadiness::Ready,
                            true, true, 1.0f, 100u, 0u};
    bool opened = false;
    bool calibrationRequested = false;
};

class FakeActions final : public FlightActionProvider {
public:
    void open() override { opened = true; }
    FlightActionSample sampleActions(uint32_t) override { return sample; }
    FlightActionStatus actionStatus(uint32_t) const override
    {
        FlightActionStatus result = status;
        result.connected = opened && result.connected;
        return result;
    }
    void close() override { opened = false; }

    FlightActionSample sample;
    FlightActionStatus status{FlightActionSource::DualButton,
                              InputReadiness::Ready, true, 100u, 0u};
    bool opened = false;
};

bool validateCompositionAndFaultIsolation()
{
    auto axes = std::make_unique<FakeAxes>();
    auto actions = std::make_unique<FakeActions>();
    FakeAxes* axesView = axes.get();
    FakeActions* actionsView = actions.get();
    actionsView->sample.valid = true;
    actionsView->sample.actions.setHeld(FlightAction::Boost, true);
    actionsView->sample.actions.setPressed(FlightAction::ToggleImmersive);
    CompositeInputProvider provider(std::move(axes), std::move(actions));
    provider.open();

    const FlightInput ready = provider.sample(100u);
    const InputStatus readyStatus = provider.status(100u);
    bool valid = check(ready.valid && ready.steer == 0.4f &&
                           ready.pitch == -0.3f && ready.throttle == 0.7f,
                       "P3 composite provider changed normalized joystick axes");
    valid &= check(ready.actions.isHeld(FlightAction::Boost) &&
                       ready.actions.wasPressed(FlightAction::ToggleImmersive),
                   "P3 composite provider lost Dual Button actions");
    valid &= check(readyStatus.readiness == InputReadiness::Ready &&
                       readyStatus.axisSource == FlightAxisSource::Joystick2 &&
                       readyStatus.actionSource == FlightActionSource::DualButton,
                   "P3 composite provider lost independent source identity");

    actionsView->sample.valid = false;
    actionsView->status.connected = false;
    actionsView->status.readiness = InputReadiness::Disconnected;
    const FlightInput noButtons = provider.sample(133u);
    const InputStatus degraded = provider.status(133u);
    valid &= check(noButtons.valid && noButtons.actions.pressed == 0u &&
                       noButtons.actions.held == 0u,
                   "P3 button loss invalidated healthy axes or retained stale actions");
    valid &= check(degraded.readiness == InputReadiness::Degraded &&
                       degraded.axesConnected && !degraded.actionsConnected &&
                       degraded.isReady(),
                   "P3 action-only disconnect did not enter playable degraded mode");

    axesView->sample.valid = false;
    axesView->status.connected = false;
    axesView->status.readiness = InputReadiness::Disconnected;
    const FlightInput noAxes = provider.sample(166u);
    const InputStatus disconnected = provider.status(166u);
    valid &= check(!noAxes.valid && noAxes.steer == 0.0f &&
                       noAxes.pitch == 0.0f && noAxes.throttle == 0.62f,
                   "P3 axis loss did not publish safe neutral input");
    valid &= check(disconnected.readiness == InputReadiness::Disconnected &&
                       !disconnected.isReady(),
                   "P3 axis disconnect was incorrectly reported as playable");

    provider.requestCalibration(200u);
    valid &= check(axesView->calibrationRequested,
                   "P3 calibration was not routed exclusively to the axis source");
    provider.close();
    return valid;
}

}  // namespace

int main()
{
    return validateCompositionAndFaultIsolation() ? 0 : 1;
}
