#include "track_types.h"

#include <cmath>

namespace lets_and_go {

float trackLength(TrackVec3 value)
{
    return std::sqrt(trackDot(value, value));
}

TrackVec3 trackNormalize(TrackVec3 value)
{
    const float length = trackLength(value);
    return length > 0.00001f ? trackScale(value, 1.0f / length)
                            : TrackVec3{0.0f, 0.0f, 1.0f};
}

}  // namespace lets_and_go
