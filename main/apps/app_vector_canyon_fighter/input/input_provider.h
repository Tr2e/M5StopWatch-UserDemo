#pragma once

#include "flight_input.h"

namespace vector_canyon_fighter {

class InputProvider {
public:
    virtual ~InputProvider() = default;
    virtual void open() = 0;
    virtual FlightInput sample(uint32_t nowMs) = 0;
    virtual InputStatus status(uint32_t nowMs) const = 0;
    virtual void requestCalibration(uint32_t nowMs) = 0;
    virtual void close() = 0;
};

}  // namespace vector_canyon_fighter
