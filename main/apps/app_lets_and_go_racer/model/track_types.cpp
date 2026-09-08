#include "track_types.h"

#include <cmath>

namespace lets_and_go {

TrackVec3 trackAdd(TrackVec3 left, TrackVec3 right)
{
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

TrackVec3 trackSubtract(TrackVec3 left, TrackVec3 right)
{
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

TrackVec3 trackScale(TrackVec3 value, float scale)
{
    return {value.x * scale, value.y * scale, value.z * scale};
}

float trackDot(TrackVec3 left, TrackVec3 right)
{
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

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
