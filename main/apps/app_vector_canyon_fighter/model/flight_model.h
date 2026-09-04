#pragma once

#include "../input/flight_input.h"

namespace vector_canyon_fighter {

struct FlightState {
    float forwardDistance = 0.0f;
    float lateralOffset = 0.0f;
    float altitude = 0.0f;
    float speed = 72.0f;
    float heading = 0.0f;
    float roll = 0.0f;
    float pitch = 0.0f;
    float turnYaw = 0.0f;
    float boostAmount = 0.0f;
    bool paused = false;
    bool collided = false;
};

class FlightModel {
public:
    void reset();
    void step(const FlightInput& input, float deltaSeconds);
    void togglePaused();
    void setCollided(bool collided);
    const FlightState& state() const { return _state; }

private:
    FlightState _state;
    float _lateralVelocity = 0.0f;
    float _verticalVelocity = 0.0f;
    float _rollRate = 0.0f;
    float _pitchRate = 0.0f;
    float _turnYawRate = 0.0f;
};

}  // namespace vector_canyon_fighter
