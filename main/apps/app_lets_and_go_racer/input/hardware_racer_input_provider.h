#pragma once

#include "racer_input_logic.h"

#include "../../app_vector_canyon_fighter/input/dual_button_action_source.h"
#include "../../app_vector_canyon_fighter/input/joystick2_axis_source.h"
#include <atomic>
#include <mutex>

namespace lets_and_go {

class HardwareRacerInputProvider final : public RacerInputProvider {
public:
    ~HardwareRacerInputProvider() override;
    void open() override;
    RacerInput sample(uint32_t nowMs) override;
    RacerInputStatus status(uint32_t nowMs) const override;
    void requestCalibration(uint32_t nowMs) override;
    void setNavigationMode(RacerNavigationMode mode) override;
    void close() override;

private:
    void samplingTask();
    void poll(uint32_t nowMs);
    vector_canyon_fighter::Joystick2AxisSource _axes;
    vector_canyon_fighter::DualButtonActionSource _actions;
    uint32_t _sequence = 0;
    uint32_t _lastDiagnosticMs = 0;
    LongChordDetector _exitChord;
    mutable std::mutex _mutex;
    RacerInputMailbox _mailbox;
    RacerMenuEvents _menuEvents;
    RacerNavigationMode _navigationMode = RacerNavigationMode::None;
    std::atomic<bool> _sampling{false};
    std::atomic<bool> _samplingExited{true};
    bool _opened = false;
};

}  // namespace lets_and_go
