#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace vector_canyon_fighter {

namespace joystick2 {

constexpr uint8_t kDefaultAddress = 0x63;
constexpr uint8_t kOffsetRegister = 0x50;
constexpr uint8_t kButtonRegister = 0x20;
constexpr uint8_t kRgbRegister = 0x30;
constexpr uint8_t kFirmwareVersionRegister = 0xfe;

struct RgbColor {
    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;

    constexpr uint32_t packed() const
    {
        return (static_cast<uint32_t>(red) << 16u) |
               (static_cast<uint32_t>(green) << 8u) |
               static_cast<uint32_t>(blue);
    }
};

enum class LedFeedbackState : uint8_t {
    Calibrating,
    Ready,
    Fault,
};

inline uint8_t ledChannel(float value)
{
    return static_cast<uint8_t>(std::lround(std::clamp(value, 0.0f, 96.0f)));
}

inline RgbColor feedbackColor(float steer, float pitch,
                              LedFeedbackState state, uint32_t nowMs)
{
    if (state == LedFeedbackState::Fault) return {64, 0, 0};
    if (state == LedFeedbackState::Calibrating) {
        constexpr uint32_t halfPeriodMs = 400u;
        const uint32_t phase = nowMs % (halfPeriodMs * 2u);
        const float pulse = phase <= halfPeriodMs
                                ? static_cast<float>(phase) / halfPeriodMs
                                : static_cast<float>(halfPeriodMs * 2u - phase) /
                                      halfPeriodMs;
        return {ledChannel(18.0f + 30.0f * pulse),
                ledChannel(7.0f + 17.0f * pulse), 0};
    }

    const float horizontal = std::clamp(std::abs(steer), 0.0f, 1.0f);
    const float vertical = std::clamp(std::abs(pitch), 0.0f, 1.0f);
    if (horizontal < 0.025f && vertical < 0.025f) return {0, 6, 14};

    float red = 0.0f;
    float green = 0.0f;
    float blue = 0.0f;
    if (steer < 0.0f) {
        red += 64.0f * horizontal;
        blue += 80.0f * horizontal;
    } else {
        green += 42.0f * horizontal;
        blue += 80.0f * horizontal;
    }
    if (pitch < 0.0f) {
        green += 72.0f * vertical;
        blue += 8.0f * vertical;
    } else {
        red += 80.0f * vertical;
        green += 28.0f * vertical;
    }
    return {ledChannel(red), ledChannel(green), ledChannel(blue)};
}

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
    bool pressed = false;
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
        result.pressed = _stablePressed;
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
