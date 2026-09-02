#include "imu_button_input_provider.h"

#include <algorithm>
#include <cmath>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <hal/hal.h>
#include <mooncake_log.h>

namespace vector_canyon_fighter {
namespace {

constexpr float kDeadZone = 0.08f;
constexpr float kAxisSensitivity = 0.85f;
constexpr float kFilterStrength = 0.18f;
constexpr uint32_t kCalibrationDelayMs = 2500;
constexpr uint32_t kImuSamplePeriodMs = 33;
constexpr uint32_t kImuTaskStackBytes = 5 * 1024;

float normalizeAxis(float value)
{
    if (std::fabs(value) <= kDeadZone) return 0.0f;
    const float signedValue = value < 0.0f ? -1.0f : 1.0f;
    const float scaled = (std::fabs(value) - kDeadZone) * kAxisSensitivity;
    return signedValue * std::min(scaled, 1.0f);
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

void ImuButtonInputProvider::samplingTask()
{
    TickType_t lastWake = xTaskGetTickCount();
    uint16_t samplesSinceLog = 0;
    while (_sampling.load(std::memory_order_acquire)) {
        GetHAL().updateImuData();
        const auto& imu = GetHAL().getImuData();
        _latestAccelX.store(imu.accelX, std::memory_order_relaxed);
        _latestAccelY.store(imu.accelY, std::memory_order_relaxed);
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
    _neutralAccelX = 0.0f;
    _neutralAccelY = 0.0f;
    _throttle = 0.62f;
    _sequence = 0;
    _openedAtMs = GetHAL().millis();
    _pauseLatched = false;
    _latestAccelX.store(0.0f, std::memory_order_relaxed);
    _latestAccelY.store(0.0f, std::memory_order_relaxed);
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
    startCalibration(_openedAtMs);
}

void ImuButtonInputProvider::startCalibration(uint32_t nowMs)
{
    _calibrated = false;
    _calibStartMs = nowMs;
    _accumX = 0.0f;
    _accumY = 0.0f;
    _accumCount = 0;
    _filteredSteer = 0.0f;
    _filteredPitch = 0.0f;
}

float ImuButtonInputProvider::calibrationProgress(uint32_t nowMs) const
{
    if (_calibrated) return 1.0f;
    if (nowMs <= _calibStartMs) return 0.0f;
    return std::min(1.0f, static_cast<float>(nowMs - _calibStartMs) /
                              static_cast<float>(kCalibrationDelayMs));
}

FlightInput ImuButtonInputProvider::sample(uint32_t nowMs)
{
    if (!_asyncSampling) {
        GetHAL().updateImuData();
        const auto& imu = GetHAL().getImuData();
        _latestAccelX.store(imu.accelX, std::memory_order_relaxed);
        _latestAccelY.store(imu.accelY, std::memory_order_relaxed);
    }
    const float accelX = _latestAccelX.load(std::memory_order_relaxed);
    const float accelY = _latestAccelY.load(std::memory_order_relaxed);

    if (!_calibrated) {
        _accumX += accelX;
        _accumY += accelY;
        ++_accumCount;
        if (nowMs - _calibStartMs >= kCalibrationDelayMs) {
            _neutralAccelX = _accumCount > 0 ? _accumX / _accumCount : accelX;
            _neutralAccelY = _accumCount > 0 ? _accumY / _accumCount : accelY;
            _calibrated = true;
        }
    }

    if (GetHAL().btnA.wasClicked()) _throttle = std::max(0.0f, _throttle - 0.08f);
    if (GetHAL().btnB.wasClicked()) _throttle = std::min(1.0f, _throttle + 0.08f);

    FlightInput input;
    input.throttle = _throttle;
    input.valid = _calibrated;
    input.sequence = ++_sequence;
    if (!_calibrated) return input;

    const float steer = normalizeAxis(accelX - _neutralAccelX);
    const float pitch = normalizeAxis(accelY - _neutralAccelY);
    _filteredSteer += kFilterStrength * (steer - _filteredSteer);
    _filteredPitch += kFilterStrength * (pitch - _filteredPitch);
    input.steer = _filteredSteer;
    input.pitch = _filteredPitch;
    input.boostActive = GetHAL().btnB.isHolding();
    if (!GetHAL().btnA.isHolding()) {
        _pauseLatched = false;
    } else if (!_pauseLatched) {
        input.pausePressed = true;
        _pauseLatched = true;
    }
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
}

}  // namespace vector_canyon_fighter
