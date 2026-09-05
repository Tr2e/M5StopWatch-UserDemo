#pragma once

#include <cstdint>

namespace vector_canyon_fighter {

enum class FlightAction : uint16_t {
    Boost = 1u << 0,
    Pause = 1u << 1,
    Reset = 1u << 2,
    ToggleImmersive = 1u << 3,
    Recalibrate = 1u << 4,
    ThrottleUp = 1u << 5,
    ThrottleDown = 1u << 6,
};

constexpr uint16_t flightActionMask(FlightAction action)
{
    return static_cast<uint16_t>(action);
}

struct FlightActions {
    uint16_t pressed = 0;
    uint16_t held = 0;

    constexpr bool wasPressed(FlightAction action) const
    {
        return (pressed & flightActionMask(action)) != 0;
    }

    constexpr bool isHeld(FlightAction action) const
    {
        return (held & flightActionMask(action)) != 0;
    }

    void setPressed(FlightAction action)
    {
        pressed |= flightActionMask(action);
    }

    void setHeld(FlightAction action, bool active)
    {
        if (active) {
            held |= flightActionMask(action);
        } else {
            held &= static_cast<uint16_t>(~flightActionMask(action));
        }
    }

    void clearPressed()
    {
        pressed = 0;
    }
};

class TwoButtonFlightActionMapper {
public:
    FlightActions update(bool primaryPressed, bool secondaryPressed,
                         bool primaryClicked, bool secondaryClicked,
                         bool primaryHoldStarted, bool secondaryHolding)
    {
        FlightActions actions;
        if (primaryPressed && secondaryPressed && !_chordActive) {
            _chordActive = true;
            _suppressPrimaryClick = true;
            _suppressSecondaryClick = true;
            actions.setPressed(FlightAction::Pause);
        }
        if (primaryClicked && !_suppressPrimaryClick) {
            actions.setPressed(FlightAction::ThrottleDown);
        }
        if (secondaryClicked && !_suppressSecondaryClick) {
            actions.setPressed(FlightAction::ThrottleUp);
        }
        if (!primaryPressed) _suppressPrimaryClick = false;
        if (!secondaryPressed) _suppressSecondaryClick = false;
        if (!primaryPressed && !secondaryPressed) _chordActive = false;
        if (primaryHoldStarted) {
            actions.setPressed(FlightAction::Reset);
            actions.setPressed(FlightAction::ToggleImmersive);
        }
        actions.setHeld(FlightAction::Boost, secondaryHolding);
        return actions;
    }

    void reset()
    {
        _chordActive = false;
        _suppressPrimaryClick = false;
        _suppressSecondaryClick = false;
    }

private:
    bool _chordActive = false;
    bool _suppressPrimaryClick = false;
    bool _suppressSecondaryClick = false;
};

enum class FlightAxisSource : uint8_t {
    None,
    Imu,
    Joystick2,
};

enum class FlightActionSource : uint8_t {
    None,
    BodyButtons,
    DualButton,
};

enum class InputReadiness : uint8_t {
    Disconnected,
    Calibrating,
    Ready,
    Degraded,
    Fault,
};

struct InputStatus {
    FlightAxisSource axisSource = FlightAxisSource::None;
    FlightActionSource actionSource = FlightActionSource::None;
    InputReadiness readiness = InputReadiness::Disconnected;
    bool axesConnected = false;
    bool actionsConnected = false;
    bool calibrationSupported = false;
    float calibrationProgress = 0.0f;
    uint32_t lastValidSampleMs = 0;
    uint16_t consecutiveErrors = 0;

    constexpr bool isReady() const
    {
        return axesConnected &&
               (readiness == InputReadiness::Ready ||
                readiness == InputReadiness::Degraded);
    }
};

struct FlightInput {
    float steer = 0.0f;
    float pitch = 0.0f;
    float throttle = 0.62f;
    FlightActions actions;
    bool valid = true;
    uint32_t sequence = 0;
};

}  // namespace vector_canyon_fighter
