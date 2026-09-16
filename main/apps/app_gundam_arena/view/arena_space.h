#pragma once
#include "../../app_gundam_museum/view/museum_layout.h"
#include "../../app_gundam_museum/view/museum_space.h"
#include "../model/rx78_bones.h"
#include <cmath>

namespace gundam_arena {
struct ArenaView {
    float orbit=.42f,pitch=.20f;
    float lookX=0,lookY=1.18f,lookZ=0,camYaw=kPi+.42f;
    float distance=9.2f;
    int percent=70;
    uint8_t padHint=0;
};

struct ArenaCamera {
    float cy,sy,cp,sp,lookX,lookY,lookZ,distance;
    explicit ArenaCamera(const ArenaView& v):
        cy(std::cos(v.camYaw)),sy(std::sin(v.camYaw)),
        cp(std::cos(v.pitch)),sp(std::sin(v.pitch)),
        lookX(v.lookX),lookY(v.lookY),lookZ(v.lookZ),distance(v.distance){}
    lets_and_go::TrackCameraPoint operator()(Point p,uint8_t=0)const{
        p.x-=lookX;p.y-=lookY;p.z-=lookZ;
        const float x=p.x*cy+p.z*sy,z=p.z*cy-p.x*sy;
        return {x,p.y*cp-z*sp,distance-(z*cp+p.y*sp)};
    }
    Point eye()const{
        return {lookX-distance*sy*cp,lookY+distance*sp,lookZ+distance*cy*cp};
    }
};

namespace space {
inline constexpr uint16_t background=0x0000,grid=0xc618,seam=0xce79,navigation=0xc618;
inline constexpr float kHalf=20.f,kFocal=88.f;
inline constexpr int kDiv=32;

inline lets_and_go::TrackCamera canvasCamera(const lgfx::LGFXBase& canvas){
    lets_and_go::TrackCamera camera{};
    camera.principalX=canvas.width()*.5f;
    camera.principalY=gundam_museum::layout::top+gundam_museum::layout::side*.5f;
    camera.focalLength=kFocal*7.f;
    return camera;
}

template<class Transform>
inline void draw(lgfx::LGFXBase& canvas,const Transform& transform,const lets_and_go::TrackCamera& camera){
    const auto emit=[&](Point a,Point b,uint16_t color){
        auto p=transform(a),q=transform(b);
        constexpr float near=lets_and_go::kTrackNearPlane;
        if(p.z<near && q.z<near)return;
        if((p.z<near)!=(q.z<near)){
            const float t=(near-p.z)/(q.z-p.z);
            const lets_and_go::TrackCameraPoint cut{p.x+(q.x-p.x)*t,p.y+(q.y-p.y)*t,near};
            if(p.z<near)p=cut;else q=cut;
        }
        const auto s=lets_and_go::projectCarSurface(camera,{p.x,p.y,p.z,0,0});
        const auto t=lets_and_go::projectCarSurface(camera,{q.x,q.y,q.z,0,0});
        float x0=s.x,y0=s.y,x1=t.x,y1=t.y;
        if(!std::isfinite(x0)||!std::isfinite(y0)||!std::isfinite(x1)||!std::isfinite(y1))return;
        if(!gundam_museum::space::clipLine(x0,y0,x1,y1,canvas.width(),canvas.height()))return;
        canvas.drawLine(int(std::lround(x0)),int(std::lround(y0)),
                        int(std::lround(x1)),int(std::lround(y1)),color);
    };
    for(int i=0;i<=kDiv;++i){
        const float a=-kHalf+(2.f*kHalf)*i/kDiv;
        const uint16_t color=(i==0||i==kDiv)?seam:grid;
        emit({a,0.f,-kHalf},{a,0.f,kHalf},color);
        emit({-kHalf,0.f,a},{kHalf,0.f,a},color);
    }
}
} // namespace space
} // namespace gundam_arena
