#pragma once

#include <cstdint>

namespace lets_and_go {

enum class RacerNavigationMode : uint8_t { None, Horizontal, Garage, Results };

enum class RacerInputReadiness : uint8_t {
    Disconnected,
    Calibrating,
    Ready,
    Fault,
};

struct RacerInput {
    float steer = 0.0f;
    float viewAxis = 0.0f; // Joystick Y, garage only; never affects driving.
    bool confirmPressed = false;
    bool cancelPressed = false;
    bool brakeHeld = false;
    bool boostHeld = false;
    bool pausePressed = false;
    bool exitPressed = false;
    bool valid = false;
    bool menuBlocked = false;
    int8_t navigationStep = 0;
    int8_t viewStep = 0;
    uint32_t sequence = 0;
};

struct RacerInputStatus {
    RacerInputReadiness readiness = RacerInputReadiness::Disconnected;
    bool axesConnected = false;
    bool actionsConfigured = false;
    float calibrationProgress = 0.0f;
    uint32_t lastValidSampleMs = 0;
    uint16_t consecutiveErrors = 0;

    constexpr bool ready() const
    {
        return readiness == RacerInputReadiness::Ready && axesConnected &&
               actionsConfigured;
    }
};

class RacerInputProvider {
public:
    virtual ~RacerInputProvider() = default;
    virtual void open() = 0;
    virtual RacerInput sample(uint32_t nowMs) = 0;
    virtual RacerInputStatus status(uint32_t nowMs) const = 0;
    virtual void requestCalibration(uint32_t nowMs) = 0;
    virtual void setNavigationMode(RacerNavigationMode) {}
    virtual void presentScreen() {}
    virtual void close() = 0;
};

static_assert(sizeof(RacerInput) <= 24u,
              "racer input frame exceeded its fixed sample budget");
static_assert(sizeof(RacerInputStatus) <= 20u,
              "racer status exceeded its fixed snapshot budget");

}  // namespace lets_and_go
