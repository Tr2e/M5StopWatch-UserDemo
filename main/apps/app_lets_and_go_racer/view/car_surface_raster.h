#pragma once
#include "car_paint.h"
#include "race_paint_atlas.h"
#include "pencil_scene.h"
#include "color_span.h"
#include "render_scratch.h"
#include <array>
#include <cmath>
#include <algorithm>
#include <new>

namespace lets_and_go {
struct CarBlitWork {
    uint32_t tileCandidates=0,rowCandidates=0,unfilteredRowCandidates=0,rows=0;
};

struct CarSurfaceVertex {float x,y,z,u,v;};
struct CarScreenVertex {float x,y,depth,uDepth,vDepth;};

struct PreparedCarPanel {
    union {
        std::array<CarSurfaceVertex,4> camera{};
        std::array<CarScreenVertex,4> screen;
    };
    float left=0,right=0,top=0,bottom=0;
    uint16_t color=0;
    CarPaint paint=CarPaint::Solid;
    uint8_t light=255;
    uint8_t visibility=0; // 0: behind near plane, 1: projected, 2: clipped.
};

inline CarScreenVertex projectCarSurface(const TrackCamera& camera,CarSurfaceVertex p) {
    const float inverse=1/p.z;
    return {camera.principalX+camera.focalLength*p.x*inverse,
            camera.principalY-camera.focalLength*p.y*inverse,inverse,p.u*inverse,p.v*inverse};
}

template<class Transform> void prepareCarPanel(PreparedCarPanel& result,
    const TrackCamera& camera,const CarPanel& face,Transform transform) {
    unsigned front=0;
    result.color=face.color;result.paint=face.paint;result.light=face.light;
    // A slot can switch representation between cars/frames. Explicitly begin
    // the array lifetime before using std::array::operator[] on a union member.
    new (&result.camera) decltype(result.camera);
    for(std::size_t i=0;i<4;++i) {
        const auto p=transform(face.point[i],face.wheel);
        result.camera[i]={p.x,p.y,p.z,(i==0 || i==3 ? face.u0 : face.u1)/255.f,
                                    (i<2 ? face.v0 : face.v1)/255.f};
        if(p.z>=kTrackNearPlane)++front;
    }
    result.visibility=front==0 ? 0 : front==4 ? 1 : 2;
    if(front==0)return;
    result.left=result.top=1e20f;result.right=result.bottom=-1e20f;
    const auto bound=[&](CarScreenVertex p) {
        result.left=std::min(result.left,p.x);result.right=std::max(result.right,p.x);
        result.top=std::min(result.top,p.y);result.bottom=std::max(result.bottom,p.y);
    };
    if(front==4) {
        // Copy the active union member before switching representation.
        const auto vertices=result.camera;
        new (&result.screen) decltype(result.screen);
        for(std::size_t i=0;i<4;++i) {result.screen[i]=projectCarSurface(camera,vertices[i]);bound(result.screen[i]);}
    } else {
        // Match the original two triangle clips, including their internal edge.
        for(unsigned tri=0;tri<2;++tri) {
            const std::array<CarSurfaceVertex,3> points{{result.camera[0],result.camera[tri+1],result.camera[tri+2]}};
            for(unsigned i=0;i<3;++i) {
                const auto p=points[i],q=points[(i+1)%3];
                if(p.z>=kTrackNearPlane)bound(projectCarSurface(camera,p));
                if((p.z>=kTrackNearPlane)!=(q.z>=kTrackNearPlane)) {
                    const float t=(kTrackNearPlane-p.z)/(q.z-p.z);
                    bound(projectCarSurface(camera,{p.x+(q.x-p.x)*t,p.y+(q.y-p.y)*t,kTrackNearPlane,0,0}));
                }
            }
        }
    }
}

// A bounded car-sized RGB565/depth tile, not a second full-screen framebuffer.
// Depth is inverse-camera-z in Q13 (near plane .2 => 40960, within uint16_t).
template<int Width,int Height> class CarSurfaceRaster {
public:
    static constexpr int kWidth=Width,kHeight=Height;
    void setPaintAtlas(const RacePaintAtlas* atlas) { _paintAtlas=atlas; }
    unsigned preferInternalMemory() {
        return unsigned(_fastDepth.allocate())+unsigned(_fastColor.allocate());
    }
    // Optional, allocated once on race open. Failure retains the original
    // candidate traversal without growing the main task's stack.
    bool preferInternalOcclusionRows() { _fastOcclusionCandidates.allocate(); return _fastOcclusionRows.allocate(); }
    void begin(int x,int y,int width=Width,int height=Height) {
        _x=x;_y=y;
        _width=std::clamp(width,1,Width);_height=std::clamp(height,1,Height);
        std::memset(depthData(),0,_width*_height*sizeof(uint16_t));
    }
    void triangle(CarScreenVertex a,CarScreenVertex b,CarScreenVertex c,
                  uint16_t color,CarPaint paint,uint8_t light) {
        auto* depthBuffer=depthData();auto* colorBuffer=colorData();
        const float det=(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
        if(!std::isfinite(det) || std::abs(det)<.001f)return;
        const float left=std::max(float(_x),rasterFloor(std::min({a.x,b.x,c.x})));
        const float right=std::min(float(_x+_width-1),rasterCeil(std::max({a.x,b.x,c.x})));
        const float top=std::max(float(_y),rasterFloor(std::min({a.y,b.y,c.y})));
        const float bottom=std::min(float(_y+_height-1),rasterCeil(std::max({a.y,b.y,c.y})));
        if(left>right || top>bottom)return;
        const int x0=int(left),x1=int(right),y0=int(top),y1=int(bottom);
        const float inverse=1/det;
        // Xtensa otherwise repeats __divsf3 for each shaded pigment. Lighting
        // is constant for the triangle; retain the exact original quotient.
        const float lightFactor=light==255 ? 1.f : light/255.f;
        const uint16_t solidColor=light==255 ? color : carTint(color,lightFactor);
        const auto* texture=_paintAtlas ? _paintAtlas->find(paint,color,light) : nullptr;
        const bool scanRows=(x1-x0)*(y1-y0)>256;
        const std::array<CarScreenVertex,3> points{{a,b,c}};
        std::array<float,3> slopes{};
        if(scanRows) for(int edge=0;edge<3;++edge) {
            const auto& p=points[edge];const auto& q=points[(edge+1)%3];
            if(std::abs(q.y-p.y)>=.00001f)slopes[edge]=(q.x-p.x)/(q.y-p.y);
        }
        for(int y=y0;y<=y1;++y) {
            // Thin panels waste most of their bounding box. Restrict each row
            // conservatively, retaining the original barycentric coverage test.
            float rowLeft=right,rowRight=left;
            bool intersects=false;
            const float rowY=y+.5f;
            if(scanRows) for(int edge=0;edge<3;++edge) {
                const auto& p=points[edge];
                const auto& q=points[(edge+1)%3];
                if(rowY<std::min(p.y,q.y)-.001f || rowY>std::max(p.y,q.y)+.001f)continue;
                if(std::abs(q.y-p.y)<.00001f) {
                    rowLeft=std::min(rowLeft,std::min(p.x,q.x));
                    rowRight=std::max(rowRight,std::max(p.x,q.x));
                } else {
                    const float x=p.x+slopes[edge]*(rowY-p.y);
                    rowLeft=std::min(rowLeft,x);rowRight=std::max(rowRight,x);
                }
                intersects=true;
            }
            // The coverage epsilon admits a thin exterior strip. Fall back at
            // extrema so degenerate/near-horizontal edges retain exact coverage.
            int first=x0,last=x1;
            if(intersects) {
                first=int(std::max(float(x0),rasterFloor(rowLeft)-2.f));
                last=int(std::min(float(x1),rasterCeil(rowRight)+2.f));
            }
            for(int x=first;x<=last;++x) {
            const float px=x+.5f-a.x,py=y+.5f-a.y;
            const float s=(px*(c.y-a.y)-py*(c.x-a.x))*inverse;
            const float t=((b.x-a.x)*py-(b.y-a.y)*px)*inverse;
            if(s<-.00001f || t<-.00001f || s+t>1.00001f)continue;
            const float depth=a.depth+s*(b.depth-a.depth)+t*(c.depth-a.depth);
            if(!(depth>0) || !std::isfinite(depth))continue;
            const auto d=uint16_t(std::clamp(depth*8192.f,1.f,65535.f));
            const auto index=std::size_t(y-_y)*_width+(x-_x);
            if(d<depthBuffer[index])continue;
            uint16_t pigment=color;
            if(paint!=CarPaint::Solid) {
                const float u=(a.uDepth+s*(b.uDepth-a.uDepth)+t*(c.uDepth-a.uDepth))/depth;
                const float v=(a.vDepth+s*(b.vDepth-a.vDepth)+t*(c.vDepth-a.vDepth))/depth;
                pigment=texture ? RacePaintAtlas::sample(texture,u,v) :
                    carPaintColor(paint,color,std::clamp(u,0.f,1.f),std::clamp(v,0.f,1.f));
            }
            depthBuffer[index]=d;
            colorBuffer[index]=texture ? pigment : paint==CarPaint::Solid ? solidColor :
                          (light==255 ? pigment : carTint(pigment,lightFactor));
            }
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
        if(v[0].z>=kTrackNearPlane && v[1].z>=kTrackNearPlane &&
           v[2].z>=kTrackNearPlane && v[3].z>=kTrackNearPlane) {
            const auto a=projectCarSurface(camera,v[0]),b=projectCarSurface(camera,v[1]);
            const auto c=projectCarSurface(camera,v[2]),d=projectCarSurface(camera,v[3]);
            triangle(a,b,c,face.color,face.paint,face.light);
            triangle(a,c,d,face.color,face.paint,face.light);
            return;
        }
        cameraTriangle(camera,v[0],v[1],v[2],face.color,face.paint,face.light);
        cameraTriangle(camera,v[0],v[2],v[3],face.color,face.paint,face.light);
    }
    void preparedPanel(const TrackCamera& camera,const PreparedCarPanel& face) {
        if(!face.visibility || face.right<_x-1 || face.left>_x+_width ||
           face.bottom<_y-1 || face.top>_y+_height)return;
        if(face.visibility==1) {
            triangle(face.screen[0],face.screen[1],face.screen[2],face.color,face.paint,face.light);
            triangle(face.screen[0],face.screen[2],face.screen[3],face.color,face.paint,face.light);
        } else {
            cameraTriangle(camera,face.camera[0],face.camera[1],face.camera[2],face.color,face.paint,face.light);
            cameraTriangle(camera,face.camera[0],face.camera[2],face.camera[3],face.color,face.paint,face.light);
        }
    }
    void blit(lgfx::LGFXBase& canvas,const PencilOcclusion* occlusion=nullptr,float occlusionScale=1.f,
              bool filterRows=true,CarBlitWork* work=nullptr) const {
        const auto* depthBuffer=depthData();const auto* colorBuffer=colorData();
        auto& candidates=_fastOcclusionCandidates.get() ? *_fastOcclusionCandidates.get() : _occlusionCandidates;
        auto* rowCandidates=_fastOcclusionRows.get();
        filterRows=filterRows && rowCandidates;
        std::size_t count=0;
        if(occlusion) for(std::size_t i=0;i<occlusion->count;++i) {
            const auto& s=occlusion->surfaces[i];
            if(s.maxX>=_x*occlusionScale && s.minX<(_x+_width)*occlusionScale &&
               s.maxY>=_y*occlusionScale && s.minY<(_y+_height)*occlusionScale) {
                float area=0;
                for(std::size_t edge=0;edge<s.count;++edge) {
                    const auto p=s.points[edge],q=s.points[(edge+1)%s.count];
                    area+=p.x*q.y-p.y*q.x;
                }
                // Reuse the index's spare high bit; no extra pixel/depth buffer.
                candidates[count++]=uint16_t(i)|(area<0 ? 0x8000u : 0u);
            }
        }
        if(work)work->tileCandidates+=count;
        for(int y=0;y<_height;++y) {
            // A surface outside this scanline cannot hide any of its pixels.
            // Keep order, bounds inclusivity and the original per-pixel math.
            const uint16_t* selected=candidates.data();
            std::size_t selectedCount=count;
            if(filterRows && count) {
                const float rowY=(_y+y+.5f)*occlusionScale;
                selectedCount=0;
                for(std::size_t i=0;i<count;++i) {
                    const auto packed=candidates[i];
                    const auto& surface=occlusion->surfaces[packed&0x7fffu];
                    if(surface.count<3 || rowY<surface.minY || rowY>surface.maxY)continue;
                    (*rowCandidates)[selectedCount++]=packed;
                }
                selected=rowCandidates->data();
            }
            if(work) {
                work->rows++;work->rowCandidates+=selectedCount;
                work->unfilteredRowCandidates+=count;
            }
            int start=-1;
            for(int x=0;x<=_width;++x) {
                const auto index=std::size_t(y)*_width+x;
                bool visible=x<_width && depthBuffer[index]!=0;
                if(visible && occlusion) {
                    const float inverse=depthBuffer[index]/8192.f;
                    const TrackScreenPoint p{(_x+x+.5f)*occlusionScale,(_y+y+.5f)*occlusionScale};
                    for(std::size_t s=0;s<selectedCount;++s) {
                        const auto packed=selected[s];
                        const auto& surface=occlusion->surfaces[packed&0x7fffu];
                        if(surface.count<3 || p.x<surface.minX || p.x>surface.maxX ||
                           p.y<surface.minY || p.y>surface.maxY)continue;
                        const auto& d=surface.inverseDepth;
                        if(d.x*p.x+d.y*p.y+d.z-inverse-.0001f<0)continue;
                        const float orientation=(packed&0x8000u) ? -1.f : 1.f;
                        bool inside=true;
                        for(std::size_t edge=0;edge<surface.count;++edge) {
                            const auto a=surface.points[edge],b=surface.points[(edge+1)%surface.count];
                            if(orientation*((b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x))<0) {
                                inside=false;break;
                            }
                        }
                        if(inside) {visible=false;break;}
                    }
                }
                if(start>=0 && !visible) {
                    drawColorSpan(canvas,_x+start,_y+y,x-start,colorBuffer+std::size_t(y)*_width+start);
                    start=-1;
                }
                if(visible && start<0)start=x;
            }
        }
    }
    // Expand only the car coverage, leaving the native UI/background intact.
    // Reuse the existing compact active planes; no second sprite is allocated.
    void blitScaled(lgfx::LGFXBase& canvas,int x,int y,int width,int height) const {
        if(width<=0 || width>Width || height<=0 || height>Height)return;
        std::array<uint16_t,Width> row{},sourceX{};
        for(int px=0;px<width;++px)sourceX[px]=uint16_t((2*px+1)*_width/(2*width));
        const auto* depth=depthData();const auto* color=colorData();
        for(int py=0;py<height;++py) {
            const int sourceY=(2*py+1)*_height/(2*height);
            const auto offset=std::size_t(sourceY)*_width;
            int start=-1;
            for(int px=0;px<=width;++px) {
                const bool visible=px<width && depth[offset+sourceX[px]]!=0;
                if(visible) {
                    row[px]=color[offset+sourceX[px]];
                    if(start<0)start=px;
                } else if(start>=0) {
                    drawColorSpan(canvas,x+start,y+py,px-start,row.data()+start);
                    start=-1;
                }
            }
        }
    }
    uint16_t depthAt(int x,int y) const {return depthData()[std::size_t(y)*_width+x];}
private:
    const RacePaintAtlas* _paintAtlas=nullptr;
    using Pixels=std::array<uint16_t,Width*Height>;
    RenderScratch<Pixels> _fastDepth,_fastColor;
    mutable std::array<uint16_t,PencilOcclusion::kCapacity> _occlusionCandidates{};
    RenderScratch<std::array<uint16_t,PencilOcclusion::kCapacity>> _fastOcclusionRows,_fastOcclusionCandidates;
    uint16_t* depthData() {return _fastDepth.get() ? _fastDepth.get()->data() : _depth.data();}
    const uint16_t* depthData() const {return _fastDepth.get() ? _fastDepth.get()->data() : _depth.data();}
    uint16_t* colorData() {return _fastColor.get() ? _fastColor.get()->data() : _color.data();}
    const uint16_t* colorData() const {return _fastColor.get() ? _fastColor.get()->data() : _color.data();}
    std::array<uint16_t,Width*Height> _depth{};
    std::array<uint16_t,Width*Height> _color{};
    int _x=0,_y=0,_width=Width,_height=Height;
};
} // namespace lets_and_go
