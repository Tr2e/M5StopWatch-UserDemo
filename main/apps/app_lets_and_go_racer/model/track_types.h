#pragma once

#include <cstdint>

namespace lets_and_go {

struct TrackVec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct TrackFrame {
    TrackVec3 center;
    TrackVec3 tangent;
    TrackVec3 lateral;
    float halfWidth = 0.0f;
    float curvature = 0.0f;
    float bankRadians = 0.0f;
    float distance = 0.0f;
};

enum class TrackLayer : uint8_t {
    Lower,
    Transition,
    Upper,
};

TrackVec3 trackAdd(TrackVec3 left, TrackVec3 right);
TrackVec3 trackSubtract(TrackVec3 left, TrackVec3 right);
TrackVec3 trackScale(TrackVec3 value, float scale);
float trackDot(TrackVec3 left, TrackVec3 right);
float trackLength(TrackVec3 value);
TrackVec3 trackNormalize(TrackVec3 value);

}  // namespace lets_and_go
