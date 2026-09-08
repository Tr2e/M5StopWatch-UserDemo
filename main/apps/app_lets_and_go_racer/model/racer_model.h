#pragma once

#include "car_catalog.h"
#include "track_types.h"
#include "../input/racer_input.h"

namespace lets_and_go {

struct RacerState {
    float distance = 0.0f;
    float lateralOffset = 0.0f;
    float lateralVelocity = 0.0f;
    float speed = 0.0f;
    float headingOffset = 0.0f;
    float boostCharge = 1.0f;
    float wallImpact = 0.0f;
    float steeringLockSeconds = 0.0f;
};

class RacerModel {
public:
    void reset(float distance = 0.0f, float lateralOffset = 0.0f,
               float speed = 0.0f);
    void step(const RacerInput& input, const CarSpec& car,
              const TrackFrame& track, float deltaSeconds);

    const RacerState& state() const { return _state; }
    RacerState& mutableState() { return _state; }

private:
    RacerState _state{};
};

static_assert(sizeof(RacerState) <= 32u,
              "racer dynamics state exceeded its fixed budget");

}  // namespace lets_and_go
