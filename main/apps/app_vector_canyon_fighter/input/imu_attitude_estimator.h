#pragma once

#include <algorithm>
#include <cmath>

namespace vector_canyon_fighter {

struct ImuMotionSample {
    float accelX = 0.0f;
    float accelY = 0.0f;
    float accelZ = 0.0f;
    float gyroX = 0.0f;
    float gyroY = 0.0f;
    float gyroZ = 0.0f;
};

struct ImuAttitude {
    float steerDegrees = 0.0f;
    float pitchDegrees = 0.0f;
    float accelerationMagnitude = 0.0f;
};

inline bool isUsableImuSample(const ImuMotionSample& sample)
{
    const bool finite = std::isfinite(sample.accelX) &&
                        std::isfinite(sample.accelY) &&
                        std::isfinite(sample.accelZ) &&
                        std::isfinite(sample.gyroX) &&
                        std::isfinite(sample.gyroY) &&
                        std::isfinite(sample.gyroZ);
    if (!finite) return false;
    const float magnitudeSquared = sample.accelX * sample.accelX +
                                   sample.accelY * sample.accelY +
                                   sample.accelZ * sample.accelZ;
    return magnitudeSquared >= 0.25f * 0.25f &&
           magnitudeSquared <= 2.50f * 2.50f;
}

inline ImuAttitude solveImuGravityAttitude(const ImuMotionSample& sample)
{
    constexpr float kRadiansToDegrees = 57.2957795131f;
    const float accelerationMagnitude =
        std::sqrt(sample.accelX * sample.accelX +
                  sample.accelY * sample.accelY +
                  sample.accelZ * sample.accelZ);
    if (!std::isfinite(accelerationMagnitude) || accelerationMagnitude < 0.0001f) {
        return {};
    }

    // HAL has already mapped the BMI270 axes to display coordinates. accelX
    // therefore preserves the existing left/right sign and accelY preserves
    // the existing climb/dive sign. atan2 removes the old cross-axis gain loss.
    const float steerDenominator =
        std::sqrt(sample.accelY * sample.accelY + sample.accelZ * sample.accelZ);
    const float pitchDenominator =
        std::sqrt(sample.accelX * sample.accelX + sample.accelZ * sample.accelZ);
    return {
        std::atan2(sample.accelX, steerDenominator) * kRadiansToDegrees,
        std::atan2(sample.accelY, pitchDenominator) * kRadiansToDegrees,
        accelerationMagnitude,
    };
}

inline float normalizeTiltControl(float degrees, float deadZoneDegrees,
                                  float fullScaleDegrees)
{
    const float magnitude = std::fabs(degrees);
    if (!std::isfinite(magnitude) || magnitude <= deadZoneDegrees ||
        fullScaleDegrees <= deadZoneDegrees) {
        return 0.0f;
    }
    const float linear = std::clamp(
        (magnitude - deadZoneDegrees) / (fullScaleDegrees - deadZoneDegrees),
        0.0f, 1.0f);
    // A restrained center lift makes 10-20 degree hand motion readable while
    // preserving a smooth slope into the full-scale clamp.
    const float shaped = linear * (1.25f - 0.25f * linear);
    return std::copysign(shaped, degrees);
}

class ImuAttitudeEstimator {
public:
    void reset(float steerDegrees, float pitchDegrees)
    {
        _attitude.steerDegrees = steerDegrees;
        _attitude.pitchDegrees = pitchDegrees;
        _initialized = true;
    }

    void clear()
    {
        _attitude = {};
        _initialized = false;
    }

    const ImuAttitude& update(const ImuMotionSample& sample, float deltaSeconds,
                              float gyroXBias, float gyroYBias)
    {
        const ImuAttitude gravity = solveImuGravityAttitude(sample);
        _attitude.accelerationMagnitude = gravity.accelerationMagnitude;
        if (!_initialized) reset(gravity.steerDegrees, gravity.pitchDegrees);

        const float dt = std::clamp(deltaSeconds, 0.0f, 0.10f);
        const float predictedSteer =
            _attitude.steerDegrees + (sample.gyroY - gyroYBias) * dt;
        const float predictedPitch =
            _attitude.pitchDegrees + (sample.gyroX - gyroXBias) * dt;

        // Trust gravity fully near 1 g, then progressively reject it while the
        // watch is being translated. Gyroscope prediction remains immediate.
        const float magnitudeError =
            std::fabs(gravity.accelerationMagnitude - 1.0f);
        const float gravityTrust = std::clamp(
            1.0f - (magnitudeError - 0.10f) / 0.25f, 0.0f, 1.0f);
        constexpr float kGravityCorrectionTimeSeconds = 0.35f;
        const float correction = gravityTrust *
            (1.0f - std::exp(-dt / kGravityCorrectionTimeSeconds));
        _attitude.steerDegrees =
            predictedSteer + (gravity.steerDegrees - predictedSteer) * correction;
        _attitude.pitchDegrees =
            predictedPitch + (gravity.pitchDegrees - predictedPitch) * correction;
        return _attitude;
    }

private:
    ImuAttitude _attitude;
    bool _initialized = false;
};

}  // namespace vector_canyon_fighter
