#pragma once

#include <cmath>
#include <cstdint>

namespace launcher_input {

enum class NavigationStep : int8_t {
    None = 0,
    Previous = -1,
    Next = 1,
};

class AxisNavigationRepeater {
public:
    NavigationStep update(float steer, bool valid, uint32_t nowMs)
    {
        constexpr float engageThreshold = 0.58f;
        constexpr float releaseThreshold = 0.30f;
        constexpr uint32_t firstRepeatDelayMs = 480u;
        constexpr uint32_t repeatPeriodMs = 180u;

        if (!valid) {
            reset();
            return NavigationStep::None;
        }

        if (std::abs(steer) <= releaseThreshold) {
            reset();
            return NavigationStep::None;
        }

        const int8_t direction = steer >= engageThreshold
                                     ? 1
                                     : (steer <= -engageThreshold ? -1 : 0);
        if (direction == 0) return NavigationStep::None;

        if (direction != _heldDirection) {
            _heldDirection = direction;
            _nextRepeatMs = nowMs + firstRepeatDelayMs;
            return direction > 0 ? NavigationStep::Next
                                 : NavigationStep::Previous;
        }

        if (nowMs < _nextRepeatMs) return NavigationStep::None;
        _nextRepeatMs = nowMs + repeatPeriodMs;
        return direction > 0 ? NavigationStep::Next
                             : NavigationStep::Previous;
    }

    void reset()
    {
        _heldDirection = 0;
        _nextRepeatMs = 0;
    }

private:
    int8_t _heldDirection = 0;
    uint32_t _nextRepeatMs = 0;
};

}  // namespace launcher_input
