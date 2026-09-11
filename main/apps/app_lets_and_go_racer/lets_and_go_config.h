#pragma once

#include <cstdint>

namespace lets_and_go::tuning {

inline constexpr uint32_t kFrameIntervalMs = 33u;
inline constexpr uint32_t kShowcaseDurationMs = 1500u;
inline constexpr uint32_t kGridIntroDurationMs = 1300u;
inline constexpr uint32_t kCountdownDurationMs = 3000u;
inline constexpr uint32_t kFinishDurationMs = 1200u;

inline constexpr float kCourseRadius = 12.0f;
inline constexpr float kCourseBaseHeight = 0.35f;
inline constexpr float kOverpassRise = 3.8f;

// Bounded first-release AI pace: last-grid rolling starts must be recoverable
// with boost AND a clean passing line, including the slower handling cars.
inline constexpr float kRivalMotorEfficiency = 0.93f;
inline constexpr float kRivalMotorVariation = 0.025f;
inline constexpr float kRivalMinimumEfficiency = 0.88f;

inline constexpr uint32_t kRenderOverloadMs = 39u;
inline constexpr uint32_t kRenderSevereMs = 54u;
inline constexpr uint32_t kRenderHealthyMs = 29u;
inline constexpr uint8_t kFramesToDegrade = 10u;
inline constexpr uint8_t kFramesToRecoverMedium = 120u;
inline constexpr uint8_t kFramesToRecoverHigh = 150u;

struct FeedbackCue {
    uint8_t vibrationStrength;
    uint16_t vibrationDurationMs;
};

inline constexpr FeedbackCue kCountdownCue{18, 35};
inline constexpr FeedbackCue kGoCue{42, 62};
inline constexpr FeedbackCue kBoostCue{22, 42};
inline constexpr FeedbackCue kWallCue{55, 76};
inline constexpr FeedbackCue kFinalLapCue{50, 68};
inline constexpr FeedbackCue kFinishCue{95, 85};

}  // namespace lets_and_go::tuning
