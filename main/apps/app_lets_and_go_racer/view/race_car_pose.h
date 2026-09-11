#pragma once

#include "pencil_scene.h"
#include "../model/car_display_mesh.h"
#include "../model/race_state.h"

#include <algorithm>
#include <cmath>

namespace lets_and_go {

inline constexpr float kRaceCarWorldScale=.34f;
inline constexpr float kRaceCarRoadClearance=.015f;

struct RaceCarPose {
    TrackVec3 base;
    TrackVec3 lateral;
    TrackVec3 forward;
    TrackVec3 up;
    float supportLift=0;
};

inline RaceCarPose makeRaceCarPose(const OverpassTrack& track,const PencilTrack& road,
                                   const RaceCarSnapshot& car)
{
    const auto frame=track.sample(car.motion.distance);
    constexpr float front=kModelFrontAxle*kRaceCarWorldScale;
    constexpr float rear=kModelRearAxle*kRaceCarWorldScale;
    const auto frontRoad=road.roadPoint(track,car.motion.distance+front,car.motion.lateralOffset);
    const auto rearRoad=road.roadPoint(track,car.motion.distance+rear,car.motion.lateralOffset);

    const float axleBlend=-rear/(front-rear);
    RaceCarPose pose;
    pose.base=trackAdd(rearRoad,trackScale(trackSubtract(frontRoad,rearRoad),axleBlend));
    const auto roadRight=trackNormalize(trackAdd(frame.lateral,
        {0,std::sin(frame.bankRadians),0}));
    auto forward=trackNormalize(trackSubtract(frontRoad,rearRoad));
    forward=trackNormalize(trackSubtract(forward,trackScale(roadRight,trackDot(forward,roadRight))));
    pose.up=trackNormalize(trackCross(forward,roadRight));

    const float cosine=std::cos(car.motion.headingOffset);
    const float sine=std::sin(car.motion.headingOffset);
    pose.lateral=trackAdd(trackScale(roadRight,cosine),trackScale(forward,-sine));
    pose.forward=trackAdd(trackScale(forward,cosine),trackScale(roadRight,sine));

    // A steered rigid car can span two road triangles and two bank samples.
    // Lift it only enough for all four tire contact points to clear the exact
    // rendered deck. The existing 0.015-unit clearance also stays above the
    // slightly raised start/finish decal.
    for(float modelX:{-.553f,.553f}) for(float modelZ:{kModelRearAxle,kModelFrontAxle}) {
        const float x=-modelX*kRaceCarWorldScale,z=modelZ*kRaceCarWorldScale;
        auto contact=trackAdd(pose.base,trackAdd(trackScale(pose.lateral,x),trackScale(pose.forward,z)));
        contact=trackAdd(contact,trackScale(pose.up,kRaceCarRoadClearance));
        const float longitudinal=-x*sine+z*cosine;
        const float lateral=x*cosine+z*sine;
        const auto support=road.roadPoint(track,car.motion.distance+longitudinal,
                                         car.motion.lateralOffset+lateral);
        pose.supportLift=std::max(pose.supportLift,kRaceCarRoadClearance+
            trackDot(trackSubtract(support,contact),pose.up));
    }
    pose.supportLift=std::max(0.f,pose.supportLift);
    pose.base=trackAdd(pose.base,trackScale(pose.up,pose.supportLift));
    return pose;
}

inline TrackVec3 raceCarPointToWorld(CarPoint point,const RaceCarPose& pose)
{
    point=carPointInTrackBasis(point);
    auto result=pose.base;
    result=trackAdd(result,trackScale(pose.lateral,point.x*kRaceCarWorldScale));
    result=trackAdd(result,trackScale(pose.forward,point.z*kRaceCarWorldScale));
    return trackAdd(result,trackScale(pose.up,kRaceCarRoadClearance+point.y*kRaceCarWorldScale));
}

} // namespace lets_and_go
