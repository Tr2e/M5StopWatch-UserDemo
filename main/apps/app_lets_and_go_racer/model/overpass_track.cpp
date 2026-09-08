#include "overpass_track.h"

#include <algorithm>
#include <cmath>

namespace lets_and_go {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTau = 2.0f * kPi;
constexpr float kCourseRadius = 12.0f;
constexpr float kLowerHeight = 0.35f;
constexpr float kOverpassRise = 3.8f;

float wrapDistance(float distance, float length)
{
    if (!(length > 0.0f) || !std::isfinite(distance)) return 0.0f;
    distance = std::fmod(distance, length);
    return distance < 0.0f ? distance + length : distance;
}

}  // namespace

TrackVec3 OverpassTrack::centerAtParameter(float parameter)
{
    // A figure-eight has one planar crossing. The cosine lift separates the
    // t=0 upper deck from the t=pi lower deck without a discontinuity.
    return {
        kCourseRadius * std::sin(parameter),
        kLowerHeight + 0.5f * kOverpassRise * (1.0f + std::cos(parameter)),
        0.68f * kCourseRadius * std::sin(2.0f * parameter),
    };
}

TrackVec3 OverpassTrack::derivativeAtParameter(float parameter)
{
    return {
        kCourseRadius * std::cos(parameter),
        -0.5f * kOverpassRise * std::sin(parameter),
        1.36f * kCourseRadius * std::cos(2.0f * parameter),
    };
}

OverpassTrack::OverpassTrack()
{
    TrackVec3 previous = centerAtParameter(0.0f);
    _arcLength[0] = 0.0f;
    for (std::size_t index = 1; index <= kArcTableSegments; ++index) {
        const float parameter = kTau * static_cast<float>(index) /
                                static_cast<float>(kArcTableSegments);
        const TrackVec3 current = centerAtParameter(parameter);
        _arcLength[index] = _arcLength[index - 1u] +
                            trackLength(trackSubtract(current, previous));
        previous = current;
    }
    _length = _arcLength.back();
}

float OverpassTrack::parameterAtDistance(float distance) const
{
    const float wrapped = wrapDistance(distance, _length);
    const auto upper = std::upper_bound(_arcLength.begin(), _arcLength.end(), wrapped);
    const std::size_t next = static_cast<std::size_t>(upper - _arcLength.begin());
    const std::size_t index = next == 0u ? 0u : next - 1u;
    const std::size_t clampedNext = std::min(index + 1u, kArcTableSegments);
    const float span = _arcLength[clampedNext] - _arcLength[index];
    const float blend = span > 0.00001f ? (wrapped - _arcLength[index]) / span : 0.0f;
    return kTau * (static_cast<float>(index) + blend) /
           static_cast<float>(kArcTableSegments);
}

TrackFrame OverpassTrack::sample(float distance) const
{
    const float wrapped = wrapDistance(distance, _length);
    const float parameter = parameterAtDistance(wrapped);
    const TrackVec3 center = centerAtParameter(parameter);
    const TrackVec3 tangent = trackNormalize(derivativeAtParameter(parameter));
    const TrackVec3 lateral = trackNormalize({tangent.z, 0.0f, -tangent.x});

    constexpr float kCurvatureProbe = 0.025f;
    const TrackVec3 before = trackNormalize(derivativeAtParameter(parameter - kCurvatureProbe));
    const TrackVec3 after = trackNormalize(derivativeAtParameter(parameter + kCurvatureProbe));
    const float turnSign = before.x * after.z - before.z * after.x;
    const float curvature = std::copysign(
        trackLength(trackSubtract(after, before)) / (2.0f * kCurvatureProbe * kCourseRadius),
        turnSign);
    const float bank = std::clamp(curvature * 1.8f, -0.20f, 0.20f);
    return {center, tangent, lateral, kHalfWidth, curvature, bank, wrapped};
}

TrackVec3 OverpassTrack::edge(float distance, float normalizedLateral) const
{
    const TrackFrame frame = sample(distance);
    const float amount = std::clamp(normalizedLateral, -1.0f, 1.0f) * frame.halfWidth;
    TrackVec3 point = trackAdd(frame.center, trackScale(frame.lateral, amount));
    point.y += std::sin(frame.bankRadians) * amount;
    return point;
}

TrackLayer OverpassTrack::layer(float distance) const
{
    const float height = sample(distance).center.y;
    if (height < kLowerHeight + 0.9f) return TrackLayer::Lower;
    if (height > kLowerHeight + kOverpassRise - 0.9f) return TrackLayer::Upper;
    return TrackLayer::Transition;
}

const char* overpassTrackName()
{
    return "SKY LOOP 01";
}

}  // namespace lets_and_go
