#pragma once

#include "track_types.h"

#include <array>
#include <cstddef>

namespace lets_and_go {

class OverpassTrack {
public:
    static constexpr std::size_t kArcTableSegments = 160u;
    static constexpr float kHalfWidth = 1.65f;

    OverpassTrack();

    float length() const { return _length; }
    TrackFrame sample(float distance) const;
    TrackVec3 edge(float distance, float normalizedLateral) const;
    TrackLayer layer(float distance) const;

private:
    static TrackVec3 centerAtParameter(float parameter);
    static TrackVec3 derivativeAtParameter(float parameter);
    float parameterAtDistance(float distance) const;

    std::array<float, kArcTableSegments + 1u> _arcLength{};
    float _length = 0.0f;
};

const char* overpassTrackName();

}  // namespace lets_and_go
