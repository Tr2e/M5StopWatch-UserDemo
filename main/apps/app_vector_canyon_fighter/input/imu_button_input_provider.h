#pragma once

#include "input_provider.h"

namespace vector_canyon_fighter {

class ImuButtonInputProvider final : public InputProvider {
public:
    void open() override;
    FlightInput sample(uint32_t nowMs) override;
    void close() override;

private:
    float _neutralAccelX = 0.0f;
    float _neutralAccelY = 0.0f;
    float _filteredSteer = 0.0f;
    float _filteredPitch = 0.0f;
    float _throttle = 0.62f;
    uint32_t _sequence = 0;
    uint32_t _openedAtMs = 0;
    bool _calibrated = false;
    bool _boostLatched = false;
};

}  // namespace vector_canyon_fighter
