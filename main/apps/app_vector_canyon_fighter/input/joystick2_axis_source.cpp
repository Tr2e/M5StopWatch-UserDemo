#include "joystick2_axis_source.h"

#include "external_input_logic.h"

#include <algorithm>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <i2c_bus.h>
#include <mooncake_log.h>

namespace vector_canyon_fighter {
namespace {

constexpr gpio_num_t kJoystickSda = GPIO_NUM_10;
constexpr gpio_num_t kJoystickScl = GPIO_NUM_11;
constexpr uint32_t kBusFrequencyHz = 100000u;
constexpr uint32_t kSamplePeriodMs = 20u;
constexpr uint32_t kStaleAfterMs = 300u;
constexpr uint16_t kFaultAfterErrors = 3u;
constexpr uint32_t kCalibrationDurationMs = 700u;
constexpr uint16_t kMinimumCalibrationSamples = 20u;
constexpr int16_t kMaximumCalibrationTravel = 180;
constexpr float kOutputFilterStrength = 0.42f;
constexpr uint32_t kSamplingTaskStackBytes = 4u * 1024u;

uint32_t taskTimeMs()
{
    return static_cast<uint32_t>(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

}  // namespace

Joystick2AxisSource::~Joystick2AxisSource()
{
    close();
}

void Joystick2AxisSource::samplingTaskEntry(void* context)
{
    static_cast<Joystick2AxisSource*>(context)->samplingTask();
}

bool Joystick2AxisSource::readOffset(int16_t& x, int16_t& y)
{
    uint8_t bytes[4] = {};
    const esp_err_t result = i2c_bus_read_bytes(
        static_cast<i2c_bus_device_handle_t>(_device),
        joystick2::kOffsetRegister, sizeof(bytes), bytes);
    if (result != ESP_OK) return false;
    x = joystick2::decodeSignedLittleEndian(bytes);
    y = joystick2::decodeSignedLittleEndian(bytes + 2);
    return true;
}

void Joystick2AxisSource::publishOffset(int16_t x, int16_t y)
{
    _publishSequence.fetch_add(1u, std::memory_order_acq_rel);
    _latestX.store(x, std::memory_order_relaxed);
    _latestY.store(y, std::memory_order_relaxed);
    _publishSequence.fetch_add(1u, std::memory_order_release);
}

bool Joystick2AxisSource::readPublishedOffset(int16_t& x, int16_t& y,
                                               uint32_t& sequence) const
{
    uint32_t before = 0;
    uint32_t after = 0;
    do {
        before = _publishSequence.load(std::memory_order_acquire);
        if ((before & 1u) != 0u) continue;
        x = _latestX.load(std::memory_order_relaxed);
        y = _latestY.load(std::memory_order_relaxed);
        after = _publishSequence.load(std::memory_order_acquire);
    } while (before != after || (after & 1u) != 0u);
    sequence = after;
    return after != 0u;
}

void Joystick2AxisSource::samplingTask()
{
    TickType_t lastWake = xTaskGetTickCount();
    while (_sampling.load(std::memory_order_acquire)) {
        if (!_identified.load(std::memory_order_acquire)) {
            uint8_t version = 0;
            if (i2c_bus_read_byte(
                    static_cast<i2c_bus_device_handle_t>(_device),
                    joystick2::kFirmwareVersionRegister, &version) != ESP_OK) {
                const uint16_t errors =
                    _consecutiveErrors.load(std::memory_order_relaxed);
                _consecutiveErrors.store(
                    errors == UINT16_MAX
                        ? errors
                        : static_cast<uint16_t>(errors + 1u),
                    std::memory_order_release);
                vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(kSamplePeriodMs));
                continue;
            }
            _firmwareVersion.store(version, std::memory_order_relaxed);
            _identified.store(true, std::memory_order_release);
            mclog::tagInfo("Vector Run", "Joystick2 identified addr=0x63 fw={}",
                           version);
        }
        int16_t x = 0;
        int16_t y = 0;
        if (readOffset(x, y)) {
            publishOffset(x, y);
            _lastValidSampleMs.store(taskTimeMs(), std::memory_order_release);
            _consecutiveErrors.store(0u, std::memory_order_release);
        } else {
            const uint16_t errors = _consecutiveErrors.load(std::memory_order_relaxed);
            _consecutiveErrors.store(
                errors == UINT16_MAX ? errors : static_cast<uint16_t>(errors + 1u),
                std::memory_order_release);
        }
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(kSamplePeriodMs));
    }
    _samplingTaskExited.store(true, std::memory_order_release);
    vTaskDelete(nullptr);
}

void Joystick2AxisSource::open()
{
    if (_opened) return;
    i2c_config_t config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = kJoystickSda,
        .scl_io_num = kJoystickScl,
        .sda_pullup_en = true,
        .scl_pullup_en = true,
        .master = {.clk_speed = kBusFrequencyHz},
        .clk_flags = 0,
    };
    _bus = i2c_bus_create(I2C_NUM_1, &config);
    if (_bus) {
        _device = i2c_bus_device_create(
            static_cast<i2c_bus_handle_t>(_bus), joystick2::kDefaultAddress,
            kBusFrequencyHz);
    }
    _opened = _bus != nullptr && _device != nullptr;
    _publishSequence.store(0u, std::memory_order_relaxed);
    _lastValidSampleMs.store(0u, std::memory_order_relaxed);
    _consecutiveErrors.store(0u, std::memory_order_relaxed);
    _identified.store(false, std::memory_order_relaxed);
    _firmwareVersion.store(0u, std::memory_order_relaxed);
    _lastConsumedSequence = 0;
    _samplingTaskExited.store(!_opened, std::memory_order_relaxed);
    if (!_opened) {
        mclog::tagError("Vector Run", "Joystick2 I2C bus/device creation failed");
        close();
        return;
    }

    _sampling.store(true, std::memory_order_release);
    TaskHandle_t task = nullptr;
    const BaseType_t created = xTaskCreatePinnedToCore(
        samplingTaskEntry, "vector_joy", kSamplingTaskStackBytes, this, 3,
        &task, 1);
    if (created != pdPASS) {
        _sampling.store(false, std::memory_order_release);
        _samplingTaskExited.store(true, std::memory_order_release);
        mclog::tagError("Vector Run", "Joystick2 sampling task creation failed");
        close();
        return;
    }
    requestAxisCalibration(taskTimeMs());
}

void Joystick2AxisSource::requestAxisCalibration(uint32_t nowMs)
{
    _calibrationStartedMs = nowMs;
    _calibrationXSum = 0;
    _calibrationYSum = 0;
    _calibrationSamples = 0;
    _calibrationMinX = INT16_MAX;
    _calibrationMaxX = INT16_MIN;
    _calibrationMinY = INT16_MAX;
    _calibrationMaxY = INT16_MIN;
    _filteredSteer = 0.0f;
    _filteredPitch = 0.0f;
    _calibrated = false;
}

FlightAxisSample Joystick2AxisSource::sampleAxes(uint32_t nowMs)
{
    FlightAxisSample result;
    if (!_opened) return result;

    int16_t x = 0;
    int16_t y = 0;
    uint32_t sequence = 0;
    const bool available = readPublishedOffset(x, y, sequence);
    const bool isNew = available && sequence != _lastConsumedSequence;
    if (isNew) {
        _lastConsumedSequence = sequence;
        if (!_calibrated) {
            _calibrationXSum += x;
            _calibrationYSum += y;
            _calibrationMinX = std::min(_calibrationMinX, x);
            _calibrationMaxX = std::max(_calibrationMaxX, x);
            _calibrationMinY = std::min(_calibrationMinY, y);
            _calibrationMaxY = std::max(_calibrationMaxY, y);
            ++_calibrationSamples;
        } else {
            const float steer = joystick2::normalizeOffset(x, _neutralX);
            // Physical up is confirmed during the G4 bench check. Keeping the
            // polarity here makes that adjustment local to this adapter.
            const float pitch = -joystick2::normalizeOffset(y, _neutralY);
            _filteredSteer += kOutputFilterStrength * (steer - _filteredSteer);
            _filteredPitch += kOutputFilterStrength * (pitch - _filteredPitch);
        }
    }

    if (!_calibrated && nowMs - _calibrationStartedMs >= kCalibrationDurationMs &&
        _calibrationSamples >= kMinimumCalibrationSamples) {
        const bool stable =
            _calibrationMaxX - _calibrationMinX <= kMaximumCalibrationTravel &&
            _calibrationMaxY - _calibrationMinY <= kMaximumCalibrationTravel;
        if (stable) {
            _neutralX = static_cast<int16_t>(_calibrationXSum / _calibrationSamples);
            _neutralY = static_cast<int16_t>(_calibrationYSum / _calibrationSamples);
            _calibrated = true;
            mclog::tagInfo("Vector Run", "Joystick2 calibrated neutral=({},{})",
                           _neutralX, _neutralY);
        } else {
            mclog::tagWarn("Vector Run", "Joystick2 moved during calibration; retry");
            requestAxisCalibration(nowMs);
        }
    }

    const uint32_t lastValid = _lastValidSampleMs.load(std::memory_order_acquire);
    result.valid = _calibrated && lastValid != 0u &&
                   nowMs - lastValid <= kStaleAfterMs &&
                   _consecutiveErrors.load(std::memory_order_acquire) <
                       kFaultAfterErrors;
    if (result.valid) {
        result.steer = _filteredSteer;
        result.pitch = _filteredPitch;
    }
    return result;
}

FlightAxisStatus Joystick2AxisSource::axisStatus(uint32_t nowMs) const
{
    FlightAxisStatus result;
    result.source = FlightAxisSource::Joystick2;
    result.calibrationSupported = true;
    result.lastValidSampleMs = _lastValidSampleMs.load(std::memory_order_acquire);
    result.consecutiveErrors = _consecutiveErrors.load(std::memory_order_acquire);
    const bool fresh = result.lastValidSampleMs != 0u &&
                       nowMs - result.lastValidSampleMs <= kStaleAfterMs;
    result.connected = _opened &&
                       _identified.load(std::memory_order_acquire) && fresh;
    if (!_opened || !fresh) {
        result.readiness = result.consecutiveErrors >= kFaultAfterErrors
                               ? InputReadiness::Fault
                               : InputReadiness::Disconnected;
    } else if (!_calibrated) {
        result.readiness = InputReadiness::Calibrating;
        result.calibrationProgress = std::min(
            0.98f, static_cast<float>(nowMs - _calibrationStartedMs) /
                       static_cast<float>(kCalibrationDurationMs));
    } else {
        result.readiness = InputReadiness::Ready;
        result.calibrationProgress = 1.0f;
    }
    return result;
}

void Joystick2AxisSource::close()
{
    _sampling.store(false, std::memory_order_release);
    while (!_samplingTaskExited.load(std::memory_order_acquire)) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    if (_device) {
        auto device = static_cast<i2c_bus_device_handle_t>(_device);
        i2c_bus_device_delete(&device);
        _device = nullptr;
    }
    if (_bus) {
        auto bus = static_cast<i2c_bus_handle_t>(_bus);
        i2c_bus_delete(&bus);
        _bus = nullptr;
    }
    _calibrated = false;
    _identified.store(false, std::memory_order_relaxed);
    _opened = false;
}

}  // namespace vector_canyon_fighter
