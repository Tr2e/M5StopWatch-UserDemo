#pragma once

#include "flight_input_sources.h"
#include "input_provider.h"

#include <memory>

namespace vector_canyon_fighter {

class CompositeInputProvider final : public InputProvider {
public:
    CompositeInputProvider(std::unique_ptr<FlightAxisProvider> axes,
                           std::unique_ptr<FlightActionProvider> actions);
    ~CompositeInputProvider() override;

    void open() override;
    FlightInput sample(uint32_t nowMs) override;
    InputStatus status(uint32_t nowMs) const override;
    void requestCalibration(uint32_t nowMs) override;
    void close() override;

private:
    std::unique_ptr<FlightAxisProvider> _axes;
    std::unique_ptr<FlightActionProvider> _actions;
    uint32_t _sequence = 0;
    float _throttle = 0.62f;
    bool _throttleOverridden = false;
    bool _opened = false;
};

}  // namespace vector_canyon_fighter
