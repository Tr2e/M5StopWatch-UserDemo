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

inline constexpr uint32_t kRenderOverloadMs = 39u;
inline constexpr uint32_t kRenderSevereMs = 54u;
inline constexpr uint32_t kRenderHealthyMs = 29u;
inline constexpr uint8_t kFramesToDegrade = 10u;
inline constexpr uint8_t kFramesToRecoverMedium = 120u;
inline constexpr uint8_t kFramesToRecoverHigh = 150u;

struct FeedbackCue {
    int frequencyHz;
    float durationSeconds;
    float volume;
    uint8_t vibrationStrength;
    uint16_t vibrationDurationMs;
};

inline constexpr FeedbackCue kCountdownCue{720, 0.035f, 0.35f, 18, 35};
inline constexpr FeedbackCue kGoCue{1380, 0.07f, 0.45f, 42, 62};
inline constexpr FeedbackCue kBoostCue{1120, 0.025f, 0.28f, 22, 42};
inline constexpr FeedbackCue kWallCue{230, 0.045f, 0.42f, 55, 76};
inline constexpr FeedbackCue kFinalLapCue{1540, 0.08f, 0.40f, 50, 68};
inline constexpr FeedbackCue kFinishCue{1760, 0.12f, 0.48f, 95, 85};

}  // namespace lets_and_go::tuning
