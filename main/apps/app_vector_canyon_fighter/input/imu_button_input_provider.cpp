#include "imu_button_input_provider.h"

#include <algorithm>
#include <cmath>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <hal/hal.h>
#include <mooncake_log.h>

namespace vector_canyon_fighter {
namespace {

constexpr float kSteerDeadZoneDegrees = 3.0f;
constexpr float kSteerFullScaleDegrees = 28.0f;
constexpr float kPitchDeadZoneDegrees = 3.0f;
constexpr float kPitchFullScaleDegrees = 26.0f;
constexpr float kOutputFilterStrength = 0.34f;
constexpr uint32_t kCalibrationDelayMs = 2500;
constexpr uint16_t kMinimumCalibrationSamples = 40;
constexpr float kMaximumCalibrationAngleDeviation = 1.5f;
constexpr float kMaximumCalibrationGyroDeviation = 4.0f;
constexpr uint32_t kImuSamplePeriodMs = 33;
constexpr uint32_t kImuStaleAfterMs = 300;
constexpr uint16_t kFaultAfterConsecutiveErrors = 3;
constexpr uint32_t kImuTaskStackBytes = 5 * 1024;

float standardDeviation(float sum, float squareSum, uint16_t count)
{
    if (count == 0) return 0.0f;
    const float mean = sum / static_cast<float>(count);
    const float variance = std::max(
        0.0f, squareSum / static_cast<float>(count) - mean * mean);
    return std::sqrt(variance);
}

ImuMotionSample readHalImuSample()
{
    const auto& imu = GetHAL().getImuData();
    return {
        imu.accelX,
        imu.accelY,
        imu.accelZ,
        imu.gyroX,
        imu.gyroY,
        imu.gyroZ,
    };
}

}  // namespace

ImuButtonInputProvider::~ImuButtonInputProvider()
{
    close();
}

void ImuButtonInputProvider::samplingTaskEntry(void* context)
{
    static_cast<ImuButtonInputProvider*>(context)->samplingTask();
}

void ImuButtonInputProvider::publishImuSample(const ImuMotionSample& sample)
{
    // The odd/even sequence is a small seqlock. It prevents the app task from
    // observing an acceleration vector from one IMU frame and gyro data from
    // the next frame without placing either task behind a mutex.
    _imuPublishSequence.fetch_add(1u, std::memory_order_acq_rel);
    _latestAccelX.store(sample.accelX, std::memory_order_relaxed);
    _latestAccelY.store(sample.accelY, std::memory_order_relaxed);
    _latestAccelZ.store(sample.accelZ, std::memory_order_relaxed);
    _latestGyroX.store(sample.gyroX, std::memory_order_relaxed);
    _latestGyroY.store(sample.gyroY, std::memory_order_relaxed);
    _latestGyroZ.store(sample.gyroZ, std::memory_order_relaxed);
    _imuPublishSequence.fetch_add(1u, std::memory_order_release);
}

bool ImuButtonInputProvider::readImuSample(ImuMotionSample& sample,
                                           uint32_t& sequence) const
{
    uint32_t before = 0;
    uint32_t after = 0;
    do {
        before = _imuPublishSequence.load(std::memory_order_acquire);
        if ((before & 1u) != 0u) continue;
        sample.accelX = _latestAccelX.load(std::memory_order_relaxed);
        sample.accelY = _latestAccelY.load(std::memory_order_relaxed);
        sample.accelZ = _latestAccelZ.load(std::memory_order_relaxed);
        sample.gyroX = _latestGyroX.load(std::memory_order_relaxed);
        sample.gyroY = _latestGyroY.load(std::memory_order_relaxed);
        sample.gyroZ = _latestGyroZ.load(std::memory_order_relaxed);
        after = _imuPublishSequence.load(std::memory_order_acquire);
    } while (before != after || (after & 1u) != 0u);
    sequence = after;
    return after != 0u;
}

void ImuButtonInputProvider::samplingTask()
{
    TickType_t lastWake = xTaskGetTickCount();
    uint16_t samplesSinceLog = 0;
    while (_sampling.load(std::memory_order_acquire)) {
        GetHAL().updateImuData();
        publishImuSample(readHalImuSample());
        if (++samplesSinceLog >= 300) {
            samplesSinceLog = 0;
            mclog::tagInfo("Vector Run", "IMU async stack={}",
                           static_cast<uint32_t>(uxTaskGetStackHighWaterMark(nullptr)));
        }
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(kImuSamplePeriodMs));
    }
    _samplingTaskExited.store(true, std::memory_order_release);
    vTaskDelete(nullptr);
}

void ImuButtonInputProvider::open()
{
    _neutralSteerDegrees = 0.0f;
    _neutralPitchDegrees = 0.0f;
    _gyroXBias = 0.0f;
    _gyroYBias = 0.0f;
    _throttle = 0.62f;
    _sequence = 0;
    _lastConsumedImuSequence = 0;
    _lastAttitudeUpdateMs = 0;
    _lastValidSampleMs = 0;
    _consecutiveErrors = 0;
    _buttonActions.reset();
    _opened = true;
    _latestAccelX.store(0.0f, std::memory_order_relaxed);
    _latestAccelY.store(0.0f, std::memory_order_relaxed);
    _latestAccelZ.store(0.0f, std::memory_order_relaxed);
    _latestGyroX.store(0.0f, std::memory_order_relaxed);
    _latestGyroY.store(0.0f, std::memory_order_relaxed);
    _latestGyroZ.store(0.0f, std::memory_order_relaxed);
    _imuPublishSequence.store(0u, std::memory_order_relaxed);
    _samplingTaskExited.store(false, std::memory_order_relaxed);
    _sampling.store(true, std::memory_order_release);
    TaskHandle_t taskHandle = nullptr;
    const BaseType_t taskCreated = xTaskCreatePinnedToCore(
        samplingTaskEntry, "vector_imu", kImuTaskStackBytes, this, 3, &taskHandle, 1);
    _asyncSampling = taskCreated == pdPASS;
    if (!_asyncSampling) {
        _sampling.store(false, std::memory_order_release);
        _samplingTaskExited.store(true, std::memory_order_release);
        mclog::tagWarn("Vector Run", "async IMU task unavailable; using synchronous sampling");
    }
    requestCalibration(GetHAL().millis());
}

void ImuButtonInputProvider::resetCalibrationWindow(uint32_t nowMs)
{
    _calibStartMs = nowMs;
    _steerAngleSum = 0.0f;
    _steerAngleSquareSum = 0.0f;
    _pitchAngleSum = 0.0f;
    _pitchAngleSquareSum = 0.0f;
    _gyroXSum = 0.0f;
    _gyroXSquareSum = 0.0f;
    _gyroYSum = 0.0f;
    _gyroYSquareSum = 0.0f;
    _calibrationSampleCount = 0;
}

void ImuButtonInputProvider::requestCalibration(uint32_t nowMs)
{
    _calibrated = false;
    _lastAttitudeUpdateMs = 0;
    _filteredSteer = 0.0f;
    _filteredPitch = 0.0f;
    _attitudeEstimator.clear();
    resetCalibrationWindow(nowMs);
}

InputStatus ImuButtonInputProvider::status(uint32_t nowMs) const
{
    InputStatus result;
    result.axisSource = FlightAxisSource::Imu;
    result.actionSource = FlightActionSource::BodyButtons;
    result.axesConnected = _opened;
    result.actionsConnected = _opened;
    result.calibrationSupported = true;
    result.lastValidSampleMs = _lastValidSampleMs;
    result.consecutiveErrors = _consecutiveErrors;
    if (!_opened) {
        result.readiness = InputReadiness::Disconnected;
        return result;
    }
    if (!_calibrated) {
        result.readiness = InputReadiness::Calibrating;
        if (nowMs > _calibStartMs) {
            result.calibrationProgress = std::min(
                0.98f, static_cast<float>(nowMs - _calibStartMs) /
                           static_cast<float>(kCalibrationDelayMs));
        }
        return result;
    }

    result.calibrationProgress = 1.0f;
    const bool stale = _lastValidSampleMs == 0u ||
                       nowMs - _lastValidSampleMs > kImuStaleAfterMs;
    result.readiness = stale ||
                               _consecutiveErrors >= kFaultAfterConsecutiveErrors
                           ? InputReadiness::Fault
                           : InputReadiness::Ready;
    return result;
}

FlightInput ImuButtonInputProvider::sample(uint32_t nowMs)
{
    if (!_asyncSampling) {
        GetHAL().updateImuData();
        publishImuSample(readHalImuSample());
    }

    ImuMotionSample motion;
    uint32_t imuSequence = 0;
    const bool snapshotAvailable = readImuSample(motion, imuSequence);
    const bool newSnapshot = snapshotAvailable &&
                             imuSequence != _lastConsumedImuSequence;
    if (newSnapshot) {
        _lastConsumedImuSequence = imuSequence;
        if (isUsableImuSample(motion)) {
            _consecutiveErrors = 0;
            _lastValidSampleMs = nowMs;
            const ImuAttitude gravity = solveImuGravityAttitude(motion);
            if (!_calibrated) {
                _steerAngleSum += gravity.steerDegrees;
                _steerAngleSquareSum += gravity.steerDegrees * gravity.steerDegrees;
                _pitchAngleSum += gravity.pitchDegrees;
                _pitchAngleSquareSum += gravity.pitchDegrees * gravity.pitchDegrees;
                _gyroXSum += motion.gyroX;
                _gyroXSquareSum += motion.gyroX * motion.gyroX;
                _gyroYSum += motion.gyroY;
                _gyroYSquareSum += motion.gyroY * motion.gyroY;
                ++_calibrationSampleCount;
            } else {
                const float deltaSeconds = _lastAttitudeUpdateMs == 0u
                    ? static_cast<float>(kImuSamplePeriodMs) * 0.001f
                    : static_cast<float>(nowMs - _lastAttitudeUpdateMs) * 0.001f;
                _lastAttitudeUpdateMs = nowMs;
                const ImuAttitude& attitude = _attitudeEstimator.update(
                    motion, deltaSeconds, _gyroXBias, _gyroYBias);
                const float steer = normalizeTiltControl(
                    attitude.steerDegrees - _neutralSteerDegrees,
                    kSteerDeadZoneDegrees, kSteerFullScaleDegrees);
                const float pitch = normalizeTiltControl(
                    attitude.pitchDegrees - _neutralPitchDegrees,
                    kPitchDeadZoneDegrees, kPitchFullScaleDegrees);
                _filteredSteer += kOutputFilterStrength * (steer - _filteredSteer);
                _filteredPitch += kOutputFilterStrength * (pitch - _filteredPitch);
            }
        } else {
            _consecutiveErrors = std::min<uint16_t>(
                static_cast<uint16_t>(_consecutiveErrors + 1u), UINT16_MAX);
        }
    }

    if (!_calibrated && nowMs - _calibStartMs >= kCalibrationDelayMs) {
        const float steerDeviation = standardDeviation(
            _steerAngleSum, _steerAngleSquareSum, _calibrationSampleCount);
        const float pitchDeviation = standardDeviation(
            _pitchAngleSum, _pitchAngleSquareSum, _calibrationSampleCount);
        const float gyroXDeviation = standardDeviation(
            _gyroXSum, _gyroXSquareSum, _calibrationSampleCount);
        const float gyroYDeviation = standardDeviation(
            _gyroYSum, _gyroYSquareSum, _calibrationSampleCount);
        const bool stable = _calibrationSampleCount >= kMinimumCalibrationSamples &&
                            steerDeviation <= kMaximumCalibrationAngleDeviation &&
                            pitchDeviation <= kMaximumCalibrationAngleDeviation &&
                            gyroXDeviation <= kMaximumCalibrationGyroDeviation &&
                            gyroYDeviation <= kMaximumCalibrationGyroDeviation;
        if (stable) {
            const float divisor = static_cast<float>(_calibrationSampleCount);
            _neutralSteerDegrees = _steerAngleSum / divisor;
            _neutralPitchDegrees = _pitchAngleSum / divisor;
            _gyroXBias = _gyroXSum / divisor;
            _gyroYBias = _gyroYSum / divisor;
            _attitudeEstimator.reset(_neutralSteerDegrees, _neutralPitchDegrees);
            _lastAttitudeUpdateMs = nowMs;
            _calibrated = true;
            mclog::tagInfo(
                "Vector Run",
                "IMU calibrated samples={} neutral=({:.1f},{:.1f}) gyro=({:.1f},{:.1f})",
                _calibrationSampleCount, _neutralSteerDegrees, _neutralPitchDegrees,
                _gyroXBias, _gyroYBias);
        } else {
            mclog::tagWarn(
                "Vector Run",
                "IMU calibration moving; retry samples={} dev=({:.1f},{:.1f},{:.1f},{:.1f})",
                _calibrationSampleCount, steerDeviation, pitchDeviation,
                gyroXDeviation, gyroYDeviation);
            resetCalibrationWindow(nowMs);
        }
    }

    const bool throttleDownPressed = GetHAL().btnA.wasClicked();
    const bool throttleUpPressed = GetHAL().btnB.wasClicked();
    const bool primaryHoldPressed = GetHAL().btnA.wasHold();
    const FlightActions buttonActions = _buttonActions.update(
        GetHAL().btnA.isPressed(), GetHAL().btnB.isPressed(),
        throttleDownPressed, throttleUpPressed, primaryHoldPressed,
        GetHAL().btnB.isHolding());
    if (buttonActions.wasPressed(FlightAction::ThrottleDown)) {
        _throttle = std::max(0.0f, _throttle - 0.08f);
    }
    if (buttonActions.wasPressed(FlightAction::ThrottleUp)) {
        _throttle = std::min(1.0f, _throttle + 0.08f);
    }

    FlightInput input;
    input.throttle = _throttle;
    input.sequence = ++_sequence;
    const bool stale = _lastValidSampleMs == 0u ||
                       nowMs - _lastValidSampleMs > kImuStaleAfterMs;
    input.valid = _calibrated && !stale &&
                  _consecutiveErrors < kFaultAfterConsecutiveErrors;
    if (!_calibrated) return input;

    input.steer = _filteredSteer;
    input.pitch = _filteredPitch;
    input.actions = buttonActions;
    return input;
}

void ImuButtonInputProvider::close()
{
    _sampling.store(false, std::memory_order_release);
    while (_asyncSampling && !_samplingTaskExited.load(std::memory_order_acquire)) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    _asyncSampling = false;
    _calibrated = false;
    _opened = false;
}

}  // namespace vector_canyon_fighter
