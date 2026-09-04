#pragma once

#include "flight_input.h"

namespace vector_canyon_fighter {

struct FlightAxisSample {
    float steer = 0.0f;
    float pitch = 0.0f;
    float throttle = 0.62f;
    bool valid = false;
};

struct FlightAxisStatus {
    FlightAxisSource source = FlightAxisSource::None;
    InputReadiness readiness = InputReadiness::Disconnected;
    bool connected = false;
    bool calibrationSupported = false;
    float calibrationProgress = 0.0f;
    uint32_t lastValidSampleMs = 0;
    uint16_t consecutiveErrors = 0;

    constexpr bool isReady() const
    {
        return connected &&
               (readiness == InputReadiness::Ready ||
                readiness == InputReadiness::Degraded);
    }
};

struct FlightActionSample {
    FlightActions actions;
    bool valid = false;
};

struct FlightActionStatus {
    FlightActionSource source = FlightActionSource::None;
    InputReadiness readiness = InputReadiness::Disconnected;
    bool connected = false;
    uint32_t lastValidSampleMs = 0;
    uint16_t consecutiveErrors = 0;

    constexpr bool isReady() const
    {
        return connected &&
               (readiness == InputReadiness::Ready ||
                readiness == InputReadiness::Degraded);
    }
};

class FlightAxisProvider {
public:
    virtual ~FlightAxisProvider() = default;
    virtual void open() = 0;
    virtual FlightAxisSample sampleAxes(uint32_t nowMs) = 0;
    virtual FlightAxisStatus axisStatus(uint32_t nowMs) const = 0;
    virtual void requestAxisCalibration(uint32_t nowMs) = 0;
    virtual void close() = 0;
};

class FlightActionProvider {
public:
    virtual ~FlightActionProvider() = default;
    virtual void open() = 0;
    virtual FlightActionSample sampleActions(uint32_t nowMs) = 0;
    virtual FlightActionStatus actionStatus(uint32_t nowMs) const = 0;
    virtual void close() = 0;
};

}  // namespace vector_canyon_fighter
