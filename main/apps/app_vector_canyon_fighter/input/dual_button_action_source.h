#pragma once

#include "external_input_logic.h"
#include "flight_input_sources.h"

namespace vector_canyon_fighter {

class DualButtonActionSource final : public FlightActionProvider {
public:
    void open() override;
    FlightActionSample sampleActions(uint32_t nowMs) override;
    FlightActionStatus actionStatus(uint32_t nowMs) const override;
    void close() override;

private:
    DebouncedActiveLowButton _redButton;
    DebouncedActiveLowButton _blueButton;
    uint32_t _lastSampleMs = 0;
    bool _opened = false;
};

}  // namespace vector_canyon_fighter
