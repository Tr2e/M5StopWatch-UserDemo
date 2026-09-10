#include "overpass_track.h"
#include "grand_spiral_data.h"
#include "../lets_and_go_config.h"

#include <algorithm>
#include <cmath>

namespace lets_and_go {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTau = 2.0f * kPi;
constexpr float kCourseRadius = tuning::kCourseRadius;
constexpr float kLowerHeight = tuning::kCourseBaseHeight;
constexpr float kOverpassRise = tuning::kOverpassRise;
constexpr float kTriCrossLift = 2.2f;

float wrapDistance(float distance, float length)
{
    if (!(length > 0.0f) || !std::isfinite(distance)) return 0.0f;
    distance = std::fmod(distance, length);
    return distance < 0.0f ? distance + length : distance;
}

}  // namespace

TrackVec3 OverpassTrack::centerAtParameter(float parameter) const
{
    if (_id == TrackId::TriCross) {
        // A (2,3) torus-knot centreline: three planar crossings, with each
        // upper/lower pair separated by 4.4 units and zero grade at crossings.
        const float radius = 10.f + 4.f * std::cos(3.f * parameter);
        return {radius * std::cos(2.f * parameter),
                kLowerHeight + kTriCrossLift * (1.f + std::sin(3.f * parameter)),
                radius * std::sin(2.f * parameter)};
    }
    // A figure-eight has one planar crossing. The cosine lift separates the
    // t=0 upper deck from the t=pi lower deck without a discontinuity.
    return {
        kCourseRadius * std::sin(parameter),
        kLowerHeight + 0.5f * kOverpassRise * (1.0f + std::cos(parameter)),
        0.68f * kCourseRadius * std::sin(2.0f * parameter),
    };
}

TrackVec3 OverpassTrack::derivativeAtParameter(float parameter) const
{
    if (_id == TrackId::TriCross) {
        const float radius = 10.f + 4.f * std::cos(3.f * parameter);
        const float radial = -12.f * std::sin(3.f * parameter);
        return {radial * std::cos(2.f * parameter) - 2.f * radius * std::sin(2.f * parameter),
                3.f * kTriCrossLift * std::cos(3.f * parameter),
                radial * std::sin(2.f * parameter) + 2.f * radius * std::cos(2.f * parameter)};
    }
    return {
        kCourseRadius * std::cos(parameter),
        -0.5f * kOverpassRise * std::sin(parameter),
        1.36f * kCourseRadius * std::cos(2.0f * parameter),
    };
}

OverpassTrack::OverpassTrack(TrackId id)
{
    select(id);
}

void OverpassTrack::select(TrackId id)
{
    _id = isValidTrack(id) ? id : TrackId::SkyLoop;
    if (_id==TrackId::GrandSpiral) { _length=grand_spiral::kLength; return; }
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
    if (_id==TrackId::GrandSpiral) {
        constexpr float step=grand_spiral::kLength/(grand_spiral::kNodes.size()-1);
        const float index=wrapped/step;
        const auto i=std::min(std::size_t(index),grand_spiral::kNodes.size()-2);
        const float t=std::clamp(index-float(i),0.f,1.f),t2=t*t,t3=t2*t;
        const auto& a=grand_spiral::kNodes[i];const auto& b=grand_spiral::kNodes[i+1];
        const auto center=trackAdd(trackAdd(trackScale(a.point,2*t3-3*t2+1),trackScale(b.point,-2*t3+3*t2)),
            trackAdd(trackScale(a.tangent,step*(t3-2*t2+t)),trackScale(b.tangent,step*(t3-t2))));
        const auto velocity=trackAdd(trackAdd(trackScale(a.point,(6*t2-6*t)/step),trackScale(b.point,(-6*t2+6*t)/step)),
            trackAdd(trackScale(a.tangent,3*t2-4*t+1),trackScale(b.tangent,3*t2-2*t)));
        const auto tangent=trackNormalize(velocity);
        const auto lateral=trackNormalize({tangent.z,0,-tangent.x});
        const float curvature=a.curvature+(b.curvature-a.curvature)*t;
        return {center,tangent,lateral,kHalfWidth,curvature,std::clamp(curvature*1.8f,-.20f,.20f),wrapped};
    }
    const float parameter = parameterAtDistance(wrapped);
    const TrackVec3 center = centerAtParameter(parameter);
    const TrackVec3 derivative = derivativeAtParameter(parameter);
    const TrackVec3 tangent = trackNormalize(derivative);
    const TrackVec3 lateral = trackNormalize({tangent.z, 0.0f, -tangent.x});

    constexpr float kCurvatureProbe = 0.025f;
    auto beforeDerivative = derivativeAtParameter(parameter - kCurvatureProbe);
    auto afterDerivative = derivativeAtParameter(parameter + kCurvatureProbe);
    // Hill crests must not add fictitious lateral force on the new course.
    if (_id == TrackId::TriCross) beforeDerivative.y = afterDerivative.y = 0.f;
    const TrackVec3 before = trackNormalize(beforeDerivative);
    const TrackVec3 after = trackNormalize(afterDerivative);
    const float turnSign = before.x * after.z - before.z * after.x;
    // Preserve SKY LOOP's tuned handling; the new parameter runs around twice
    // and must use its actual local speed, not the old fixed-radius divisor.
    const float metric = _id == TrackId::TriCross
        ? std::hypot(derivative.x, derivative.z) : kCourseRadius;
    const float curvature = std::copysign(
        trackLength(trackSubtract(after, before)) / (2.0f * kCurvatureProbe * metric),
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
    const float rise = _id==TrackId::GrandSpiral ? 16.f : _id == TrackId::TriCross ? 2.f * kTriCrossLift : kOverpassRise;
    if (height > kLowerHeight + rise - 0.9f) return TrackLayer::Upper;
    return TrackLayer::Transition;
}

const char* overpassTrackName(TrackId id)
{
    return id==TrackId::GrandSpiral ? "GRAND SPIRAL 03" : id == TrackId::TriCross ? "TRI CROSS 02" : "SKY LOOP 01";
}

}  // namespace lets_and_go
