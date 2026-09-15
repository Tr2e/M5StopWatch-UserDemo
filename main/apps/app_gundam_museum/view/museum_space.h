#pragma once
#include "museum_camera.h"
#include "museum_layout.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace gundam_museum::space {
// Light gray room with soft charcoal lines (RGB565).
inline constexpr uint16_t background=0xdefb,grid=0x8410,seam=0x632c,navigation=0x4208;
inline constexpr uint16_t diagnosticBackground=0x0863;
// A true 8 x 8 x 8 cube, over twice the body height, with ample equipment space.
inline constexpr Point lower{-4.f,-.05f,-4.f},upper{4.f,7.95f,4.f};
inline constexpr int divisions=8;

inline float component(Point p,int axis){return axis==0?p.x:axis==1?p.y:p.z;}
inline Point point(int axis,float fixed,int u,float a,int v,float b){
    Point p{};float* values[3]{&p.x,&p.y,&p.z};
    *values[axis]=fixed;*values[u]=a;*values[v]=b;return p;
}
inline bool contains(Point p){
    return p.x>=lower.x && p.x<=upper.x && p.y>=lower.y &&
        p.y<=upper.y && p.z>=lower.z && p.z<=upper.z;
}
// Cut away only walls facing the outside camera. Interior walls remain;
// because the entire model is inside this convex cube, every visible wall
// sample is behind the model on the same camera ray. No front grid overlays.
template<class Emit> void lines(const MuseumCamera& transform,Emit emit){
    const auto eye=transform.eye();
    for(int axis=0;axis<3;++axis)for(int side=0;side<2;++side){
        const float fixed=component(side?upper:lower,axis);
        if((component(eye,axis)-fixed)*(side?1.f:-1.f)>=0)continue;
        const int u=(axis+1)%3,v=(axis+2)%3;
        for(int i=0;i<=divisions;++i){
            const float a=component(lower,u)+(component(upper,u)-component(lower,u))*i/divisions;
            const float b=component(lower,v)+(component(upper,v)-component(lower,v))*i/divisions;
            const uint16_t color=(i==0 || i==divisions)?seam:grid;
            emit(point(axis,fixed,u,a,v,component(lower,v)),point(axis,fixed,u,a,v,component(upper,v)),color);
            emit(point(axis,fixed,u,component(lower,u),v,b),point(axis,fixed,u,component(upper,u),v,b),color);
        }
    }
}
// Clip before converting to integer endpoints: a near-plane crossing must
// never create enormous drawLine loops or overflow. Clip to the full native
// canvas, not the caller's dirty rect, to retain identical line raster phase.
inline bool clipLine(float& x0,float& y0,float& x1,float& y1,int width,int height){
    const float dx=x1-x0,dy=y1-y0;float enter=0,leave=1;
    const auto plane=[&](float p,float q){
        if(std::abs(p)<1e-6f)return q>=0;
        const float t=q/p;
        if(p<0)enter=std::max(enter,t);else leave=std::min(leave,t);
        return enter<=leave;
    };
    if(!plane(-dx,x0)||!plane(dx,width-1-x0)||!plane(-dy,y0)||!plane(dy,height-1-y0))return false;
    x1=x0+leave*dx;y1=y0+leave*dy;x0+=enter*dx;y0+=enter*dy;return true;
}
inline bool projectLine(const MuseumCamera& transform,const lets_and_go::TrackCamera& camera,
                        Point a,Point b,float& x0,float& y0,float& x1,float& y1){
    auto p=transform(a),q=transform(b);
    constexpr float near=lets_and_go::kTrackNearPlane;
    if(p.z<near && q.z<near)return false;
    if((p.z<near)!=(q.z<near)){
        const float t=(near-p.z)/(q.z-p.z);
        const lets_and_go::TrackCameraPoint cut{p.x+(q.x-p.x)*t,p.y+(q.y-p.y)*t,near};
        if(p.z<near)p=cut;else q=cut;
    }
    const auto s=lets_and_go::projectCarSurface(camera,{p.x,p.y,p.z,0,0});
    const auto t=lets_and_go::projectCarSurface(camera,{q.x,q.y,q.z,0,0});
    x0=s.x;y0=s.y;x1=t.x;y1=t.y;
    return std::isfinite(x0)&&std::isfinite(y0)&&std::isfinite(x1)&&std::isfinite(y1);
}
inline void draw(lgfx::LGFXBase& canvas,const View& view){
    const MuseumCamera transform(view);
    lets_and_go::TrackCamera camera{};
    camera.principalX=canvas.width()*.5f;camera.principalY=layout::top+layout::side*.5f;
    camera.focalLength=MuseumCamera::scale(view)*7.f;
    lines(transform,[&](Point a,Point b,uint16_t color){
        float x0,y0,x1,y1;
        if(projectLine(transform,camera,a,b,x0,y0,x1,y1) &&
           clipLine(x0,y0,x1,y1,canvas.width(),canvas.height()))
            canvas.drawLine(int(std::lround(x0)),int(std::lround(y0)),
                            int(std::lround(x1)),int(std::lround(y1)),color);
    });
}
} // namespace gundam_museum::space
