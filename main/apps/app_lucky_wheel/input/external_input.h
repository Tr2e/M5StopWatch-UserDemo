#pragma once

#include <apps/app_launcher/launcher_external_input_logic.h>
#include <apps/app_vector_canyon_fighter/input/dual_button_action_source.h>
#include <apps/app_vector_canyon_fighter/input/joystick2_axis_source.h>

#include <atomic>
#include <cstdint>

namespace lucky_wheel {

struct ExternalInputEvents {
    int selectionSteps = 0;
    bool confirm = false;
    bool exit = false;
};

// Owns the external controller while LuckyWheel is open. GPIO and menu
// navigation are sampled independently of LVGL's frame rate.
class ExternalInput {
public:
    ~ExternalInput();
    void open();
    void changeToWheel();
    ExternalInputEvents consume();
    vector_canyon_fighter::InputReadiness readiness() const;
    void close();

private:
    static void taskEntry(void* context);
    void samplingTask();
    void poll(uint32_t nowMs);

    vector_canyon_fighter::Joystick2AxisSource _joystick;
    vector_canyon_fighter::DualButtonActionSource _buttons;
    launcher_input::AxisNavigationRepeater _navigation;
    std::atomic<bool> _sampling{false};
    std::atomic<bool> _taskExited{true};
    std::atomic<bool> _selectionPage{true};
    std::atomic<uint32_t> _pageGeneration{0};
    std::atomic<int> _pendingSteps{0};
    // Zero means empty; otherwise this is the page generation plus one.
    std::atomic<uint32_t> _pendingConfirmGeneration{0};
    std::atomic<bool> _pendingExit{false};
    std::atomic<vector_canyon_fighter::InputReadiness> _readiness{
        vector_canyon_fighter::InputReadiness::Disconnected};
    uint32_t _sampledGeneration = 0;
    bool _redWasHeld = false;
    bool _blueArmed = false;
    bool _opened = false;
};

}  // namespace lucky_wheel
