#include "imu_button_input_provider.h"

#include <algorithm>
#include <cmath>
#include <hal/hal.h>

namespace vector_canyon_fighter {
namespace {

constexpr float kDeadZone = 0.08f;
constexpr float kAxisSensitivity = 0.85f;
constexpr float kFilterStrength = 0.18f;
constexpr uint32_t kCalibrationDelayMs = 650;

float normalizeAxis(float value)
{
    if (std::fabs(value) <= kDeadZone) return 0.0f;
    const float signedValue = value < 0.0f ? -1.0f : 1.0f;
    const float scaled = (std::fabs(value) - kDeadZone) * kAxisSensitivity;
    return signedValue * std::min(scaled, 1.0f);
}

}  // namespace

void ImuButtonInputProvider::open()
{
    _neutralAccelX = 0.0f;
    _neutralAccelY = 0.0f;
    _filteredSteer = 0.0f;
    _filteredPitch = 0.0f;
    _throttle = 0.62f;
    _sequence = 0;
    _openedAtMs = GetHAL().millis();
    _calibrated = false;
    _boostLatched = false;
}

FlightInput ImuButtonInputProvider::sample(uint32_t nowMs)
{
    GetHAL().updateImuData();
    const auto& imu = GetHAL().getImuData();

    if (!_calibrated && nowMs - _openedAtMs >= kCalibrationDelayMs) {
        _neutralAccelX = imu.accelX;
        _neutralAccelY = imu.accelY;
        _calibrated = true;
    }

    if (GetHAL().btnA.wasClicked()) _throttle = std::max(0.0f, _throttle - 0.08f);
    if (GetHAL().btnB.wasClicked()) _throttle = std::min(1.0f, _throttle + 0.08f);

    FlightInput input;
    input.throttle = _throttle;
    input.valid = _calibrated;
    input.sequence = ++_sequence;
    if (!_calibrated) return input;

    const float steer = normalizeAxis(imu.accelX - _neutralAccelX);
    const float pitch = normalizeAxis(imu.accelY - _neutralAccelY);
    _filteredSteer += kFilterStrength * (steer - _filteredSteer);
    _filteredPitch += kFilterStrength * (pitch - _filteredPitch);
    input.steer = _filteredSteer;
    input.pitch = _filteredPitch;
    if (!GetHAL().btnB.isHolding()) {
        _boostLatched = false;
    } else if (!_boostLatched) {
        input.boostPressed = true;
        _boostLatched = true;
    }
    return input;
}

void ImuButtonInputProvider::close()
{
    _calibrated = false;
}

}  // namespace vector_canyon_fighter
