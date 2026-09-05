#pragma once

#include "explicit_canyon_types.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace vector_canyon_fighter {

class ExplicitCanyonStream {
public:
    static constexpr std::size_t kSliceCount = 34;
    static constexpr std::size_t kFarSliceCount = 2;
    static constexpr std::size_t kProfileCount = kExplicitCanyonProfileCount;
    static constexpr float kSliceSpacing = 1.118690f;
    static constexpr float kFarSliceSpacing = 28.0f;
    static constexpr float kForwardDistanceScale = 0.018f;

    void reset(uint32_t seed);
    void resetStraightBaseline();
    void resetCurvedBaseline(uint32_t seed);
    void update(float flightForwardDistance);

    const std::array<ExplicitCanyonSlice, kSliceCount>& slices() const { return _slices; }
    const std::array<ExplicitCanyonSlice, kFarSliceCount>& farSlices() const { return _farSlices; }
    const CanyonEventWindow& eventWindow() const { return _eventWindow; }
    const std::array<CanyonProfileSample, kProfileCount>& profile() const { return kExplicitCanyonProfile; }

    CanyonWorldPoint worldPoint(std::size_t slice, std::size_t profilePoint) const;
    CanyonWorldPoint farWorldPoint(std::size_t slice, std::size_t profilePoint) const;
    CanyonBoundary boundaryAt(float worldS) const;
    CanyonRouteFrame routeFrameAt(float worldS) const;
    CanyonShoulderEvent eventAtIndex(uint32_t eventIndex) const;

    float playerWorldS() const { return _playerWorldS; }
    uint32_t firstSegment() const { return _firstSegment; }
    uint32_t seed() const { return _seed; }

private:
    enum class Mode : uint8_t {
        Production,
        StraightBaseline,
        CurvedBaseline,
    };

    ExplicitCanyonSlice makeSlice(uint32_t segment) const;
    ExplicitCanyonSlice makeSliceAtRouteParameter(uint32_t segment, float routeParameter) const;
    CanyonRouteFrame calculateRouteFrame(float worldS) const;
    void rebuildSlices(uint32_t firstSegment);
    void refreshFarSlices();
    void refreshEventWindow();

    std::array<ExplicitCanyonSlice, kSliceCount> _slices = {};
    std::array<ExplicitCanyonSlice, kFarSliceCount> _farSlices = {};
    CanyonEventWindow _eventWindow = {};
    uint32_t _seed = 0;
    uint32_t _firstSegment = 0;
    float _playerWorldS = 0.0f;
    float _backRouteParameter = 0.0f;
    Mode _mode = Mode::Production;
};

static_assert(sizeof(ExplicitCanyonStream) <= 1280,
              "Explicit canyon model exceeded the M0 fixed-memory target");

}  // namespace vector_canyon_fighter
