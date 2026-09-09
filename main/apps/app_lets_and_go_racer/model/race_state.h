#pragma once

#include "game_types.h"
#include "racer_model.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace lets_and_go {

inline constexpr std::size_t kMaximumRaceCars = kMaximumRivals + 1u;
inline constexpr uint8_t kRaceLapCount = 3u;

struct RaceCarSnapshot {
    CarId car = CarId::CycloneMagnum;
    RacerState motion{};
    float startDistance = 0.0f;
    float raceProgress = 0.0f;
    float bestLapSeconds = 0.0f;
    float finishSeconds = 0.0f;
    uint8_t completedLaps = 0;
    uint8_t position = 1;
    uint8_t finishOrder = 0;
    bool player = false;
    bool active = false;
    bool finished = false;
};

struct RaceSnapshot {
    std::array<RaceCarSnapshot, kMaximumRaceCars> cars{};
    std::size_t carCount = 0;
    std::size_t playerIndex = 0;
    float elapsedSeconds = 0.0f;
    bool playerFinished = false;
    uint16_t simulationClampCount = 0;

    const RaceCarSnapshot& player() const { return cars[playerIndex]; }
};

inline bool raceCarAhead(const RaceCarSnapshot& a, std::size_t aIndex,
                         const RaceCarSnapshot& b, std::size_t bIndex)
{
    if (a.finished != b.finished) return a.finished;
    if (a.finished && a.finishSeconds != b.finishSeconds) {
        return a.finishSeconds < b.finishSeconds;
    }
    if (!a.finished && a.motion.distance != b.motion.distance) {
        return a.motion.distance > b.motion.distance;
    }
    return aIndex < bIndex; // Stable, unique positions even on an exact tie.
}

static_assert(kMaximumRaceCars == 3u, "device performance trial supports three grid slots");
static_assert(sizeof(RaceSnapshot) <= 320u,
              "race snapshot exceeded its fixed render-copy budget");

}  // namespace lets_and_go
