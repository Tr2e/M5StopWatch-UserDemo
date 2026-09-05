#pragma once

#include "imu_attitude_estimator.h"
#include "input_provider.h"

#include <atomic>

namespace vector_canyon_fighter {

class ImuButtonInputProvider final : public InputProvider {
public:
    ~ImuButtonInputProvider() override;
    void open() override;
    FlightInput sample(uint32_t nowMs) override;
    InputStatus status(uint32_t nowMs) const override;
    void requestCalibration(uint32_t nowMs) override;
    void close() override;

private:
    static void samplingTaskEntry(void* context);
    void samplingTask();
    void publishImuSample(const ImuMotionSample& sample);
    bool readImuSample(ImuMotionSample& sample, uint32_t& sequence) const;
    void resetCalibrationWindow(uint32_t nowMs);

    std::atomic<float> _latestAccelX{0.0f};
    std::atomic<float> _latestAccelY{0.0f};
    std::atomic<float> _latestAccelZ{0.0f};
    std::atomic<float> _latestGyroX{0.0f};
    std::atomic<float> _latestGyroY{0.0f};
    std::atomic<float> _latestGyroZ{0.0f};
    std::atomic<uint32_t> _imuPublishSequence{0};
    std::atomic<bool> _sampling{false};
    std::atomic<bool> _samplingTaskExited{true};
    ImuAttitudeEstimator _attitudeEstimator;
    TwoButtonFlightActionMapper _buttonActions;
    float _neutralSteerDegrees = 0.0f;
    float _neutralPitchDegrees = 0.0f;
    float _gyroXBias = 0.0f;
    float _gyroYBias = 0.0f;
    float _filteredSteer = 0.0f;
    float _filteredPitch = 0.0f;
    float _throttle = 0.62f;
    uint32_t _sequence = 0;
    uint32_t _lastConsumedImuSequence = 0;
    uint32_t _lastAttitudeUpdateMs = 0;
    uint32_t _calibStartMs = 0;
    uint32_t _lastValidSampleMs = 0;
    float _steerAngleSum = 0.0f;
    float _steerAngleSquareSum = 0.0f;
    float _pitchAngleSum = 0.0f;
    float _pitchAngleSquareSum = 0.0f;
    float _gyroXSum = 0.0f;
    float _gyroXSquareSum = 0.0f;
    float _gyroYSum = 0.0f;
    float _gyroYSquareSum = 0.0f;
    uint16_t _calibrationSampleCount = 0;
    uint16_t _consecutiveErrors = 0;
    bool _calibrated = false;
    bool _asyncSampling = false;
    bool _opened = false;
};

}  // namespace vector_canyon_fighter
