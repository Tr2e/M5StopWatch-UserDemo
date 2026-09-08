#pragma once

#include "game_types.h"

#include <array>
#include <cstdint>

namespace lets_and_go {

inline constexpr uint8_t kPlayerProgressVersion = 1u;

struct PlayerProgress {
    uint8_t version = kPlayerProgressVersion;
    CarId lastCar = CarId::CycloneMagnum;
    std::array<uint32_t, kTrackCount> bestLapMilliseconds{};
};

inline PlayerProgress sanitizePlayerProgress(PlayerProgress progress)
{
    if (progress.version != kPlayerProgressVersion) return {};
    if (!isValidCar(progress.lastCar)) progress.lastCar = CarId::CycloneMagnum;
    for (uint32_t& lap : progress.bestLapMilliseconds) {
        // Corrupt/unrealistic values are discarded rather than shown forever.
        if (lap < 1000u || lap > 10u * 60u * 1000u) lap = 0u;
    }
    return progress;
}

inline bool recordBestLap(PlayerProgress& progress, TrackId track, float seconds)
{
    if (!isValidTrack(track) || !(seconds >= 1.0f) || !(seconds <= 600.0f)) return false;
    const uint32_t milliseconds = static_cast<uint32_t>(seconds * 1000.0f + 0.5f);
    uint32_t& stored = progress.bestLapMilliseconds[static_cast<std::size_t>(track)];
    if (stored != 0u && stored <= milliseconds) return false;
    stored = milliseconds;
    return true;
}

}  // namespace lets_and_go
