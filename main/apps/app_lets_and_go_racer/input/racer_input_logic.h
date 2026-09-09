#pragma once

#include "racer_input.h"

#include <algorithm>
#include <cmath>

namespace lets_and_go {

struct RawRacerInput {
    float steer = 0.0f;
    float viewAxis = 0.0f;
    bool axesValid = false;
    bool actionsValid = false;
    bool redClicked = false;
    bool blueClicked = false;
    bool redHeld = false;
    bool blueHeld = false;
    bool redHoldStarted = false;
    bool chordStarted = false;
};

inline RacerInput mapRacerInput(const RawRacerInput& raw, uint32_t sequence)
{
    RacerInput result;
    result.sequence = sequence;
    // Axis and action devices are independent. A Joystick2 calibration or
    // disconnect must not erase a valid Dual Button edge on a selection page.
    result.exitPressed = raw.actionsValid && raw.chordStarted;
    result.menuBlocked = raw.actionsValid && raw.redHeld && raw.blueHeld;
    result.valid = raw.axesValid && raw.actionsValid && std::isfinite(raw.steer);
    if (result.valid) {
        result.steer = std::clamp(raw.steer, -1.0f, 1.0f);
    }
    if (!raw.actionsValid || (raw.redHeld && raw.blueHeld)) return result;
    if (result.valid) {
        result.viewAxis = std::isfinite(raw.viewAxis)
                              ? std::clamp(raw.viewAxis, -1.0f, 1.0f)
                              : 0.0f;
    }
    result.confirmPressed = raw.blueClicked;
    result.cancelPressed = raw.redClicked;
    result.brakeHeld = raw.redHeld;
    result.boostHeld = raw.blueHeld;
    result.pausePressed = raw.redHoldStarted;
    return result;
}

// Single-consumer mailbox. The hardware adapter holds its mutex around both
// operations. Continuous state is latest-wins; short edges survive slow frames
// and are consumed once, even if press and release happen during rendering.
class RacerInputMailbox {
public:
    void publish(const RacerInput& input)
    {
        RacerInput next = input;
        next.confirmPressed |= _latest.confirmPressed;
        next.cancelPressed |= _latest.cancelPressed;
        next.pausePressed |= _latest.pausePressed;
        next.exitPressed |= _latest.exitPressed;
        if (!next.navigationStep) next.navigationStep = _latest.navigationStep;
        if (!next.viewStep) next.viewStep = _latest.viewStep;
        if (next.menuBlocked || next.exitPressed) {
            next.confirmPressed = false;
            next.cancelPressed = false;
            next.pausePressed = false;
            next.navigationStep = next.viewStep = 0;
        }
        _latest = next;
    }

    RacerInput consume()
    {
        const RacerInput result = _latest;
        _latest.confirmPressed = false;
        _latest.cancelPressed = false;
        _latest.pausePressed = false;
        _latest.exitPressed = false;
        _latest.navigationStep = _latest.viewStep = 0;
        return result;
    }

    void reset() { _latest = {}; }

private:
    RacerInput _latest;
};

class MenuAxisRepeater {
public:
    int update(float axis, uint32_t nowMs)
    {
        if (!std::isfinite(axis)) axis = 0.0f;
        constexpr float kPressThreshold = 0.58f;
        constexpr float kReleaseThreshold = 0.30f;
        const int direction = axis >= kPressThreshold ? 1
                              : axis <= -kPressThreshold ? -1
                                                        : 0;
        if (_activeDirection != 0 && std::abs(axis) <= kReleaseThreshold) {
            _activeDirection = 0;
            _nextRepeatMs = 0;
            return 0;
        }
        if (direction == 0) return 0;
        if (direction != _activeDirection) {
            _activeDirection = direction;
            _nextRepeatMs = nowMs + 360u;
            return direction;
        }
        if (static_cast<int32_t>(nowMs - _nextRepeatMs) >= 0) {
            _nextRepeatMs = nowMs + 160u;
            return direction;
        }
        return 0;
    }

    void reset()
    {
        _activeDirection = 0;
        _nextRepeatMs = 0;
    }

private:
    int _activeDirection = 0;
    uint32_t _nextRepeatMs = 0;
};

// Lock the dominant axis until the stick returns to centre. Diagonal holds
// cannot move both the car cursor and the view preset, even with sensor noise.
class GarageMenuNavigation {
public:
    struct Step { int car=0,view=0; };
    Step update(float x,float y,uint32_t nowMs) {
        if(!std::isfinite(x) || !std::isfinite(y)) {reset();return {};}
        if(std::abs(x)<=.30f && std::abs(y)<=.30f) {reset();return {};}
        if(_axis==0 && std::max(std::abs(x),std::abs(y))>=.58f)
            _axis=std::abs(x)>=std::abs(y) ? 1 : 2;
        if(_axis==1)return {_repeat.update(x,nowMs),0};
        if(_axis==2)return {0,_repeat.update(y,nowMs)};
        return {};
    }
    void reset() {_axis=0;_repeat.reset();}
private:
    int _axis=0;
    MenuAxisRepeater _repeat;
};

class RacerMenuEvents {
public:
    void setMode(RacerNavigationMode mode) {
        if (_mode == mode) return;
        _mode = mode;
        resetGesture();
    }
    void resetGesture() { _armed = false; _navigation.reset(); }
    GarageMenuNavigation::Step update(const RacerInput& input, uint32_t nowMs) {
        if (_mode == RacerNavigationMode::None || !input.valid || input.menuBlocked) {
            resetGesture(); return {};
        }
        const float y = (_mode == RacerNavigationMode::Garage || _mode == RacerNavigationMode::Results)
                            ? -input.viewAxis : 0.f;
        if (std::abs(input.steer) <= .30f && std::abs(y) <= .30f) _armed = true;
        if (!_armed) return {};
        auto step = _navigation.update(input.steer, y, nowMs);
        // Results are vertical. Retain horizontal navigation, with the same
        // dominant-axis lock so a diagonal gesture never skips two rows.
        if (_mode == RacerNavigationMode::Results) {
            step.car = step.car ? step.car : step.view;
            step.view = 0;
        }
        return step;
    }
private:
    GarageMenuNavigation _navigation;
    RacerNavigationMode _mode = RacerNavigationMode::None;
    bool _armed = false;
};

// Production hardware input context. Screen changes discard ordinary queued
// actions but retain exit. A new gesture starts only after presentation and
// release/centre; continuous driving and independent emergency exit stay live.
class RacerScreenInput {
public:
    void changeScreen(RacerNavigationMode mode) {
        const bool exit = _mailbox.consume().exitPressed;
        _mailbox.reset();
        RacerInput pending;
        pending.exitPressed = exit;
        _mailbox.publish(pending);
        _menu.setMode(mode);
        _menu.resetGesture(); // Rival and course pages share Horizontal mode.
        _presented = _buttonsArmed = false;
    }
    void presentScreen() { _presented = true; }
    void publish(const RawRacerInput& raw, uint32_t sequence, uint32_t nowMs) {
        auto input = mapRacerInput(raw, sequence);
        if (!_presented || !_buttonsArmed || !raw.actionsValid) {
            input.confirmPressed = input.cancelPressed = input.pausePressed = false;
            // Discard the release edge which arms the next gesture.
            _buttonsArmed = _presented && raw.actionsValid && !raw.redHeld && !raw.blueHeld;
        }
        if (_presented) {
            const auto step = _menu.update(input, nowMs);
            input.navigationStep = step.car;
            input.viewStep = step.view;
        }
        _mailbox.publish(input);
    }
    RacerInput consume() { return _mailbox.consume(); }
    void reset() { *this = {}; }
private:
    RacerInputMailbox _mailbox;
    RacerMenuEvents _menu;
    bool _presented = false;
    bool _buttonsArmed = false;
};

class LongChordDetector {
public:
    bool update(bool firstHeld, bool secondHeld, uint32_t nowMs,
                uint32_t holdMs = 800u)
    {
        const bool bothHeld = firstHeld && secondHeld;
        if (!bothHeld) {
            reset();
            return false;
        }
        if (!_timing) {
            _startedMs = nowMs;
            _timing = true;
            return false;
        }
        if (!_latched && nowMs - _startedMs >= holdMs) {
            _latched = true;
            return true;
        }
        return false;
    }

    void reset()
    {
        _startedMs = 0;
        _timing = false;
        _latched = false;
    }

private:
    uint32_t _startedMs = 0;
    bool _timing = false;
    bool _latched = false;
};

}  // namespace lets_and_go
