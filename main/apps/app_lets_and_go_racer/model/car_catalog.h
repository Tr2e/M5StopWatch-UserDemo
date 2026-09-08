#pragma once

#include "game_types.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace lets_and_go {

enum class CarLod : uint8_t { Race, Showcase };
enum class WireStroke : uint8_t { Body, Accent, Mechanical };

struct CarPoint {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct WireLine {
    CarPoint from{};
    CarPoint to{};
    WireStroke stroke = WireStroke::Body;
};

struct CarProfileStation {
    float z = 0.0f;
    float halfWidth = 0.0f;
    float sillY = 0.0f;
    float deckY = 0.0f;
    float centerY = 0.0f;
};

struct CarDimensionsMm {
    uint16_t length = 0;
    uint16_t width = 0;
    uint16_t height = 0;
};

struct CarPerformance {
    float topSpeed = 0.0f;
    float acceleration = 0.0f;
    float steering = 0.0f;
    float stability = 0.0f;
};

struct CarSpec {
    CarId id = CarId::CycloneMagnum;
    const char* officialName = "";
    const char* shortName = "";
    CarDimensionsMm dimensions{};
    CarPerformance performance{};
    uint16_t bodyColor = 0;
    uint16_t accentColor = 0;
    uint16_t wheelColor = 0;
    std::array<CarProfileStation, 7> profile{};
    float frontAxleZ = 0.0f;
    float rearAxleZ = 0.0f;
    float wheelRadius = 0.0f;
    float wingZ = 0.0f;
    float wingY = 0.0f;
    float wingHalfWidth = 0.0f;
    float wingChord = 0.0f;
    uint8_t wingPlanes = 1;
};

struct CarWireframe {
    static constexpr std::size_t kMaximumLines = 144;
    std::array<WireLine, kMaximumLines> lines{};
    std::size_t lineCount = 0;
    bool overflowed = false;
};

const std::array<CarSpec, kCarCount>& carCatalog();
const CarSpec& carSpec(CarId id);
CarWireframe buildCarWireframe(CarId id, CarLod lod);

}  // namespace lets_and_go
