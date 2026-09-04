#include "../main/apps/app_vector_canyon_fighter/input/imu_attitude_estimator.h"

#include <cmath>
#include <iostream>
#include <limits>

namespace {

using namespace vector_canyon_fighter;

bool check(bool condition, const char* message)
{
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}

bool near(float actual, float expected, float tolerance)
{
    return std::fabs(actual - expected) <= tolerance;
}

ImuMotionSample tiltedSteer(float degrees)
{
    constexpr float kDegreesToRadians = 0.0174532925199f;
    const float angle = degrees * kDegreesToRadians;
    ImuMotionSample sample;
    sample.accelX = std::sin(angle);
    sample.accelZ = std::cos(angle);
    return sample;
}

ImuMotionSample tiltedPitch(float degrees)
{
    constexpr float kDegreesToRadians = 0.0174532925199f;
    const float angle = degrees * kDegreesToRadians;
    ImuMotionSample sample;
    sample.accelY = std::sin(angle);
    sample.accelZ = std::cos(angle);
    return sample;
}

bool validateGravityAttitude()
{
    const ImuAttitude steer = solveImuGravityAttitude(tiltedSteer(20.0f));
    const ImuAttitude pitch = solveImuGravityAttitude(tiltedPitch(-15.0f));
    bool valid = check(near(steer.steerDegrees, 20.0f, 0.05f) &&
                           near(steer.pitchDegrees, 0.0f, 0.05f),
                       "P1 gravity attitude lost the established steer sign");
    valid &= check(near(pitch.pitchDegrees, -15.0f, 0.05f) &&
                       near(pitch.steerDegrees, 0.0f, 0.05f),
                   "P1 gravity attitude lost the established pitch sign");
    valid &= check(near(steer.accelerationMagnitude, 1.0f, 0.001f),
                   "P1 gravity attitude did not preserve acceleration magnitude");
    return valid;
}

bool validateInputCurve()
{
    const float center = normalizeTiltControl(3.0f, 3.0f, 28.0f);
    const float medium = normalizeTiltControl(15.0f, 3.0f, 28.0f);
    const float full = normalizeTiltControl(35.0f, 3.0f, 28.0f);
    const float negative = normalizeTiltControl(-15.0f, 3.0f, 28.0f);
    bool valid = check(center == 0.0f,
                       "P1 angular dead zone does not suppress neutral tremor");
    valid &= check(medium > 0.50f && medium < 0.65f,
                   "P1 mid-tilt response is outside the intended readable range");
    valid &= check(full == 1.0f,
                   "P1 full tilt does not clamp to the provider contract");
    valid &= check(near(negative, -medium, 0.0001f),
                   "P1 input curve is not symmetric around the calibrated center");
    return valid;
}

bool validateComplementaryFilter()
{
    ImuMotionSample neutral;
    neutral.accelZ = 1.0f;
    neutral.gyroX = 60.0f;
    neutral.gyroY = 90.0f;

    ImuAttitudeEstimator estimator;
    estimator.reset(0.0f, 0.0f);
    const ImuAttitude immediate = estimator.update(neutral, 0.033f, 0.0f, 0.0f);
    bool valid = check(immediate.steerDegrees > 2.5f &&
                           immediate.pitchDegrees > 1.6f,
                       "P1 gyro path does not provide immediate signed response");

    ImuMotionSample translated;
    translated.accelX = 1.06066f;
    translated.accelZ = 1.06066f;
    estimator.reset(10.0f, 0.0f);
    const ImuAttitude rejected = estimator.update(translated, 0.033f, 0.0f, 0.0f);
    valid &= check(near(rejected.steerDegrees, 10.0f, 0.05f),
                   "P1 filter treated strong linear acceleration as tilt");

    neutral.gyroX = 0.0f;
    neutral.gyroY = 0.0f;
    estimator.reset(15.0f, -12.0f);
    ImuAttitude settled;
    for (int i = 0; i < 120; ++i) {
        settled = estimator.update(neutral, 1.0f / 60.0f, 0.0f, 0.0f);
    }
    valid &= check(std::fabs(settled.steerDegrees) < 0.10f &&
                       std::fabs(settled.pitchDegrees) < 0.10f,
                   "P1 gravity reference does not remove long-term gyro drift");
    return valid;
}

bool validateSampleGuards()
{
    ImuMotionSample valid;
    valid.accelZ = 1.0f;
    ImuMotionSample empty;
    ImuMotionSample excessive;
    excessive.accelZ = 3.0f;
    ImuMotionSample notFinite = valid;
    notFinite.gyroX = std::numeric_limits<float>::quiet_NaN();
    return check(isUsableImuSample(valid) && !isUsableImuSample(empty) &&
                     !isUsableImuSample(excessive) && !isUsableImuSample(notFinite),
                 "P1 invalid or implausible sensor frames bypassed the guard");
}

}  // namespace

int main()
{
    bool valid = validateGravityAttitude();
    valid &= validateInputCurve();
    valid &= validateComplementaryFilter();
    valid &= validateSampleGuards();
    return valid ? 0 : 1;
}
