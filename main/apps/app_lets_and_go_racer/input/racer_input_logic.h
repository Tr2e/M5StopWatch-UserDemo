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
    // Exit remains available when the joystick is disconnected; button validity
    // is independent. A chord must not also pause, confirm or consume boost.
    result.exitPressed = raw.actionsValid && raw.chordStarted;
    result.menuBlocked = raw.actionsValid && raw.redHeld && raw.blueHeld;
    result.valid = raw.axesValid && raw.actionsValid && std::isfinite(raw.steer);
    if (!result.valid) return result;  // Fail neutral: never replay steer/buttons.
    result.steer = std::clamp(raw.steer, -1.0f, 1.0f);
    if (raw.redHeld && raw.blueHeld) return result;
    result.viewAxis = std::isfinite(raw.viewAxis) ? std::clamp(raw.viewAxis,-1.0f,1.0f) : 0.0f;
    result.confirmPressed = raw.blueClicked;
    result.cancelPressed = raw.redClicked;
    result.brakeHeld = raw.redHeld;
    result.boostHeld = raw.blueHeld;
    result.pausePressed = raw.redHoldStarted;
    return result;
}

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
