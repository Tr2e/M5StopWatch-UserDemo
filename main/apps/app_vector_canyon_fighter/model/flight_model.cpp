#include "flight_model.h"

#include <algorithm>

namespace vector_canyon_fighter {
namespace {

constexpr float kMinSpeed = 42.0f;
constexpr float kMaxSpeed = 132.0f;
constexpr float kResponse = 5.2f;
constexpr float kMaxLateralVelocity = 1.4f;
constexpr float kMaxVerticalVelocity = 1.1f;

float approach(float current, float target, float amount)
{
    if (current < target) return std::min(current + amount, target);
    return std::max(current - amount, target);
}

}  // namespace

void FlightModel::reset()
{
    _state = {};
    _state.speed = 72.0f;
    _lateralVelocity = 0.0f;
    _verticalVelocity = 0.0f;
}

void FlightModel::step(const FlightInput& input, float deltaSeconds)
{
    if (_state.collided) {
        if (input.pausePressed) reset();
        return;
    }
    if (input.pausePressed) _state.paused = !_state.paused;
    if (_state.paused) return;

    const float safeSteer = input.valid ? std::clamp(input.steer, -1.0f, 1.0f) : 0.0f;
    const float safePitch = input.valid ? std::clamp(input.pitch, -1.0f, 1.0f) : 0.0f;
    const float safeThrottle = input.valid ? std::clamp(input.throttle, 0.0f, 1.0f) : 0.55f;
    const float targetSpeed = kMinSpeed + (kMaxSpeed - kMinSpeed) * safeThrottle;
    const float response = kResponse * deltaSeconds;

    _state.speed = approach(_state.speed, targetSpeed, 42.0f * deltaSeconds);
    _lateralVelocity = approach(_lateralVelocity, safeSteer * kMaxLateralVelocity, response);
    _verticalVelocity = approach(_verticalVelocity, safePitch * kMaxVerticalVelocity, response);
    _state.lateralOffset = std::clamp(_state.lateralOffset + _lateralVelocity * deltaSeconds, -1.7f, 1.7f);
    _state.altitude = std::clamp(_state.altitude + _verticalVelocity * deltaSeconds, -1.25f, 1.35f);
    _state.heading += safeSteer * 24.0f * deltaSeconds;
    if (_state.heading < 0.0f) _state.heading += 360.0f;
    if (_state.heading >= 360.0f) _state.heading -= 360.0f;
    _state.roll = approach(_state.roll, -safeSteer * 18.0f, 58.0f * deltaSeconds);
    _state.pitch = approach(_state.pitch, safePitch * 12.0f, 38.0f * deltaSeconds);

    if (input.boostPressed) _state.boostAmount = 1.0f;
    _state.boostAmount = std::max(0.0f, _state.boostAmount - 1.8f * deltaSeconds);
    _state.forwardDistance += (_state.speed + 44.0f * _state.boostAmount) * deltaSeconds;
}

void FlightModel::setCollided(bool collided)
{
    if (collided) _state.collided = true;
}

}  // namespace vector_canyon_fighter
