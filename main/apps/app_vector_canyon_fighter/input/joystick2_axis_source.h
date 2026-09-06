#pragma once

#include "external_input_logic.h"
#include "flight_input_sources.h"

#include <atomic>
#include <cstdint>

namespace vector_canyon_fighter {

class Joystick2AxisSource final : public FlightAxisProvider {
public:
    Joystick2AxisSource() = default;
    ~Joystick2AxisSource() override;

    void open() override;
    FlightAxisSample sampleAxes(uint32_t nowMs) override;
    FlightAxisStatus axisStatus(uint32_t nowMs) const override;
    void requestAxisCalibration(uint32_t nowMs) override;
    bool sampleStickButtonClick(uint32_t nowMs);
    void close() override;

private:
    static void samplingTaskEntry(void* context);
    void samplingTask();
    bool readOffset(int16_t& x, int16_t& y);
    bool writeRgb(uint32_t packedColor);
    void updateRgbFeedback(uint32_t nowMs, joystick2::LedFeedbackState state,
                           float steer = 0.0f, float pitch = 0.0f,
                           bool force = false);
    void publishOffset(int16_t x, int16_t y);
    bool readPublishedOffset(int16_t& x, int16_t& y, uint32_t& sequence) const;

    void* _bus = nullptr;
    void* _device = nullptr;
    std::atomic<bool> _sampling{false};
    std::atomic<bool> _samplingTaskExited{true};
    std::atomic<uint32_t> _publishSequence{0};
    std::atomic<int16_t> _latestX{0};
    std::atomic<int16_t> _latestY{0};
    std::atomic<bool> _stickButtonPressed{false};
    std::atomic<uint32_t> _lastValidSampleMs{0};
    std::atomic<uint16_t> _consecutiveErrors{0};
    std::atomic<bool> _identified{false};
    std::atomic<uint8_t> _firmwareVersion{0};
    uint32_t _lastConsumedSequence = 0;
    uint32_t _calibrationStartedMs = 0;
    int32_t _calibrationXSum = 0;
    int32_t _calibrationYSum = 0;
    int16_t _calibrationMinX = 0;
    int16_t _calibrationMaxX = 0;
    int16_t _calibrationMinY = 0;
    int16_t _calibrationMaxY = 0;
    uint16_t _calibrationSamples = 0;
    int16_t _neutralX = 0;
    int16_t _neutralY = 0;
    float _filteredSteer = 0.0f;
    float _filteredPitch = 0.0f;
    uint32_t _lastRgbUpdateMs = 0;
    uint32_t _lastRgbColor = UINT32_MAX;
    DebouncedActiveLowButton _stickButton;
    bool _calibrated = false;
    bool _opened = false;
};

}  // namespace vector_canyon_fighter
