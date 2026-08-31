#pragma once

#include <cstdint>

namespace vector_canyon_fighter {

struct FlightInput {
    float steer = 0.0f;
    float pitch = 0.0f;
    float throttle = 0.62f;
    bool boostPressed = false;
    bool pausePressed = false;
    bool valid = true;
    uint32_t sequence = 0;
};

}  // namespace vector_canyon_fighter
