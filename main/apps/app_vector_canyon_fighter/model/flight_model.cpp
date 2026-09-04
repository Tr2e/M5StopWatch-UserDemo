#include "flight_model.h"

#include <algorithm>

namespace vector_canyon_fighter {
namespace {

constexpr float kMinSpeed = 42.0f;
constexpr float kMaxSpeed = 132.0f;
constexpr float kResponse = 5.2f;
constexpr float kMaxLateralVelocity = 1.8f;
constexpr float kMaxVerticalVelocity = 1.1f;
constexpr float kMaximumVisualRoll = 30.0f;
constexpr float kMaximumVisualPitch = 18.0f;
constexpr float kMaximumVisualTurnYaw = 8.0f;

float approach(float current, float target, float amount)
{
    if (current < target) return std::min(current + amount, target);
    return std::max(current - amount, target);
}

void approachCriticallyDamped(float& value, float& velocity, float target,
                              float naturalFrequency, float deltaSeconds)
{
    const float dt = std::clamp(deltaSeconds, 0.0f, 0.05f);
    const float acceleration = naturalFrequency * naturalFrequency * (target - value) -
                               2.0f * naturalFrequency * velocity;
    velocity += acceleration * dt;
    value += velocity * dt;
}

}  // namespace

void FlightModel::reset()
{
    _state = {};
    _state.speed = 72.0f;
    _state.altitude = 0.5f;
    _lateralVelocity = 0.0f;
    _verticalVelocity = 0.0f;
    _rollRate = 0.0f;
    _pitchRate = 0.0f;
    _turnYawRate = 0.0f;
}

void FlightModel::step(const FlightInput& input, float deltaSeconds)
{
    if (_state.collided) return;
    if (_state.paused) return;

    const float safeSteer = input.valid ? std::clamp(input.steer, -1.0f, 1.0f) : 0.0f;
    const float safePitch = input.valid ? std::clamp(input.pitch, -1.0f, 1.0f) : 0.0f;
    const float safeThrottle = input.valid ? std::clamp(input.throttle, 0.0f, 1.0f) : 0.55f;
    const float targetSpeed = kMinSpeed + (kMaxSpeed - kMinSpeed) * safeThrottle;
    const float response = kResponse * deltaSeconds;

    _state.speed = approach(_state.speed, targetSpeed, 42.0f * deltaSeconds);
    _lateralVelocity = approach(_lateralVelocity, safeSteer * kMaxLateralVelocity, response);
    _verticalVelocity = approach(_verticalVelocity, safePitch * kMaxVerticalVelocity, response);
    _state.lateralOffset = std::clamp(_state.lateralOffset + _lateralVelocity * deltaSeconds, -3.2f, 3.2f);
    _state.altitude = std::clamp(_state.altitude + _verticalVelocity * deltaSeconds, -1.25f, 1.35f);
    _state.heading += safeSteer * 24.0f * deltaSeconds;
    if (_state.heading < 0.0f) _state.heading += 360.0f;
    if (_state.heading >= 360.0f) _state.heading -= 360.0f;
    // These are local aircraft pose channels, not camera rotations. Bank is
    // deliberately strongest, pitch remains readable in chase view, and the
    // restrained yaw exposes turn direction through perspective without
    // making the fighter look detached from the canyon course.
    approachCriticallyDamped(_state.roll, _rollRate,
                             -safeSteer * kMaximumVisualRoll, 10.0f, deltaSeconds);
    approachCriticallyDamped(_state.pitch, _pitchRate,
                             safePitch * kMaximumVisualPitch, 8.0f, deltaSeconds);
    approachCriticallyDamped(_state.turnYaw, _turnYawRate,
                             safeSteer * kMaximumVisualTurnYaw, 9.0f, deltaSeconds);

    const bool boostActive = input.actions.isHeld(FlightAction::Boost);
    const float boostTarget = boostActive ? 1.0f : 0.0f;
    const float boostResponse = (boostActive ? 5.5f : 1.8f) * deltaSeconds;
    _state.boostAmount = approach(_state.boostAmount, boostTarget, boostResponse);
    _state.forwardDistance += (_state.speed + 44.0f * _state.boostAmount) * deltaSeconds;
}

void FlightModel::togglePaused()
{
    if (!_state.collided) _state.paused = !_state.paused;
}

void FlightModel::setCollided(bool collided)
{
    if (collided) _state.collided = true;
}

}  // namespace vector_canyon_fighter
