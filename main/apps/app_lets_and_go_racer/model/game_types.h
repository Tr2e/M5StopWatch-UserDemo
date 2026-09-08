#pragma once

#include <cstddef>
#include <cstdint>

namespace lets_and_go {

enum class CarId : uint8_t {
    CycloneMagnum = 0,
    HurricaneSonic,
    NeoTridaggerZmc,
    BrockenGigant,
    SpinCobra,
    BeakSpider,
    RayStinger,
    Diospada,
    Count,
};

enum class TrackId : uint8_t {
    SkyLoop = 0,
    TriCross,
    Count,
};

constexpr std::size_t kCarCount = static_cast<std::size_t>(CarId::Count);
constexpr std::size_t kTrackCount = static_cast<std::size_t>(TrackId::Count);
constexpr uint8_t kMaximumRivals = 3;

static_assert(kCarCount > 0u && kCarCount <= 8u,
              "CarId must fit in the rival selection bit mask");
static_assert(kMaximumRivals < kCarCount,
              "A player car must remain outside the rival selection");

constexpr bool isValidCar(CarId car)
{
    return static_cast<std::size_t>(car) < kCarCount;
}

constexpr bool isValidTrack(TrackId track)
{
    return static_cast<std::size_t>(track) < kTrackCount;
}

constexpr uint8_t carMask(CarId car)
{
    return isValidCar(car)
               ? static_cast<uint8_t>(1u << static_cast<uint8_t>(car))
               : 0u;
}

}  // namespace lets_and_go
