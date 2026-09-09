#pragma once

#include "racer_input_logic.h"
#include "../controller/game_flow.h"
#include "../../app_vector_canyon_fighter/input/external_input_logic.h"

namespace lets_and_go {

struct DeviceControlFrame {
    RacerInput input{};
    int navigation = 0;
    int view = 0;
};

// Caller serializes GPIO/touch publication and consumption. A screen change
// cancels queued commands and requires release before accepting another gesture.
class DeviceControlLogic {
public:
    void reset() { *this = {}; }
    void setScreen(GameScreen screen) {
        if (_screen == screen) return;
        _screen = screen;
        _frame = {};
        _buttonsArmed = false;
        _touchArmed = false;
    }
    void buttons(bool aDown, bool bDown, uint32_t now) {
        const auto a = _a.update(aDown, now);
        const auto b = _b.update(bDown, now);
        const bool chord = _exit.update(a.pressed, b.pressed, now);
        _frame.input.exitPressed |= chord;
        if (a.pressed && b.pressed) {
            _buttonsArmed = false;
            _frame.input.confirmPressed = _frame.input.cancelPressed = false;
            _frame.input.pausePressed = false;
            _frame.navigation = _frame.view = 0;
        }
        _frame.input.brakeHeld = _buttonsArmed && a.pressed && !b.pressed;
        _frame.input.boostHeld = _buttonsArmed && b.pressed && !a.pressed;
        if (_buttonsArmed) {
            if (_screen == GameScreen::Paused) {
                _frame.input.pausePressed |= b.clicked;
            } else if (_screen != GameScreen::Racing) {
                if (a.clicked) _frame.navigation = 1;
                _frame.input.confirmPressed |= b.clicked;
                _frame.input.cancelPressed |= a.holdStarted;
            }
        }
        if (!a.pressed && !b.pressed) _buttonsArmed = true;
    }
    void touch(bool down, int x, int y) {
        if (!down) {
            _touchDown = false;
            _touchArmed = true;
            _frame.input.steer = 0;
            return;
        }
        if (!_touchArmed) return;
        if (_screen == GameScreen::Racing) {
            if (!_touchDown) _steeringGesture = y >= 130;
            if (_steeringGesture)
                _frame.input.steer = std::clamp((x - 233) / 140.f, -1.f, 1.f);
            else if (!_touchDown) _frame.input.pausePressed = true;
        } else if (!_touchDown) {
            if (_screen == GameScreen::Paused) _frame.input.pausePressed = true;
            else if (_screen == GameScreen::InputCheck || _screen == GameScreen::InputCalibration)
                _frame.input.confirmPressed = true;
            else if (y < 110) _frame.input.cancelPressed = true;
            else if (_screen == GameScreen::CarSelect && y < 175) _frame.view = 1;
            else if (x < 155) _frame.navigation = -1;
            else if (x > 311) _frame.navigation = 1;
            else _frame.input.confirmPressed = true;
        }
        _touchDown = true;
    }
    DeviceControlFrame consume(bool healthy) {
        auto result = _frame;
        result.input.valid = healthy;
        if (!healthy) result.input.steer = 0;
        if (result.input.exitPressed) {
            result.input.confirmPressed = result.input.cancelPressed = false;
            result.input.pausePressed = result.input.brakeHeld = result.input.boostHeld = false;
            result.navigation = result.view = 0;
        }
        _frame.input.confirmPressed = _frame.input.cancelPressed = false;
        _frame.input.pausePressed = _frame.input.exitPressed = false;
        _frame.navigation = _frame.view = 0;
        return result;
    }
private:
    vector_canyon_fighter::DebouncedActiveLowButton _a, _b;
    LongChordDetector _exit;
    DeviceControlFrame _frame{};
    GameScreen _screen = GameScreen::InputCheck;
    bool _buttonsArmed = false, _touchArmed = false, _touchDown = false;
    bool _steeringGesture = false;
};
} // namespace lets_and_go
