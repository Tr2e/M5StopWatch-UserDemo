#pragma once

#include "../vector_canyon_config.h"
#include "composite_input_provider.h"
#include "dual_button_action_source.h"
#include "input_provider.h"
#include "imu_button_input_provider.h"
#include "joystick2_axis_source.h"

#include <memory>

namespace vector_canyon_fighter {

inline std::unique_ptr<InputProvider> makeDefaultFlightInputProvider()
{
#if VECTOR_CANYON_USE_EXTERNAL_INPUT
    return std::make_unique<CompositeInputProvider>(
        std::make_unique<Joystick2AxisSource>(),
        std::make_unique<DualButtonActionSource>());
#else
    return std::make_unique<ImuButtonInputProvider>();
#endif
}

}  // namespace vector_canyon_fighter
