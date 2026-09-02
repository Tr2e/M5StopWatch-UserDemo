#pragma once

#include <cstdint>

namespace vector_canyon_fighter {

inline constexpr float kExplicitCanyonPreviewCruiseSpeed = 104.0f;
inline constexpr float kExplicitCanyonPreviewBoostSpeed = 176.0f;
inline constexpr uint32_t kExplicitCanyonPreviewSpeedCycleMs = 20000;
inline constexpr uint32_t kExplicitCanyonPreviewBoostStartMs = 15000;

inline bool isExplicitCanyonPreviewBoosted(uint32_t elapsedMs)
{
    return elapsedMs % kExplicitCanyonPreviewSpeedCycleMs >= kExplicitCanyonPreviewBoostStartMs;
}

inline float explicitCanyonPreviewSpeed(uint32_t elapsedMs)
{
    return isExplicitCanyonPreviewBoosted(elapsedMs) ? kExplicitCanyonPreviewBoostSpeed
                                                     : kExplicitCanyonPreviewCruiseSpeed;
}

}  // namespace vector_canyon_fighter
