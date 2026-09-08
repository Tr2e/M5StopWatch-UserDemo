#pragma once

#include "track_types.h"
#include "game_types.h"

#include <array>
#include <cstddef>

namespace lets_and_go {

class OverpassTrack {
public:
    static constexpr std::size_t kArcTableSegments = 160u;
    static constexpr float kHalfWidth = 1.65f;

    explicit OverpassTrack(TrackId id = TrackId::SkyLoop);
    void select(TrackId id);
    TrackId id() const { return _id; }

    float length() const { return _length; }
    TrackFrame sample(float distance) const;
    TrackVec3 edge(float distance, float normalizedLateral) const;
    TrackLayer layer(float distance) const;

private:
    TrackVec3 centerAtParameter(float parameter) const;
    TrackVec3 derivativeAtParameter(float parameter) const;
    float parameterAtDistance(float distance) const;

    std::array<float, kArcTableSegments + 1u> _arcLength{};
    float _length = 0.0f;
    TrackId _id = TrackId::SkyLoop;
};

const char* overpassTrackName(TrackId id = TrackId::SkyLoop);

}  // namespace lets_and_go
