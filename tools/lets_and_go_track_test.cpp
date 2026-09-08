#include "../main/apps/app_lets_and_go_racer/model/overpass_track.h"
#include "../main/apps/app_lets_and_go_racer/view/track_projection.h"

#include <cmath>
#include <iostream>
#include <array>

namespace {
using namespace lets_and_go;

bool check(bool condition, const char* message)
{
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}

bool validateTrack()
{
    OverpassTrack track;
    bool valid = check(track.length() > 70.0f && track.length() < 100.0f,
                       "unexpected course length");
    const TrackFrame start = track.sample(0.0f);
    const TrackFrame closed = track.sample(track.length());
    valid &= check(trackLength(trackSubtract(start.center, closed.center)) < 0.001f,
                   "course is not position-continuous at closure");
    valid &= check(trackDot(start.tangent, closed.tangent) > 0.999f,
                   "course is not tangent-continuous at closure");

    float maximumStep = 0.0f;
    for (int index = 0; index < 320; ++index) {
        const float distance = track.length() * static_cast<float>(index) / 320.0f;
        const TrackFrame frame = track.sample(distance);
        const TrackFrame next = track.sample(distance + track.length() / 320.0f);
        maximumStep = std::max(maximumStep,
                               trackLength(trackSubtract(next.center, frame.center)));
        valid &= check(std::abs(trackLength(frame.tangent) - 1.0f) < 0.001f,
                       "tangent is not normalized");
        valid &= check(std::abs(trackLength(frame.lateral) - 1.0f) < 0.001f,
                       "lateral is not normalized");
        valid &= check(std::abs(trackDot(frame.tangent, frame.lateral)) < 0.02f,
                       "track frame is not orthogonal");
        valid &= check(std::isfinite(frame.curvature), "curvature is not finite");
    }
    valid &= check(maximumStep < 0.35f, "arc sampling produced a discontinuity");

    const float upperHeight = track.sample(0.0f).center.y;
    const float lowerHeight = track.sample(track.length() * 0.5f).center.y;
    valid &= check(upperHeight - lowerHeight > 3.5f,
                   "overpass layers are not vertically separated");
    valid &= check(track.layer(0.0f) == TrackLayer::Upper &&
                       track.layer(track.length() * 0.5f) == TrackLayer::Lower,
                   "crossing layer classification failed");
    const float width = trackLength(trackSubtract(track.edge(4.0f, 1.0f),
                                                   track.edge(4.0f, -1.0f)));
    valid &= check(width > 3.2f && width < 3.5f, "track width is invalid");
    return valid;
}

bool validateProjection()
{
    const TrackCamera camera = makeTrackLookAtCamera({0.0f, 8.0f, -16.0f},
                                                      {0.0f, 1.5f, 0.0f}, 466, 466);
    TrackCameraPoint behind{0.0f, 0.0f, -1.0f};
    TrackCameraPoint ahead{1.0f, 0.0f, 4.0f};
    bool valid = check(clipTrackSegmentToNear(behind, ahead),
                       "near-plane crossing was rejected");
    valid &= check(std::abs(behind.z - kTrackNearPlane) < 0.0001f,
                   "near-plane endpoint was not clipped");
    TrackScreenPoint screen{};
    valid &= check(projectTrackPoint(camera, ahead, screen) &&
                       std::isfinite(screen.x) && std::isfinite(screen.y),
                   "visible point projection failed");
    TrackCameraPoint hiddenA{0.0f, 0.0f, -2.0f};
    TrackCameraPoint hiddenB{1.0f, 0.0f, 0.1f};
    valid &= check(!clipTrackSegmentToNear(hiddenA, hiddenB),
                   "fully hidden segment survived clipping");
    TrackScreenPoint left{-100000.0f, 220.0f};
    TrackScreenPoint right{100000.0f, 240.0f};
    const bool clippedViewport = clipTrackSegmentToViewport(left, right, 466.0f, 466.0f);
    valid &= check(clippedViewport &&
                       left.x >= -0.01f && right.x <= 466.01f,
                   "viewport crossing was not safely clipped");
    TrackScreenPoint outsideA{-20.0f, -30.0f};
    TrackScreenPoint outsideB{-10.0f, -5.0f};
    valid &= check(!clipTrackSegmentToViewport(outsideA, outsideB, 466.0f, 466.0f),
                   "fully offscreen segment survived viewport clipping");
    return valid;
}

bool validateTriCross()
{
    OverpassTrack track(TrackId::TriCross);
    bool valid=check(track.length()>125.f && track.length()<175.f,"Tri Cross length");
    constexpr int count=720;
    std::array<TrackFrame,count+1> frames{};
    float maxCurvature=0,maxGrade=0,minClearance=100;
    for(int i=0;i<=count;++i) {
        frames[i]=track.sample(track.length()*i/count);
        maxCurvature=std::max(maxCurvature,std::abs(frames[i].curvature));
        maxGrade=std::max(maxGrade,std::abs(frames[i].tangent.y));
        valid &= check(std::isfinite(frames[i].curvature) &&
            std::abs(trackLength(frames[i].tangent)-1)<.001f,"Tri Cross finite normalized frame");
        if(i)valid &= check(trackLength(trackSubtract(frames[i].center,frames[i-1].center))<.3f &&
                             trackDot(frames[i].tangent,frames[i-1].tangent)>.99f,"Tri Cross smooth closure/arc table");
    }
    int crossings=0;
    for(int i=0;i<count;++i)for(int j=i+2;j<count;++j) {
        const int separation=std::min(j-i,count-(j-i));
        if(separation*track.length()/count<6.f)continue;
        const auto a=frames[i].center,b=frames[i+1].center,c=frames[j].center,d=frames[j+1].center;
        if(std::hypot(a.x-c.x,a.z-c.z)<3.7f)minClearance=std::min(minClearance,std::abs(a.y-c.y));
        const float ux=b.x-a.x,uz=b.z-a.z,vx=d.x-c.x,vz=d.z-c.z;
        const float det=ux*vz-uz*vx;
        if(std::abs(det)<1e-7f)continue;
        const float t=((c.x-a.x)*vz-(c.z-a.z)*vx)/det;
        const float s=((c.x-a.x)*uz-(c.z-a.z)*ux)/det;
        if(t>=0 && t<1 && s>=0 && s<1) {
            ++crossings;
            valid &= check(std::abs(a.y+(b.y-a.y)*t-c.y-(d.y-c.y)*s)>4.2f,"crossing vertical clearance");
        }
    }
    valid &= check(crossings==3,"expected exactly three overpasses");
    valid &= check(minClearance>3.f,"nonadjacent road ribbons intersect");
    valid &= check(maxGrade<.5f,"ramp is too steep");
    valid &= check(maxCurvature*OverpassTrack::kHalfWidth<.8f,"inner road edge folds");
    std::cout << "Tri Cross lap length=" << track.length() << '\n';
    track.select(TrackId::SkyLoop);
    valid &= check(track.length()<100.f && track.id()==TrackId::SkyLoop,"track switch left stale arc table");
    std::cout << "Tri Cross: crossings=" << crossings << " clearance=" << minClearance
              << " curvature=" << maxCurvature << " grade=" << maxGrade << '\n';
    return valid;
}
}  // namespace

int main()
{
    return validateTrack() && validateTriCross() && validateProjection() ? 0 : 1;
}
