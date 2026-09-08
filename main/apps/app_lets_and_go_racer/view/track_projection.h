#pragma once

#include "../model/track_types.h"

#include <algorithm>
#include <cmath>

namespace lets_and_go {

inline constexpr float kTrackNearPlane = 0.20f;

struct TrackCameraPoint {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct TrackScreenPoint {
    float x = 0.0f;
    float y = 0.0f;
};

struct TrackCamera {
    TrackVec3 position;
    TrackVec3 right;
    TrackVec3 up;
    TrackVec3 forward;
    float principalX = 0.0f;
    float principalY = 0.0f;
    float focalLength = 0.0f;
};

inline TrackVec3 trackCross(TrackVec3 left, TrackVec3 right)
{
    return {left.y * right.z - left.z * right.y,
            left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x};
}

inline TrackCamera makeTrackLookAtCamera(TrackVec3 position, TrackVec3 target,
                                         int width, int height, float focalRatio = 0.78f)
{
    const TrackVec3 forward = trackNormalize(trackSubtract(target, position));
    const TrackVec3 right = trackNormalize(trackCross({0.0f, 1.0f, 0.0f}, forward));
    const TrackVec3 up = trackNormalize(trackCross(forward, right));
    return {position, right, up, forward, static_cast<float>(width) * 0.5f,
            static_cast<float>(height) * 0.50f,
            static_cast<float>(width) * focalRatio};
}

inline TrackCameraPoint trackToCamera(const TrackCamera& camera, TrackVec3 world)
{
    const TrackVec3 relative = trackSubtract(world, camera.position);
    return {trackDot(relative, camera.right), trackDot(relative, camera.up),
            trackDot(relative, camera.forward)};
}

inline bool clipTrackSegmentToNear(TrackCameraPoint& from, TrackCameraPoint& to)
{
    const bool fromVisible = from.z >= kTrackNearPlane;
    const bool toVisible = to.z >= kTrackNearPlane;
    if (!fromVisible && !toVisible) return false;
    if (fromVisible && toVisible) return true;
    TrackCameraPoint& behind = fromVisible ? to : from;
    const TrackCameraPoint& ahead = fromVisible ? from : to;
    const float denominator = ahead.z - behind.z;
    if (std::abs(denominator) <= 0.00001f) return false;
    const float blend = std::clamp((kTrackNearPlane - behind.z) / denominator,
                                   0.0f, 1.0f);
    behind.x += (ahead.x - behind.x) * blend;
    behind.y += (ahead.y - behind.y) * blend;
    behind.z = kTrackNearPlane;
    return true;
}

inline bool projectTrackPoint(const TrackCamera& camera, TrackCameraPoint point,
                              TrackScreenPoint& screen)
{
    if (point.z < kTrackNearPlane) return false;
    screen.x = camera.principalX + camera.focalLength * point.x / point.z;
    screen.y = camera.principalY - camera.focalLength * point.y / point.z;
    return std::isfinite(screen.x) && std::isfinite(screen.y);
}

inline bool clipTrackSegmentToViewport(TrackScreenPoint& from, TrackScreenPoint& to,
                                       float width, float height, float guard = 0.0f)
{
    const float minimumX = -guard;
    const float minimumY = -guard;
    const float maximumX = width + guard;
    const float maximumY = height + guard;
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    float enter = 0.0f;
    float leave = 1.0f;
    const auto clipBoundary = [&](float p, float q) {
        if (std::abs(p) <= 0.00001f) return q >= 0.0f;
        const float ratio = q / p;
        if (p < 0.0f) {
            if (ratio > leave) return false;
            enter = std::max(enter, ratio);
        } else {
            if (ratio < enter) return false;
            leave = std::min(leave, ratio);
        }
        return true;
    };
    if (!clipBoundary(-dx, from.x - minimumX) ||
        !clipBoundary(dx, maximumX - from.x) ||
        !clipBoundary(-dy, from.y - minimumY) ||
        !clipBoundary(dy, maximumY - from.y)) {
        return false;
    }
    const TrackScreenPoint original = from;
    from = {original.x + dx * enter, original.y + dy * enter};
    to = {original.x + dx * leave, original.y + dy * leave};
    return true;
}

}  // namespace lets_and_go
