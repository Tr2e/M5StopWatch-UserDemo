#pragma once
#include "museum_renderer.h"
#include <cmath>

namespace gundam_museum {
// One world-to-camera transform for both the exhibit and its cubic room.
struct MuseumCamera {
    float cy,sy,cp,sp,pivot;
    explicit MuseumCamera(const View& v):cy(std::cos(v.yaw)),sy(std::sin(v.yaw)),cp(std::cos(v.pitch)),sp(std::sin(v.pitch)),
        pivot(v.model==ModelId::DestinyGundam?(v.detail?2.78f:1.72f):v.model==ModelId::StrikeGundam?(v.detail?2.78f:1.72f):v.model==ModelId::NuGundam?(v.detail?2.68f:1.65f):v.model==ModelId::Sazabi?(v.detail?2.58f:1.70f):v.model==ModelId::CharZaku?(v.detail?2.55f:1.67f):(v.detail?2.41f:1.49f)){}
    lets_and_go::TrackCameraPoint operator()(Point p,uint8_t=0)const{
        p.y-=pivot;const float x=p.x*cy+p.z*sy,z=p.z*cy-p.x*sy;
        return {x,p.y*cp-z*sp,7.f-(z*cp+p.y*sp)};
    }
    Point eye()const{return {-7*sy*cp,7*sp+pivot,7*cy*cp};}
    static float scale(const View& v){
        return v.model==ModelId::DestinyGundam?(v.detail?148.f:74.f):
            v.model==ModelId::StrikeGundam?(v.detail?148.f:82.f):
            v.model==ModelId::NuGundam?(v.detail?148.f:78.f):
            v.model==ModelId::Sazabi?(v.detail?155.f:76.f):
            v.model==ModelId::CharZaku?(v.detail?158.f:86.f):(v.detail?164.f:98.f);
    }
};
} // namespace gundam_museum
