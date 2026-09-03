#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace vector_canyon_fighter {

inline constexpr float kExplicitCanyonFloorHalfWidth = 2.18f;
inline constexpr float kExplicitCanyonWallHeight = 3.80f;

enum class CanyonProfilePoint : uint8_t {
    LeftPlateauOuter,
    LeftPlateauMid,
    LeftPlateauInner,
    LeftCap,
    LeftFaceHigh,
    LeftFaceLow,
    LeftToe,
    LeftFloorEdge,
    FloorLeft4,
    FloorLeft3,
    FloorLeft2,
    FloorLeft1,
    FloorCenter,
    FloorRight1,
    FloorRight2,
    FloorRight3,
    FloorRight4,
    RightFloorEdge,
    RightToe,
    RightFaceLow,
    RightFaceHigh,
    RightCap,
    RightPlateauInner,
    RightPlateauMid,
    RightPlateauOuter,
    Count,
};

struct CanyonProfileSample {
    float lateral = 0.0f;
    float height = 0.0f;
};

inline constexpr std::size_t kExplicitCanyonProfileCount =
    static_cast<std::size_t>(CanyonProfilePoint::Count);

inline constexpr std::array<CanyonProfileSample, kExplicitCanyonProfileCount> kExplicitCanyonProfile = {{
    {-7.00f, kExplicitCanyonWallHeight}, {-5.20f, kExplicitCanyonWallHeight},
    {-3.65f, kExplicitCanyonWallHeight}, {-3.32f, kExplicitCanyonWallHeight}, {-3.04f, 3.34f},
    {-2.67f, 1.06f}, {-2.38f, 0.38f}, {-kExplicitCanyonFloorHalfWidth, 0.0f},
    {-1.744f, 0.0f}, {-1.308f, 0.0f}, {-0.872f, 0.0f}, {-0.436f, 0.0f}, {0.0f, 0.0f},
    {0.436f, 0.0f}, {0.872f, 0.0f}, {1.308f, 0.0f}, {1.744f, 0.0f},
    {kExplicitCanyonFloorHalfWidth, 0.0f}, {2.38f, 0.38f}, {2.67f, 1.06f}, {3.04f, 3.34f},
    {3.32f, kExplicitCanyonWallHeight}, {3.65f, kExplicitCanyonWallHeight},
    {5.20f, kExplicitCanyonWallHeight}, {7.00f, kExplicitCanyonWallHeight},
}};

// Returns how far the rendered cliff surface sits outside its floor edge at a
// given height. This is derived from the right-side semantic profile; the left
// side is its mirror and has the same positive outset.
inline float explicitCanyonWallOutsetAtHeight(float height)
{
    constexpr std::array<CanyonProfilePoint, 5> kWallPoints = {{
        CanyonProfilePoint::RightFloorEdge,
        CanyonProfilePoint::RightToe,
        CanyonProfilePoint::RightFaceLow,
        CanyonProfilePoint::RightFaceHigh,
        CanyonProfilePoint::RightCap,
    }};
    const CanyonProfileSample& floor =
        kExplicitCanyonProfile[static_cast<std::size_t>(CanyonProfilePoint::RightFloorEdge)];
    if (height <= floor.height) return 0.0f;
    for (std::size_t point = 1; point < kWallPoints.size(); ++point) {
        const CanyonProfileSample& from =
            kExplicitCanyonProfile[static_cast<std::size_t>(kWallPoints[point - 1])];
        const CanyonProfileSample& to =
            kExplicitCanyonProfile[static_cast<std::size_t>(kWallPoints[point])];
        if (height <= to.height) {
            const float blend = (height - from.height) / (to.height - from.height);
            const float lateral = from.lateral + (to.lateral - from.lateral) * blend;
            return lateral - floor.lateral;
        }
    }
    const CanyonProfileSample& cap =
        kExplicitCanyonProfile[static_cast<std::size_t>(CanyonProfilePoint::RightCap)];
    return cap.lateral - floor.lateral;
}

enum class CanyonSide : uint8_t {
    Left,
    Right,
};

struct CanyonShoulderEvent {
    CanyonSide side = CanyonSide::Left;
    float centerWorldS = 0.0f;
    float halfLength = 1.0f;
    float amplitude = 0.0f;
};

struct CanyonBoundary {
    float leftWidth = kExplicitCanyonFloorHalfWidth;
    float rightWidth = kExplicitCanyonFloorHalfWidth;
};

struct CanyonRouteFrame {
    float worldS = 0.0f;
    float centerX = 0.0f;
    float centerZ = 0.0f;
    float tangentX = 0.0f;
    float tangentZ = 1.0f;
};

struct CanyonWorldPoint {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct ExplicitCanyonSlice {
    uint32_t segmentId = 0;
    float worldS = 0.0f;
    float centerX = 0.0f;
    float centerZ = 0.0f;
    float tangentX = 0.0f;
    float tangentZ = 1.0f;
    float leftWidth = kExplicitCanyonFloorHalfWidth;
    float rightWidth = kExplicitCanyonFloorHalfWidth;
};

inline constexpr std::size_t kExplicitCanyonEventCapacity = 6;

struct CanyonEventWindow {
    std::array<CanyonShoulderEvent, kExplicitCanyonEventCapacity> events = {};
    uint8_t count = 0;
    bool overflow = false;
};

static_assert(kExplicitCanyonProfileCount == 25, "The reviewed explicit canyon profile has exactly 25 rails");
static_assert(sizeof(CanyonProfileSample) == 8, "Profile samples must remain two packed floats");
static_assert(sizeof(CanyonShoulderEvent) == 16, "Shoulder events must stay within the reviewed memory budget");
static_assert(sizeof(CanyonBoundary) == 8, "A boundary stores only the two gameplay widths");
static_assert(sizeof(CanyonRouteFrame) == 20, "Route frames must remain compact value objects");
static_assert(sizeof(CanyonWorldPoint) == 12, "World points must remain transient XYZ values");
static_assert(sizeof(ExplicitCanyonSlice) == 32, "Each production canyon slice must remain exactly 32 bytes");
static_assert(sizeof(CanyonEventWindow) <= 100, "The six-event window exceeded its reviewed memory budget");

}  // namespace vector_canyon_fighter
