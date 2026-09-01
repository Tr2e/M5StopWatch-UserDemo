#pragma once

#include "input_provider.h"

namespace vector_canyon_fighter {

class ImuButtonInputProvider final : public InputProvider {
public:
    void open() override;
    FlightInput sample(uint32_t nowMs) override;
    void close() override;

    void startCalibration(uint32_t nowMs);
    float calibrationProgress(uint32_t nowMs) const;
    bool isCalibrated() const { return _calibrated; }

private:
    float _neutralAccelX = 0.0f;
    float _neutralAccelY = 0.0f;
    float _filteredSteer = 0.0f;
    float _filteredPitch = 0.0f;
    float _throttle = 0.62f;
    uint32_t _sequence = 0;
    uint32_t _openedAtMs = 0;
    uint32_t _calibStartMs = 0;
    float _accumX = 0.0f;
    float _accumY = 0.0f;
    int _accumCount = 0;
    bool _calibrated = false;
    bool _pauseLatched = false;
};

}  // namespace vector_canyon_fighter
