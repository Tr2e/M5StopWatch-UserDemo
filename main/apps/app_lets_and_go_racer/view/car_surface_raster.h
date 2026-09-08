#pragma once
#include "car_paint.h"
#include "pencil_scene.h"
#include <array>
#include <cmath>
#include <algorithm>

namespace lets_and_go {
struct CarSurfaceVertex {float x,y,z,u,v;};
struct CarScreenVertex {float x,y,depth,uDepth,vDepth;};

// A bounded car-sized RGB565/depth tile, not a second full-screen framebuffer.
// Depth is inverse-camera-z in Q13 (near plane .2 => 40960, within uint16_t).
template<int Width,int Height> class CarSurfaceRaster {
public:
    static constexpr int kWidth=Width,kHeight=Height;
    void begin(int x,int y) {
        _x=x;_y=y;
        std::fill(_depth.begin(),_depth.end(),uint16_t(0));
    }
    void triangle(CarScreenVertex a,CarScreenVertex b,CarScreenVertex c,
                  uint16_t color,CarPaint paint,uint8_t light) {
        const float det=(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
        if(!std::isfinite(det) || std::abs(det)<.001f)return;
        const float left=std::max(float(_x),std::floor(std::min({a.x,b.x,c.x})));
        const float right=std::min(float(_x+Width-1),std::ceil(std::max({a.x,b.x,c.x})));
        const float top=std::max(float(_y),std::floor(std::min({a.y,b.y,c.y})));
        const float bottom=std::min(float(_y+Height-1),std::ceil(std::max({a.y,b.y,c.y})));
        if(left>right || top>bottom)return;
        const int x0=int(left),x1=int(right),y0=int(top),y1=int(bottom);
        const float inverse=1/det;
        for(int y=y0;y<=y1;++y) for(int x=x0;x<=x1;++x) {
            const float px=x+.5f-a.x,py=y+.5f-a.y;
            const float s=(px*(c.y-a.y)-py*(c.x-a.x))*inverse;
            const float t=((b.x-a.x)*py-(b.y-a.y)*px)*inverse;
            if(s<-.00001f || t<-.00001f || s+t>1.00001f)continue;
            const float depth=a.depth+s*(b.depth-a.depth)+t*(c.depth-a.depth);
            if(!(depth>0) || !std::isfinite(depth))continue;
            const auto d=uint16_t(std::clamp(depth*8192.f,1.f,65535.f));
            const auto index=std::size_t(y-_y)*Width+(x-_x);
            if(d<_depth[index])continue;
            uint16_t pigment=color;
            if(paint!=CarPaint::Solid) {
                const float u=(a.uDepth+s*(b.uDepth-a.uDepth)+t*(c.uDepth-a.uDepth))/depth;
                const float v=(a.vDepth+s*(b.vDepth-a.vDepth)+t*(c.vDepth-a.vDepth))/depth;
                pigment=carPaintColor(paint,color,std::clamp(u,0.f,1.f),std::clamp(v,0.f,1.f));
            }
            _depth[index]=d;
            _color[index]=light==255 ? pigment : carTint(pigment,light/255.f);
        }
    }
    void cameraTriangle(const TrackCamera& camera,CarSurfaceVertex a,CarSurfaceVertex b,
                        CarSurfaceVertex c,uint16_t color,CarPaint paint,uint8_t light) {
        const std::array<CarSurfaceVertex,3> input{{a,b,c}};
        std::array<CarSurfaceVertex,4> output{};
        std::size_t count=0;
        for(std::size_t i=0;i<3;++i) {
            const auto p=input[i],q=input[(i+1)%3];
            if(p.z>=kTrackNearPlane)output[count++]=p;
            if((p.z>=kTrackNearPlane)!=(q.z>=kTrackNearPlane)) {
                const float t=(kTrackNearPlane-p.z)/(q.z-p.z);
                output[count++]={p.x+(q.x-p.x)*t,p.y+(q.y-p.y)*t,kTrackNearPlane,
                    p.u+(q.u-p.u)*t,p.v+(q.v-p.v)*t};
            }
        }
        if(count<3)return;
        const auto project=[&](CarSurfaceVertex p) {
            const float inverse=1/p.z;
            return CarScreenVertex{camera.principalX+camera.focalLength*p.x*inverse,
                camera.principalY-camera.focalLength*p.y*inverse,inverse,p.u*inverse,p.v*inverse};
        };
        for(std::size_t i=1;i+1<count;++i)
            triangle(project(output[0]),project(output[i]),project(output[i+1]),color,paint,light);
    }
    template<class Transform> void panel(const TrackCamera& camera,const CarPanel& face,Transform transform) {
        std::array<CarSurfaceVertex,4> v{};
        for(std::size_t i=0;i<4;++i) {
            const auto p=transform(face.point[i],face.wheel);
            v[i]={p.x,p.y,p.z,(i==0 || i==3 ? face.u0 : face.u1)/255.f,
                  (i<2 ? face.v0 : face.v1)/255.f};
        }
        cameraTriangle(camera,v[0],v[1],v[2],face.color,face.paint,face.light);
        cameraTriangle(camera,v[0],v[2],v[3],face.color,face.paint,face.light);
    }
    void blit(LGFX_Sprite& canvas,const PencilOcclusion* occlusion=nullptr) const {
        std::array<uint16_t,PencilTrack::kSegments*4> candidates{};
        std::size_t count=0;
        if(occlusion) for(std::size_t i=0;i<occlusion->count;++i) {
            const auto& s=occlusion->surfaces[i];
            if(s.maxX>=_x && s.minX<_x+Width && s.maxY>=_y && s.minY<_y+Height)
                candidates[count++]=uint16_t(i);
        }
        for(int y=0;y<Height;++y) {
            int start=-1;uint16_t color=0;
            for(int x=0;x<=Width;++x) {
                const auto index=std::size_t(y)*Width+x;
                bool visible=x<Width && _depth[index]!=0;
                if(visible && occlusion) {
                    const float inverse=_depth[index]/8192.f;
                    const TrackScreenPoint p{_x+x+.5f,_y+y+.5f};
                    for(std::size_t s=0;s<count;++s) {
                        float from,to;
                        if(pencilHiddenInterval(occlusion->surfaces[candidates[s]],p,p,
                                                inverse,inverse,from,to)) {visible=false;break;}
                    }
                }
                if(start>=0 && (!visible || _color[index]!=color)) {
                    canvas.drawLine(_x+start,_y+y,_x+x-1,_y+y,color);start=-1;
                }
                if(visible && start<0) {start=x;color=_color[index];}
            }
        }
    }
    uint16_t depthAt(int x,int y) const {return _depth[std::size_t(y)*Width+x];}
private:
    std::array<uint16_t,Width*Height> _depth{};
    std::array<uint16_t,Width*Height> _color{};
    int _x=0,_y=0;
};
} // namespace lets_and_go
