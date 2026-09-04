#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace vector_canyon_fighter {

namespace joystick2 {

constexpr uint8_t kDefaultAddress = 0x63;
constexpr uint8_t kOffsetRegister = 0x50;
constexpr uint8_t kFirmwareVersionRegister = 0xfe;

inline int16_t decodeSignedLittleEndian(const uint8_t* bytes)
{
    return static_cast<int16_t>(
        static_cast<uint16_t>(bytes[0]) |
        (static_cast<uint16_t>(bytes[1]) << 8u));
}

inline float normalizeOffset(int16_t value, int16_t neutral,
                             float deadZoneCounts = 120.0f,
                             float fullScaleCounts = 1900.0f)
{
    const float centered = static_cast<float>(value - neutral);
    const float magnitude = std::abs(centered);
    if (magnitude <= deadZoneCounts) return 0.0f;
    const float span = std::max(1.0f, fullScaleCounts - deadZoneCounts);
    const float linear = std::clamp((magnitude - deadZoneCounts) / span,
                                    0.0f, 1.0f);
    // A mild cubic blend keeps the center precise without making full travel
    // feel dull. The game only sees the normalized intent, never ADC counts.
    const float curved = linear * (0.68f + 0.32f * linear * linear);
    return centered < 0.0f ? -curved : curved;
}

}  // namespace joystick2

struct ButtonTransition {
    bool clicked = false;
    bool holdStarted = false;
    bool holding = false;
};

class DebouncedActiveLowButton {
public:
    ButtonTransition update(bool rawPressed, uint32_t nowMs,
                            uint32_t debounceMs = 20u,
                            uint32_t holdMs = 500u)
    {
        ButtonTransition result;
        if (!_initialized) {
            _initialized = true;
            _rawPressed = rawPressed;
            _stablePressed = rawPressed;
            _rawChangedMs = nowMs;
            _pressedMs = nowMs;
        }

        if (rawPressed != _rawPressed) {
            _rawPressed = rawPressed;
            _rawChangedMs = nowMs;
        }
        if (_rawPressed != _stablePressed &&
            nowMs - _rawChangedMs >= debounceMs) {
            _stablePressed = _rawPressed;
            if (_stablePressed) {
                _pressedMs = nowMs;
                _holdReported = false;
            } else {
                result.clicked = !_holdReported;
            }
        }

        if (_stablePressed && !_holdReported &&
            nowMs - _pressedMs >= holdMs) {
            _holdReported = true;
            result.holdStarted = true;
        }
        result.holding = _stablePressed && _holdReported;
        return result;
    }

    void reset()
    {
        _initialized = false;
        _rawPressed = false;
        _stablePressed = false;
        _holdReported = false;
        _rawChangedMs = 0;
        _pressedMs = 0;
    }

private:
    bool _initialized = false;
    bool _rawPressed = false;
    bool _stablePressed = false;
    bool _holdReported = false;
    uint32_t _rawChangedMs = 0;
    uint32_t _pressedMs = 0;
};

}  // namespace vector_canyon_fighter
