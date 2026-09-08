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

static_assert(kMaximumRaceCars == 4u, "first release supports exactly four grid slots");
static_assert(sizeof(RaceSnapshot) <= 320u,
              "race snapshot exceeded its fixed render-copy budget");

}  // namespace lets_and_go
