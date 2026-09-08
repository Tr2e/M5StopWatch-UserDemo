#pragma once

#include "racer_input.h"

#include <algorithm>
#include <cmath>

namespace lets_and_go {

struct RawRacerInput {
    float steer = 0.0f;
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
    result.valid = raw.axesValid && raw.actionsValid && std::isfinite(raw.steer);
    if (!result.valid) return result;  // Fail neutral: never replay steer/buttons.
    result.steer = std::clamp(raw.steer, -1.0f, 1.0f);
    result.confirmPressed = raw.blueClicked;
    result.cancelPressed = raw.redClicked;
    result.brakeHeld = raw.redHeld;
    result.boostHeld = raw.blueHeld;
    result.pausePressed = raw.redHoldStarted;
    result.exitPressed = raw.chordStarted;
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
