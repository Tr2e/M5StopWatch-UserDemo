#include "racer_model.h"

#include <algorithm>
#include <cmath>

namespace lets_and_go {

void RacerModel::reset(float distance, float lateralOffset, float speed)
{
    _state = {};
    _state.distance = std::isfinite(distance) ? distance : 0.0f;
    _state.lateralOffset = std::isfinite(lateralOffset) ? lateralOffset : 0.0f;
    _state.speed = std::max(0.0f, std::isfinite(speed) ? speed : 0.0f);
    _state.boostCharge = 1.0f;
}

void RacerModel::step(const RacerInput& input, const CarSpec& car,
                      const TrackFrame& track, float deltaSeconds,
                      float motorEfficiency)
{
    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f) return;
    const float dt = std::min(deltaSeconds, 0.05f);
    const float steer = input.valid && std::isfinite(input.steer)
                            ? std::clamp(input.steer, -1.0f, 1.0f)
                            : 0.0f;
    const bool braking = input.valid ? input.brakeHeld : true;
    const bool boosting = input.valid && input.boostHeld && !braking &&
                          _state.boostCharge > 0.02f;

    const float efficiency = std::isfinite(motorEfficiency)
                                 ? std::clamp(motorEfficiency, 0.75f, 1.20f)
                                 : 1.0f;
    const float maximumSpeed = (13.0f + car.performance.topSpeed * 8.0f) * efficiency;
    const float curveLoad = std::min(1.0f, std::abs(track.curvature) * 5.5f);
    const float curveLimit = maximumSpeed *
        (1.0f - curveLoad * (0.28f - car.performance.stability * 0.12f));
    const float boostMultiplier = boosting ? 1.13f : 1.0f;
    const float targetSpeed = braking ? 0.0f : curveLimit * boostMultiplier;
    const float acceleration = (4.0f + car.performance.acceleration * 4.5f) * efficiency;
    const float deceleration = braking ? 13.0f : 5.5f;
    const float speedRate = targetSpeed > _state.speed ? acceleration : deceleration;
    const float speedDelta = std::clamp(targetSpeed - _state.speed,
                                        -speedRate * dt, speedRate * dt);
    _state.speed = std::max(0.0f, _state.speed + speedDelta);

    if (boosting) {
        _state.boostCharge = std::max(0.0f, _state.boostCharge - dt * 0.24f);
    } else {
        _state.boostCharge = std::min(1.0f, _state.boostCharge + dt * 0.075f);
    }

    _state.steeringLockSeconds = std::max(0.0f, _state.steeringLockSeconds - dt);
    const float steeringAuthority = _state.steeringLockSeconds > 0.0f ? 0.32f : 1.0f;
    const float desiredLateralVelocity = steer * steeringAuthority *
        car.performance.steering * (1.6f + _state.speed * 0.085f);
    const float lateralResponse = std::min(1.0f, dt * (6.0f + car.performance.stability * 3.0f));
    _state.lateralVelocity +=
        (desiredLateralVelocity - _state.lateralVelocity) * lateralResponse;
    _state.lateralOffset += _state.lateralVelocity * dt;
    _state.headingOffset +=
        (steer * 0.20f - _state.headingOffset) * std::min(1.0f, dt * 8.0f);

    const float safeHalfWidth = std::max(0.1f, track.halfWidth - 0.29f);
    if (std::abs(_state.lateralOffset) > safeHalfWidth) {
        _state.lateralOffset = std::copysign(safeHalfWidth, _state.lateralOffset);
        _state.lateralVelocity *= -0.18f;
        const float retained = 0.58f + car.performance.stability * 0.16f;
        _state.speed *= retained;
        _state.wallImpact = 1.0f;
        _state.steeringLockSeconds = 0.16f;
    } else {
        _state.wallImpact = std::max(0.0f, _state.wallImpact - dt * 3.8f);
    }
    _state.distance += _state.speed * dt;
}

}  // namespace lets_and_go
