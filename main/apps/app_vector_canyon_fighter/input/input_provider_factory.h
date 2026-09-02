#pragma once

#include "input_provider.h"
#include "imu_button_input_provider.h"

#include <memory>

namespace vector_canyon_fighter {

inline std::unique_ptr<InputProvider> makeDefaultFlightInputProvider()
{
    return std::make_unique<ImuButtonInputProvider>();
}

}  // namespace vector_canyon_fighter
