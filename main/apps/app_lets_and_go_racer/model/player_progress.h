#pragma once

#include "game_types.h"

#include <array>
#include <cstdint>
#include <cstring>

namespace lets_and_go {

inline constexpr uint8_t kPlayerProgressVersion = 3u;

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

// V1 persisted one track in an eight-byte native struct. Explicit migration
// preserves the selected car and SKY LOOP record; V2 also preserves TRI CROSS.
inline PlayerProgress decodePlayerProgress(const void* bytes, std::size_t size)
{
    if (!bytes) return {};
    PlayerProgress progress{};
    if (size == sizeof(PlayerProgress)) {
        std::memcpy(&progress, bytes, size);
    } else if(size==12u) {
        struct V2 { uint8_t version; CarId lastCar; std::array<uint32_t,2> lap; };
        static_assert(sizeof(V2)==12, "V2 NVS layout");
        V2 legacy{};std::memcpy(&legacy,bytes,size);
        if(legacy.version!=2)return {};
        progress.lastCar=legacy.lastCar;
        progress.bestLapMilliseconds[0]=legacy.lap[0];
        progress.bestLapMilliseconds[1]=legacy.lap[1];
    } else {
        struct LegacyProgress { uint8_t version; CarId lastCar; uint32_t lap; };
        static_assert(sizeof(LegacyProgress)==8, "V1 NVS layout");
        if (size != sizeof(LegacyProgress)) return {};
        LegacyProgress legacy{};
        std::memcpy(&legacy, bytes, size);
        if (legacy.version != 1) return {};
        progress.lastCar=legacy.lastCar;
        progress.bestLapMilliseconds[0]=legacy.lap;
    }
    return sanitizePlayerProgress(progress);
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
