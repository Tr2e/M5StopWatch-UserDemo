#pragma once
#include "museum_renderer.h"
#include <cmath>

namespace gundam_museum {
// One world-to-camera transform for both the exhibit and its cubic room.
struct MuseumCamera {
    float cy,sy,cp,sp,pivot;
    explicit MuseumCamera(const View& v):cy(std::cos(v.yaw)),sy(std::sin(v.yaw)),cp(std::cos(v.pitch)),sp(std::sin(v.pitch)),
        pivot(v.detail?2.41f:1.49f){}
    lets_and_go::TrackCameraPoint operator()(Point p,uint8_t=0)const{
        p.y-=pivot;const float x=p.x*cy+p.z*sy,z=p.z*cy-p.x*sy;
        return {x,p.y*cp-z*sp,7.f-(z*cp+p.y*sp)};
    }
    Point eye()const{return {-7*sy*cp,7*sp+pivot,7*cy*cp};}
    static float scale(const View& v){
        return v.detail?164.f:98.f;
    }
};
} // namespace gundam_museum
