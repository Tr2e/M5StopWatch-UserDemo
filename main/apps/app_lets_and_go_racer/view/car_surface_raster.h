#pragma once
#include "car_paint.h"
#include "race_paint_atlas.h"
#include "pencil_scene.h"
#include "color_span.h"
#include "render_scratch.h"
#include <array>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <new>

namespace lets_and_go {
struct CarBlitWork {
    uint32_t tileCandidates=0,rowCandidates=0,unfilteredRowCandidates=0,rows=0;
};
struct SurfaceRasterMetrics {
    uint32_t testedPixels=0,writtenPixels=0,depthRejectedPixels=0;
};
template<bool Enabled> struct SurfaceRasterMetricsStorage {};
template<> struct SurfaceRasterMetricsStorage<true> {SurfaceRasterMetrics value{};};

struct CarSurfaceVertex {float x,y,z,u,v;};
struct CarScreenVertex {float x,y,depth,uDepth,vDepth;};
struct SolidScreenVertex {float x,y,depth;};

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

// Band-parallel solid rendering does not need perspective UVs or horizontal
// bounds after the panel has passed the frame-level reject. Keeping only xyz
// (screen depth when projected, camera z when clipped) reduces shared-memory
// traffic when multiple cores replay the same ordered panel list.
struct PreparedSolidPanel {
    struct Vertex {float x=0,y=0,z=0;};
    std::array<Vertex,4> vertex{};
    float top=0,bottom=0;
    uint16_t color=0;
    uint8_t light=255;
    uint8_t visibility=0; // Low bits match PreparedCarPanel; high bits carry replay invariants.
};
// Once band selection is complete, vertical bounds are dead. Keep the final
// raster command tightly packed so both cores stream less shared memory.
struct PreparedSolidRasterPanel {
    std::array<PreparedSolidPanel::Vertex,4> vertex{};
    uint16_t color=0;
    uint8_t light=255;
    uint8_t visibility=0;
};
// When projected vertices live in fast shared storage, keep only their keys in
// the ordered command stream. This shrinks the PSRAM command payload from 52 B
// to 12 B without changing the projected float values consumed by raster.
struct PreparedIndexedSolidRasterPanel {
    std::array<uint16_t,4> vertex{};
    uint16_t color=0;
    uint8_t light=255;
    uint8_t visibility=0;
};
static_assert(sizeof(PreparedIndexedSolidRasterPanel)==12);
constexpr uint8_t kPreparedSolidTriangle=0x80u;
constexpr uint8_t kPreparedSolidTrustedDepth=0x40u;
constexpr uint8_t kPreparedSolidBandSelected=0x20u;

inline PreparedSolidPanel compactSolidPanel(const PreparedCarPanel& source) {
    PreparedSolidPanel result{};
    result.top=source.top;result.bottom=source.bottom;
    result.color=source.color;result.light=source.light;result.visibility=source.visibility;
    if(source.visibility==1 && source.screen[2].x==source.screen[3].x &&
       source.screen[2].y==source.screen[3].y && source.screen[2].depth==source.screen[3].depth)
        result.visibility|=kPreparedSolidTriangle;
    else if(source.visibility==2 && source.camera[2].x==source.camera[3].x &&
            source.camera[2].y==source.camera[3].y && source.camera[2].z==source.camera[3].z)
        result.visibility|=kPreparedSolidTriangle;
    if(source.visibility==1)for(std::size_t i=0;i<4;++i)
        result.vertex[i]={source.screen[i].x,source.screen[i].y,source.screen[i].depth};
    else for(std::size_t i=0;i<4;++i)
        result.vertex[i]={source.camera[i].x,source.camera[i].y,source.camera[i].z};
    if(source.visibility==1) {
        bool trusted=true;
        for(const auto& vertex:source.screen)
            trusted=trusted && std::isfinite(vertex.depth) && vertex.depth>=.001f && vertex.depth<=7.9f;
        if(trusted)result.visibility|=kPreparedSolidTrustedDepth;
    }
    return result;
}

inline PreparedSolidRasterPanel compactSolidRasterPanel(const PreparedSolidPanel& source) {
    return {source.vertex,source.color,source.light,source.visibility};
}

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
template<int Width,int Height,bool Measure=false>
class CarSurfaceRaster : private SurfaceRasterMetricsStorage<Measure> {
public:
    static constexpr int kWidth=Width,kHeight=Height;
    void resetMetrics(){if constexpr(Measure)static_cast<SurfaceRasterMetricsStorage<true>&>(*this).value={};}
    SurfaceRasterMetrics metrics() const{
        if constexpr(Measure)return static_cast<const SurfaceRasterMetricsStorage<true>&>(*this).value;
        return {};
    }
    void setPaintAtlas(const RacePaintAtlas* atlas) { _paintAtlas=atlas; }
    void setIncrementalInterpolation(bool enabled) { _incrementalInterpolation=enabled; }
    void setSolidFastPath(bool enabled) { _solidFastPath=enabled; }
    // Solid triangles have convex pixel coverage on every scanline. Locate the
    // two boundary pixels with the reference barycentric predicate, then omit
    // that predicate for the guaranteed-covered interior span.
    void setSolidSpanFastPath(bool enabled) { _solidSpanFastPath=enabled; }
    // Projection-clipped solid geometry has finite positive inverse depth. A
    // guarded triangle-level check lets its inner loop omit redundant finite,
    // sign and saturation work while retaining the general fallback.
    void setTrustedSolidDepthFastPath(bool enabled) { _trustedSolidDepthFastPath=enabled; }
    // Prepared solid quads share lighting and validated depth across both
    // constituent triangles. Hoist those panel-constant checks and tint work.
    void setSolidQuadFastPath(bool enabled) {_solidQuadFastPath=enabled;}
    // The framebuffer panel already owns the surrounding transaction. Under a
    // guarded native 16-bit/full-clip layout, write spans through its address
    // window instead of rebuilding pushImage clipping state for every row.
    void setDirectSpanFastPath(bool enabled) { _directSpanFastPath=enabled; }
    // Native framebuffer rows avoid thousands of tiny image API calls. The
    // implementation remains guarded by layout, clip and rotation checks.
    void setNativeFrameBufferFastPath(bool enabled) { _nativeFrameBufferFastPath=enabled; }
    void setSparseCompositeFastPath(bool enabled) {_sparseCompositeFastPath=enabled;}
    // Optional caller-owned storage for rows [splitRow,height). This lets a
    // band-parallel renderer keep one write-only color band in internal RAM
    // without mirroring or copying its depth plane.
    void setSplitColorStorage(uint16_t* storage,int width,int splitRow) {
        _splitColor=storage;_splitColorWidth=width;_splitColorRow=splitRow;
    }
    // Authoritative depth storage for rows [splitRow,height). Occupancy-driven
    // composite can consume this band without copying it back to PSRAM.
    void setSplitDepthStorage(uint16_t* storage,int width,int splitRow) {
        if(storage!=_splitDepth || width!=_splitDepthWidth || splitRow!=_splitDepthRow)
            _lastSparseDepthClear=false;
        _splitDepth=storage;_splitDepthWidth=width;_splitDepthRow=splitRow;
    }
    void setSparseDepthStorage(uint8_t* storage,std::size_t bytes) {
        _occupiedDepth=storage;_occupiedDepthBytes=bytes;_trackedDepthPixels=0;_lastSparseDepthClear=false;
        if(storage && bytes)std::memset(storage,0,bytes);
    }
    void setSparseDepthClearFastPath(bool enabled) {_sparseDepthClearRequested=enabled;}
    void setSparseDepthSpanClearFastPath(bool enabled) {
        if(enabled!=_sparseDepthSpanClearRequested)_lastSparseDepthClear=false;
        _sparseDepthSpanClearRequested=enabled;
    }
    void setDeferredSparseDepthRecord(bool enabled) {_deferredSparseDepthRecord=enabled;}
    unsigned preferInternalMemory() {
        return unsigned(_fastDepth.allocate())+unsigned(_fastColor.allocate());
    }
    // Optional, allocated once on race open. Failure retains the original
    // candidate traversal without growing the main task's stack.
    bool preferInternalOcclusionRows() { _fastOcclusionCandidates.allocate(); return _fastOcclusionRows.allocate(); }
    void begin(int x,int y,int width=Width,int height=Height) {
        _x=x;_y=y;
        _width=std::clamp(width,1,Width);_height=std::clamp(height,1,Height);
        const std::size_t activePixels=std::size_t(_width)*_height;
        _sparseDepthClearFastPath=_sparseDepthClearRequested && _occupiedDepth &&
                                  _occupiedDepthBytes>=(activePixels+7)/8;
        if(_sparseDepthClearFastPath && _lastSparseDepthClear) {
            const std::size_t bytes=(_trackedDepthPixels+7)/8;
            if(_sparseDepthSpanClearRequested && _trackedDepthWidth==_width) {
                const int rows=int(_trackedDepthPixels/std::size_t(_trackedDepthWidth));
                for(int row=0;row<rows;++row) {
                    const std::size_t rowStart=std::size_t(row)*_trackedDepthWidth;
                    const std::size_t rowEnd=rowStart+_trackedDepthWidth;
                    const std::size_t firstByte=rowStart>>3,lastByte=(rowEnd-1)>>3;
                    std::size_t first=rowEnd,last=rowStart;
                    for(std::size_t byte=firstByte;byte<=lastByte;++byte) {
                        unsigned bits=_occupiedDepth[byte];
                        if(byte==firstByte)bits&=0xffu<<unsigned(rowStart&7);
                        if(byte==lastByte && (rowEnd&7))bits&=(1u<<unsigned(rowEnd&7))-1u;
                        if(bits){first=byte*8+unsigned(__builtin_ctz(bits));break;}
                    }
                    if(first<rowEnd)for(std::size_t byte=lastByte+1;byte-->firstByte;) {
                        unsigned bits=_occupiedDepth[byte];
                        if(byte==firstByte)bits&=0xffu<<unsigned(rowStart&7);
                        if(byte==lastByte && (rowEnd&7))bits&=(1u<<unsigned(rowEnd&7))-1u;
                        if(bits){last=byte*8+unsigned(31-__builtin_clz(bits));break;}
                    }
                    if(first<=last)clearDepthRange(first,last-first+1);
                }
                std::memset(_occupiedDepth,0,bytes);
            } else {
                for(std::size_t byte=0;byte<bytes;++byte){
                    unsigned bits=_occupiedDepth[byte];
                    while(bits){
                        const unsigned bit=unsigned(__builtin_ctz(bits));
                        depthPixel(byte*8+bit)=0;bits&=bits-1;
                    }
                    _occupiedDepth[byte]=0;
                }
            }
            // A smaller compact frame may leave older data in the inactive
            // tail. Clear that tail when resolution grows again.
            if(activePixels>_trackedDepthPixels)
                clearDepthRange(_trackedDepthPixels,activePixels-_trackedDepthPixels);
        } else {
            clearDepthRange(0,std::size_t(_width)*_height);
            if(_occupiedDepth && _trackedDepthPixels)std::memset(_occupiedDepth,0,(_trackedDepthPixels+7)/8);
        }
        _trackedDepthPixels=std::size_t(_width)*_height;
        _trackedDepthWidth=_width;
        _lastSparseDepthClear=_sparseDepthClearFastPath;
    }
    // Nearest-expand the packed active tile to the full template size in
    // place. Source rows are read before dest rows overwrite them.
    void upsampleNearestToFull() {
        if(_width==Width && _height==Height)return;
        const int srcW=_width,srcH=_height;
        auto* depth=depthData();auto* color=colorData();
        std::array<uint16_t,Width> depthRow{},colorRow{};
        for(int y=Height-1;y>=0;--y){
            const int sy=(2*y+1)*srcH/(2*Height);
            for(int x=0;x<Width;++x){
                const int sx=(2*x+1)*srcW/(2*Width);
                const int s=sy*srcW+sx;
                depthRow[x]=depth[s];colorRow[x]=color[s];
            }
            std::memcpy(depth+std::size_t(y)*Width,depthRow.data(),sizeof(depthRow));
            std::memcpy(color+std::size_t(y)*Width,colorRow.data(),sizeof(colorRow));
        }
        _width=Width;_height=Height;
    }
    void triangle(CarScreenVertex a,CarScreenVertex b,CarScreenVertex c,
                  uint16_t color,CarPaint paint,uint8_t light) {
        triangleRows(a,b,c,color,paint,light,_y,_y+_height-1);
    }
    // Render an inclusive row range into the same backing planes. Disjoint,
    // byte-aligned ranges can be dispatched to separate cores without changing
    // panel order or depth semantics.
    void triangleRows(CarScreenVertex a,CarScreenVertex b,CarScreenVertex c,
                      uint16_t color,CarPaint paint,uint8_t light,int clipTop,int clipBottom) {
        if(_solidFastPath && paint==CarPaint::Solid) {
            if(_solidSpanFastPath) {
                const auto trustedDepth=[&](float depth) {
                    return std::isfinite(depth) && depth>=.001f && depth<=7.9f;
                };
                if(_trustedSolidDepthFastPath && trustedDepth(a.depth) &&
                   trustedDepth(b.depth) && trustedDepth(c.depth)) {
                    triangleImpl<false,true,true,true>(a,b,c,color,paint,light,clipTop,clipBottom);return;
                }
                triangleImpl<false,true,true>(a,b,c,color,paint,light,clipTop,clipBottom);return;
            }
            triangleImpl<false,true>(a,b,c,color,paint,light,clipTop,clipBottom);return;
        }
        if(_incrementalInterpolation)
            triangleImpl<true>(a,b,c,color,paint,light,clipTop,clipBottom);
        else
            triangleImpl<false>(a,b,c,color,paint,light,clipTop,clipBottom);
    }
private:
    struct SolidRasterTarget {
        uint16_t* depth=nullptr;
        uint16_t* color=nullptr;
        std::size_t depthIndexOffset=0;
        std::size_t colorIndexOffset=0;
    };
    template<bool Incremental,bool Solid=false,bool SolidSpan=false,bool TrustedDepth=false,
             bool PreparedSolidColor=false,bool PreparedTarget=false,bool PreparedSparseRecord=false,
             class ScreenVertex=CarScreenVertex>
#ifdef ESP_PLATFORM
    __attribute__((optimize("O3")))
#endif
    void triangleImpl(ScreenVertex a,ScreenVertex b,ScreenVertex c,
                  uint16_t color,CarPaint paint,uint8_t light,int clipTop,int clipBottom,
                  uint16_t preparedSolidColor=0,const SolidRasterTarget* preparedTarget=nullptr) {
        // A band-parallel caller may fall back to one full-frame pass (for
        // example when its worker is unavailable). Split a crossing range so
        // the lower depth band remains authoritative in that fallback too.
        uint16_t* depthBuffer=nullptr;uint16_t* colorBuffer=nullptr;
        std::size_t depthIndexOffset=0,colorIndexOffset=0;
        if constexpr(PreparedTarget) {
            depthBuffer=preparedTarget->depth;colorBuffer=preparedTarget->color;
            depthIndexOffset=preparedTarget->depthIndexOffset;
            colorIndexOffset=preparedTarget->colorIndexOffset;
        } else {
            if(_splitDepth && _width==_splitDepthWidth &&
               clipTop<_splitDepthRow && clipBottom>=_splitDepthRow) {
                triangleImpl<Incremental,Solid,SolidSpan,TrustedDepth,PreparedSolidColor>(
                    a,b,c,color,paint,light,clipTop,_splitDepthRow-1,preparedSolidColor);
                triangleImpl<Incremental,Solid,SolidSpan,TrustedDepth,PreparedSolidColor>(
                    a,b,c,color,paint,light,_splitDepthRow,clipBottom,preparedSolidColor);
                return;
            }
            depthBuffer=depthData();colorBuffer=colorData();
            if(_splitDepth && _width==_splitDepthWidth && clipTop>=_splitDepthRow) {
                depthBuffer=_splitDepth;
                depthIndexOffset=std::size_t(_splitDepthRow)*_width;
            }
            if(_splitColor && _width==_splitColorWidth && clipTop>=_splitColorRow) {
                colorBuffer=_splitColor;
                colorIndexOffset=std::size_t(_splitColorRow)*_width;
            }
        }
        const float det=(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
        if(!std::isfinite(det) || std::abs(det)<.001f)return;
        const float left=std::max(float(_x),rasterFloor(std::min({a.x,b.x,c.x})));
        const float right=std::min(float(_x+_width-1),rasterCeil(std::max({a.x,b.x,c.x})));
        const float top=std::max(float(std::max(_y,clipTop)),rasterFloor(std::min({a.y,b.y,c.y})));
        const float bottom=std::min(float(std::min(_y+_height-1,clipBottom)),rasterCeil(std::max({a.y,b.y,c.y})));
        if(left>right || top>bottom)return;
        const int x0=int(left),x1=int(right),y0=int(top),y1=int(bottom);
        const float inverse=1/det;
        float sStep=0,tStep=0;
        if constexpr(Incremental) {
            sStep=(c.y-a.y)*inverse;
            tStep=-(b.y-a.y)*inverse;
        }
        // Xtensa otherwise repeats __divsf3 for each shaded pigment. Lighting
        // is constant for the triangle; retain the exact original quotient.
        const float lightFactor=light==255 ? 1.f : light/255.f;
        const uint16_t solidColor=PreparedSolidColor ? preparedSolidColor :
                                  (light==255 ? color : carTint(color,lightFactor));
        const auto* texture=_paintAtlas ? _paintAtlas->find(paint,color,light) : nullptr;
        const bool scanRows=(x1-x0)*(y1-y0)>256;
        const std::array<ScreenVertex,3> points{{a,b,c}};
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
            const float py=y+.5f-a.y;
            const auto barycentric=[&](int x,float& s,float& t) {
                const float px=x+.5f-a.x;
                s=(px*(c.y-a.y)-py*(c.x-a.x))*inverse;
                t=((b.x-a.x)*py-(b.y-a.y)*px)*inverse;
            };
            if constexpr(SolidSpan) {
                float s=0,t=0;
                while(first<=last) {
                    barycentric(first,s,t);
                    if(s>=-.00001f && t>=-.00001f && s+t<=1.00001f)break;
                    ++first;
                }
                while(last>first) {
                    barycentric(last,s,t);
                    if(s>=-.00001f && t>=-.00001f && s+t<=1.00001f)break;
                    --last;
                }
            }
            float incrementalS=0,incrementalT=0;
            if constexpr(Incremental) {
                const float firstPx=first+.5f-a.x;
                incrementalS=(firstPx*(c.y-a.y)-py*(c.x-a.x))*inverse;
                incrementalT=((b.x-a.x)*py-(b.y-a.y)*firstPx)*inverse;
            }
            for(int x=first;x<=last;++x) {
            float s,t;
            if constexpr(Incremental) {
                s=incrementalS;t=incrementalT;
                incrementalS+=sStep;incrementalT+=tStep;
            } else {
                barycentric(x,s,t);
            }
            if constexpr(!SolidSpan)
                if(s<-.00001f || t<-.00001f || s+t>1.00001f)continue;
            const float depth=a.depth+s*(b.depth-a.depth)+t*(c.depth-a.depth);
            if constexpr(!TrustedDepth)
                if(!(depth>0) || !std::isfinite(depth))continue;
            if constexpr(Measure)++static_cast<SurfaceRasterMetricsStorage<true>&>(*this).value.testedPixels;
            const auto d=uint16_t(TrustedDepth ? depth*8192.f :
                                  std::clamp(depth*8192.f,1.f,65535.f));
            const auto index=std::size_t(y-_y)*_width+(x-_x);
            const auto oldDepth=depthBuffer[index-depthIndexOffset];
            if(d<oldDepth){if constexpr(Measure)++static_cast<SurfaceRasterMetricsStorage<true>&>(*this).value.depthRejectedPixels;continue;}
            if constexpr(Measure)++static_cast<SurfaceRasterMetricsStorage<true>&>(*this).value.writtenPixels;
            if constexpr(PreparedSparseRecord) {
                _occupiedDepth[index>>3]|=uint8_t(1u<<(index&7));
            } else if(_sparseDepthClearFastPath && !_deferredSparseDepthRecord && !oldDepth)
                _occupiedDepth[index>>3]|=uint8_t(1u<<(index&7));
            if constexpr(Solid) {
                // Identical barycentric/depth operations; compile out texture
                // sampling and per-pixel material branches for solid exhibits.
                depthBuffer[index-depthIndexOffset]=d;colorBuffer[index-colorIndexOffset]=solidColor;
            }else{
            uint16_t pigment=color;
            if(paint!=CarPaint::Solid) {
                const float u=(a.uDepth+s*(b.uDepth-a.uDepth)+t*(c.uDepth-a.uDepth))/depth;
                const float v=(a.vDepth+s*(b.vDepth-a.vDepth)+t*(c.vDepth-a.vDepth))/depth;
                pigment=texture ? RacePaintAtlas::sample(texture,u,v) :
                    carPaintColor(paint,color,std::clamp(u,0.f,1.f),std::clamp(v,0.f,1.f));
            }
            const uint16_t output=texture ? pigment : paint==CarPaint::Solid ? solidColor :
                                  (light==255 ? pigment : carTint(pigment,lightFactor));
            depthBuffer[index-depthIndexOffset]=d;colorBuffer[index-colorIndexOffset]=output;
            }
            }
        }
    }
    template<bool PreparedSparseRecord>
#ifdef ESP_PLATFORM
    __attribute__((optimize("O3")))
#endif
    void solidQuadRowsExact(SolidScreenVertex a,SolidScreenVertex b,
                            SolidScreenVertex c,SolidScreenVertex d,
                            int clipTop,int clipBottom,uint16_t solidColor,
                            const SolidRasterTarget& target) {
        struct TriangleSetup {
            std::array<SolidScreenVertex,3> point{};
            std::array<float,3> slope{};
            float inverse=0,left=0,right=-1;
            int x0=0,x1=-1,y0=0,y1=-1;
            bool scanRows=false,valid=false;
        };
        const auto prepare=[&](SolidScreenVertex p,SolidScreenVertex q,SolidScreenVertex r) {
            TriangleSetup setup{{p,q,r}};
            const float det=(q.x-p.x)*(r.y-p.y)-(q.y-p.y)*(r.x-p.x);
            if(!std::isfinite(det) || std::abs(det)<.001f)return setup;
            setup.left=std::max(float(_x),rasterFloor(std::min({p.x,q.x,r.x})));
            setup.right=std::min(float(_x+_width-1),rasterCeil(std::max({p.x,q.x,r.x})));
            const float top=std::max(float(std::max(_y,clipTop)),rasterFloor(std::min({p.y,q.y,r.y})));
            const float bottom=std::min(float(std::min(_y+_height-1,clipBottom)),rasterCeil(std::max({p.y,q.y,r.y})));
            if(setup.left>setup.right || top>bottom)return setup;
            setup.x0=int(setup.left);setup.x1=int(setup.right);
            setup.y0=int(top);setup.y1=int(bottom);setup.inverse=1/det;
            setup.scanRows=(setup.x1-setup.x0)*(setup.y1-setup.y0)>256;
            if(setup.scanRows)for(int edge=0;edge<3;++edge) {
                const auto& u=setup.point[edge];const auto& v=setup.point[(edge+1)%3];
                if(std::abs(v.y-u.y)>=.00001f)setup.slope[edge]=(v.x-u.x)/(v.y-u.y);
            }
            setup.valid=true;return setup;
        };
        const TriangleSetup firstTriangle=prepare(a,b,c),secondTriangle=prepare(a,c,d);
        if(!firstTriangle.valid && !secondTriangle.valid)return;
        const int firstY=std::min(firstTriangle.valid?firstTriangle.y0:secondTriangle.y0,
                                  secondTriangle.valid?secondTriangle.y0:firstTriangle.y0);
        const int lastY=std::max(firstTriangle.valid?firstTriangle.y1:secondTriangle.y1,
                                 secondTriangle.valid?secondTriangle.y1:firstTriangle.y1);
        const auto interval=[&](const TriangleSetup& setup,int y,int& first,int& last) {
            if(!setup.valid || y<setup.y0 || y>setup.y1)return false;
            float rowLeft=setup.right,rowRight=setup.left;bool intersects=false;
            const float rowY=y+.5f;
            if(setup.scanRows)for(int edge=0;edge<3;++edge) {
                const auto& p=setup.point[edge];const auto& q=setup.point[(edge+1)%3];
                if(rowY<std::min(p.y,q.y)-.001f || rowY>std::max(p.y,q.y)+.001f)continue;
                if(std::abs(q.y-p.y)<.00001f) {
                    rowLeft=std::min(rowLeft,std::min(p.x,q.x));
                    rowRight=std::max(rowRight,std::max(p.x,q.x));
                } else {
                    const float x=p.x+setup.slope[edge]*(rowY-p.y);
                    rowLeft=std::min(rowLeft,x);rowRight=std::max(rowRight,x);
                }
                intersects=true;
            }
            first=setup.x0;last=setup.x1;
            if(intersects) {
                first=int(std::max(float(setup.x0),rasterFloor(rowLeft)-2.f));
                last=int(std::min(float(setup.x1),rasterCeil(rowRight)+2.f));
            }
            const auto& p=setup.point[0];const auto& q=setup.point[1];const auto& r=setup.point[2];
            const float py=y+.5f-p.y;
            const auto covered=[&](int x) {
                const float px=x+.5f-p.x;
                const float s=(px*(r.y-p.y)-py*(r.x-p.x))*setup.inverse;
                const float t=((q.x-p.x)*py-(q.y-p.y)*px)*setup.inverse;
                return s>=-.00001f && t>=-.00001f && s+t<=1.00001f;
            };
            while(first<=last && !covered(first))++first;
            while(last>first && !covered(last))--last;
            return first<=last;
        };
        const auto draw=[&](const TriangleSetup& setup,int y,int first,int last) {
            const auto& p=setup.point[0];const auto& q=setup.point[1];const auto& r=setup.point[2];
            const float py=y+.5f-p.y;
            for(int x=first;x<=last;++x) {
                const float px=x+.5f-p.x;
                const float s=(px*(r.y-p.y)-py*(r.x-p.x))*setup.inverse;
                const float t=((q.x-p.x)*py-(q.y-p.y)*px)*setup.inverse;
                const float depth=p.depth+s*(q.depth-p.depth)+t*(r.depth-p.depth);
                const auto value=uint16_t(depth*8192.f);
                const auto index=std::size_t(y-_y)*_width+(x-_x);
                const auto oldDepth=target.depth[index-target.depthIndexOffset];
                if constexpr(Measure)++static_cast<SurfaceRasterMetricsStorage<true>&>(*this).value.testedPixels;
                if(value<oldDepth){if constexpr(Measure)++static_cast<SurfaceRasterMetricsStorage<true>&>(*this).value.depthRejectedPixels;continue;}
                if constexpr(Measure)++static_cast<SurfaceRasterMetricsStorage<true>&>(*this).value.writtenPixels;
                if constexpr(PreparedSparseRecord)
                    _occupiedDepth[index>>3]|=uint8_t(1u<<(index&7));
                else if(_sparseDepthClearFastPath && !_deferredSparseDepthRecord && !oldDepth)
                    _occupiedDepth[index>>3]|=uint8_t(1u<<(index&7));
                target.depth[index-target.depthIndexOffset]=value;
                target.color[index-target.colorIndexOffset]=solidColor;
            }
        };
        for(int y=firstY;y<=lastY;++y) {
            int first=0,last=-1;
            if(interval(firstTriangle,y,first,last))draw(firstTriangle,y,first,last);
            if(interval(secondTriangle,y,first,last))draw(secondTriangle,y,first,last);
        }
    }
public:
    void cameraTriangle(const TrackCamera& camera,CarSurfaceVertex a,CarSurfaceVertex b,
                        CarSurfaceVertex c,uint16_t color,CarPaint paint,uint8_t light) {
        cameraTriangleRows(camera,a,b,c,color,paint,light,_y,_y+_height-1);
    }
    void cameraTriangleRows(const TrackCamera& camera,CarSurfaceVertex a,CarSurfaceVertex b,
                            CarSurfaceVertex c,uint16_t color,CarPaint paint,uint8_t light,
                            int clipTop,int clipBottom) {
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
            triangleRows(project(output[0]),project(output[i]),project(output[i+1]),color,paint,light,clipTop,clipBottom);
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
        preparedPanelRows(camera,face,_y,_y+_height-1);
    }
    void preparedPanelRows(const TrackCamera& camera,const PreparedCarPanel& face,int clipTop,int clipBottom) {
        if(!face.visibility || face.right<_x-1 || face.left>_x+_width ||
           face.bottom<std::max(_y,clipTop)-1 || face.top>std::min(_y+_height-1,clipBottom)+1)return;
        if(face.visibility==1) {
            triangleRows(face.screen[0],face.screen[1],face.screen[2],face.color,face.paint,face.light,clipTop,clipBottom);
            triangleRows(face.screen[0],face.screen[2],face.screen[3],face.color,face.paint,face.light,clipTop,clipBottom);
        } else {
            cameraTriangleRows(camera,face.camera[0],face.camera[1],face.camera[2],face.color,face.paint,face.light,clipTop,clipBottom);
            cameraTriangleRows(camera,face.camera[0],face.camera[2],face.camera[3],face.color,face.paint,face.light,clipTop,clipBottom);
        }
    }
    // Keep the innermost triangle clone in flash. A source-level IRAM
    // duplicate lost GCC's parameter-specialized clone, and relocating the
    // exact clone increased frame time while reducing internal heap.
    void preparedSolidPanelRows(const TrackCamera& camera,const PreparedSolidPanel& face,
                                int clipTop,int clipBottom) {
        const uint8_t visibility=face.visibility&~(kPreparedSolidTriangle|kPreparedSolidTrustedDepth|
                                                   kPreparedSolidBandSelected);
        if(!visibility || (!(face.visibility&kPreparedSolidBandSelected) &&
           (face.bottom<std::max(_y,clipTop)-1 ||
            face.top>std::min(_y+_height-1,clipBottom)+1)))return;
        if(visibility==1) {
            const bool triangle=face.visibility&kPreparedSolidTriangle;
            if(_solidFastPath && _solidSpanFastPath && _trustedSolidDepthFastPath &&
               _solidQuadFastPath && (face.visibility&kPreparedSolidTrustedDepth)) {
                const auto vertex=[&](unsigned i) {const auto p=face.vertex[i];return SolidScreenVertex{p.x,p.y,p.z};};
                const auto a=vertex(0),b=vertex(1),c=vertex(2),d=vertex(3);
                const uint16_t solidColor=face.light==255 ? face.color : carTint(face.color,face.light/255.f);
                const bool crossesDepth=_splitDepth && _width==_splitDepthWidth &&
                    clipTop<_splitDepthRow && clipBottom>=_splitDepthRow;
                const bool crossesColor=_splitColor && _width==_splitColorWidth &&
                    clipTop<_splitColorRow && clipBottom>=_splitColorRow;
                if(!crossesDepth && !crossesColor) {
                    SolidRasterTarget target{depthData(),colorData(),0,0};
                    if(_splitDepth && _width==_splitDepthWidth && clipTop>=_splitDepthRow) {
                        target.depth=_splitDepth;
                        target.depthIndexOffset=std::size_t(_splitDepthRow)*_width;
                    }
                    if(_splitColor && _width==_splitColorWidth && clipTop>=_splitColorRow) {
                        target.color=_splitColor;
                        target.colorIndexOffset=std::size_t(_splitColorRow)*_width;
                    }
                    if(_sparseDepthClearFastPath && !_deferredSparseDepthRecord) {
                        triangleImpl<false,true,true,true,true,true,true>(a,b,c,face.color,CarPaint::Solid,face.light,
                                                                        clipTop,clipBottom,solidColor,&target);
                        if(!triangle)triangleImpl<false,true,true,true,true,true,true>(a,c,d,face.color,CarPaint::Solid,face.light,
                                                                                     clipTop,clipBottom,solidColor,&target);
                    } else {
                        triangleImpl<false,true,true,true,true,true>(a,b,c,face.color,CarPaint::Solid,face.light,
                                                                    clipTop,clipBottom,solidColor,&target);
                        if(!triangle)triangleImpl<false,true,true,true,true,true>(a,c,d,face.color,CarPaint::Solid,face.light,
                                                                                 clipTop,clipBottom,solidColor,&target);
                    }
                } else {
                    triangleImpl<false,true,true,true,true>(a,b,c,face.color,CarPaint::Solid,face.light,
                                                           clipTop,clipBottom,solidColor);
                    if(!triangle)triangleImpl<false,true,true,true,true>(a,c,d,face.color,CarPaint::Solid,face.light,
                                                                        clipTop,clipBottom,solidColor);
                }
            } else {
                const auto vertex=[&](unsigned i) {const auto p=face.vertex[i];return CarScreenVertex{p.x,p.y,p.z,0,0};};
                const auto a=vertex(0),b=vertex(1),c=vertex(2),d=vertex(3);
                triangleRows(a,b,c,face.color,CarPaint::Solid,face.light,clipTop,clipBottom);
                if(!triangle)triangleRows(a,c,d,face.color,CarPaint::Solid,face.light,clipTop,clipBottom);
            }
        } else {
            const auto vertex=[&](unsigned i) {const auto p=face.vertex[i];return CarSurfaceVertex{p.x,p.y,p.z,0,0};};
            const auto a=vertex(0),b=vertex(1),c=vertex(2),d=vertex(3);
            cameraTriangleRows(camera,a,b,c,face.color,CarPaint::Solid,face.light,clipTop,clipBottom);
            if(!(face.visibility&kPreparedSolidTriangle))
                cameraTriangleRows(camera,a,c,d,face.color,CarPaint::Solid,face.light,clipTop,clipBottom);
        }
    }
private:
    template<bool RecordSparse>
#ifdef ESP_PLATFORM
    __attribute__((noinline,optimize("O3"),section(".iram1")))
#endif
    void trustedSolidFace(const std::array<SolidScreenVertex,4>& vertex,uint16_t color,
                          uint8_t light,uint8_t visibility,int clipTop,int clipBottom,
                          const SolidRasterTarget& target) {
        const auto& a=vertex[0];const auto& b=vertex[1];const auto& c=vertex[2];const auto& d=vertex[3];
        const uint16_t solidColor=light==255 ? color : carTint(color,light/255.f);
        if(visibility&kPreparedSolidTriangle)
            triangleImpl<false,true,true,true,true,true,RecordSparse>(a,b,c,color,CarPaint::Solid,light,
                                                                      clipTop,clipBottom,solidColor,&target);
        else solidQuadRowsExact<RecordSparse>(a,b,c,d,clipTop,clipBottom,solidColor,target);
    }
public:
#ifdef ESP_PLATFORM
    __attribute__((optimize("O3"),section(".iram1")))
#endif
    // Replay an already selected, trusted solid band. The caller guarantees
    // projected/trusted vertices and ordered band membership, allowing the
    // frame-wide raster/storage invariants to be resolved once per batch
    // instead of once per panel.
    void preparedSolidPanelBatchRowsTrusted(const TrackCamera& camera,
                                             const PreparedSolidRasterPanel* panels,
                                             const uint16_t* panelIndices,
                                             std::size_t count,int clipTop,int clipBottom) {
        const bool crossesDepth=_splitDepth && _width==_splitDepthWidth &&
            clipTop<_splitDepthRow && clipBottom>=_splitDepthRow;
        const bool crossesColor=_splitColor && _width==_splitColorWidth &&
            clipTop<_splitColorRow && clipBottom>=_splitColorRow;
        if(!_solidFastPath || !_solidSpanFastPath || !_trustedSolidDepthFastPath ||
           !_solidQuadFastPath || crossesDepth || crossesColor) {
            for(std::size_t i=0;i<count;++i) {
                const auto panel=panelIndices?panelIndices[i]:i;
                PreparedSolidPanel expanded{};
                expanded.vertex=panels[panel].vertex;
                expanded.color=panels[panel].color;expanded.light=panels[panel].light;
                expanded.visibility=panels[panel].visibility|kPreparedSolidBandSelected;
                preparedSolidPanelRows(camera,expanded,clipTop,clipBottom);
            }
            return;
        }
        SolidRasterTarget target{depthData(),colorData(),0,0};
        if(_splitDepth && _width==_splitDepthWidth && clipTop>=_splitDepthRow) {
            target.depth=_splitDepth;
            target.depthIndexOffset=std::size_t(_splitDepthRow)*_width;
        }
        if(_splitColor && _width==_splitColorWidth && clipTop>=_splitColorRow) {
            target.color=_splitColor;
            target.colorIndexOffset=std::size_t(_splitColorRow)*_width;
        }
        const bool recordSparse=_sparseDepthClearFastPath && !_deferredSparseDepthRecord;
        for(std::size_t i=0;i<count;++i) {
            const auto& face=panels[panelIndices?panelIndices[i]:i];
            std::array<SolidScreenVertex,4> vertex{};
            for(unsigned v=0;v<4;++v) {
                const auto p=face.vertex[v];vertex[v]={p.x,p.y,p.z};
            }
            if(recordSparse)trustedSolidFace<true>(vertex,face.color,face.light,face.visibility,
                                                    clipTop,clipBottom,target);
            else trustedSolidFace<false>(vertex,face.color,face.light,face.visibility,
                                         clipTop,clipBottom,target);
        }
    }
    // Indexed companion to the trusted batch above. The command stream stays
    // ordered; only vertex materialization moves to the consuming core and
    // reads the exact cached float triplets. A directly generated stream may
    // grow backward in memory and is then consumed with a negative stride.
#ifdef ESP_PLATFORM
    __attribute__((optimize("O3"),section(".iram1")))
#endif
    void preparedIndexedSolidPanelBatchRowsTrusted(const PreparedIndexedSolidRasterPanel* panels,
                                                    const TrackCameraPoint* projected,
                                                    const uint16_t* panelIndices,std::size_t count,
                                                    int clipTop,int clipBottom,bool reverseDirect=false) {
        SolidRasterTarget target{depthData(),colorData(),0,0};
        if(_splitDepth && _width==_splitDepthWidth && clipTop>=_splitDepthRow) {
            target.depth=_splitDepth;target.depthIndexOffset=std::size_t(_splitDepthRow)*_width;
        }
        if(_splitColor && _width==_splitColorWidth && clipTop>=_splitColorRow) {
            target.color=_splitColor;target.colorIndexOffset=std::size_t(_splitColorRow)*_width;
        }
        const bool recordSparse=_sparseDepthClearFastPath && !_deferredSparseDepthRecord;
        const auto rasterFace=[&](const PreparedIndexedSolidRasterPanel& face) {
            std::array<SolidScreenVertex,4> vertex{};
            for(unsigned v=0;v<4;++v) {
                const auto p=projected[face.vertex[v]];vertex[v]={p.x,p.y,p.z};
            }
            if(recordSparse)trustedSolidFace<true>(vertex,face.color,face.light,face.visibility,
                                                    clipTop,clipBottom,target);
            else trustedSolidFace<false>(vertex,face.color,face.light,face.visibility,
                                         clipTop,clipBottom,target);
        };
        if(panelIndices)for(std::size_t i=0;i<count;++i)rasterFace(panels[panelIndices[i]]);
        else {
            const std::ptrdiff_t directStep=reverseDirect ? -1 : 1;
            const auto* directPanel=panels;
            for(std::size_t i=0;i<count;++i) {
                rasterFace(*directPanel);directPanel+=directStep;
            }
        }
    }
    // Hidden-line stroke: keep a surface only when its depth matches the filled
    // buffer. Empty pixels stay empty so far-side edges cannot leak around the
    // silhouette. Bias absorbs Q13 quantization against the parent panel.
    void stroke(CarScreenVertex a,CarScreenVertex b,uint16_t color,uint16_t bias=8,bool writeEmpty=false) {
        auto* depthBuffer=depthData();auto* colorBuffer=colorData();
        if(!std::isfinite(a.x)||!std::isfinite(a.y)||!std::isfinite(b.x)||!std::isfinite(b.y))return;
        if(!(a.depth>0)||!(b.depth>0)||!std::isfinite(a.depth)||!std::isfinite(b.depth))return;
        const int x0=int(std::lround(a.x)),y0=int(std::lround(a.y));
        const int x1=int(std::lround(b.x)),y1=int(std::lround(b.y));
        const int steps=std::max(std::abs(x1-x0),std::abs(y1-y0));
        if(steps<0)return;
        const float inverse=steps==0?0.f:1.f/float(steps);
        for(int i=0;i<=steps;++i) {
            const float t=float(i)*inverse;
            const int x=x0+int(std::lround((x1-x0)*t)),y=y0+int(std::lround((y1-y0)*t));
            if(x<_x||x>=_x+_width||y<_y||y>=_y+_height)continue;
            const float depth=a.depth+(b.depth-a.depth)*t;
            if(!(depth>0)||!std::isfinite(depth))continue;
            const auto d=uint16_t(std::clamp(depth*8192.f,1.f,65535.f));
            const auto index=std::size_t(y-_y)*_width+(x-_x);
            if(depthBuffer[index]==0){
                if(!writeEmpty)continue;
                depthBuffer[index]=d;colorBuffer[index]=color;continue;
            }
            if(uint32_t(d)+bias<depthBuffer[index])continue;
            colorBuffer[index]=color;
        }
    }
    void strokeCamera(const TrackCamera& camera,CarSurfaceVertex a,CarSurfaceVertex b,uint16_t color,bool writeEmpty=false) {
        if(a.z<kTrackNearPlane && b.z<kTrackNearPlane)return;
        if((a.z<kTrackNearPlane)!=(b.z<kTrackNearPlane)) {
            const float t=(kTrackNearPlane-a.z)/(b.z-a.z);
            const CarSurfaceVertex cut{a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,kTrackNearPlane,0,0};
            if(a.z<kTrackNearPlane)a=cut;else b=cut;
        }
        stroke(projectCarSurface(camera,a),projectCarSurface(camera,b),color,8,writeEmpty);
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
    void blitScaled(lgfx::LGFXBase& canvas,int x,int y,int width,int height,
                    uint8_t* nativeFrameBuffer=nullptr,std::size_t nativeStride=0) {
        if(width<=0 || width>Width || height<=0 || height>Height)return;
        if(width==_width && height==_height)blitScaledImpl<false>(canvas,x,y,width,height,nativeFrameBuffer,nativeStride);
        else blitScaledImpl<true>(canvas,x,y,width,height,nativeFrameBuffer,nativeStride);
    }
#ifdef ESP_PLATFORM
    bool canBlitScaledNativeSparse(int width,int height,uint8_t* nativeFrameBuffer,
                                   std::size_t nativeStride,int x) const {
        return width>0 && height>0 && width<=Width && height<=Height &&
            (width!=_width || height!=_height) && _sparseCompositeFastPath &&
            _sparseDepthClearFastPath && !_deferredSparseDepthRecord && _occupiedDepth &&
            nativeFrameBuffer && nativeStride>=std::size_t(x+width)*2;
    }
    void blitScaledNativeSparseRows(uint8_t* nativeFrameBuffer,std::size_t nativeStride,
                                    int x,int y,int width,int height,int firstRow,int lastRow,
                                    const uint16_t* mappedFirst=nullptr,const uint16_t* mappedLast=nullptr,
                                    const uint16_t* mappedSourceY=nullptr) {
        std::array<uint16_t,Width> sourceX{},destinationFirstStorage{},destinationLastStorage{},row{};
        if(!mappedFirst || !mappedLast) {
            destinationFirstStorage.fill(uint16_t(width));
            for(int px=0;px<width;++px) {
                const auto sx=uint16_t((2*px+1)*_width/(2*width));
                destinationFirstStorage[sx]=std::min(destinationFirstStorage[sx],uint16_t(px));
                destinationLastStorage[sx]=uint16_t(px+1);
            }
        }
        const auto* destinationFirst=mappedFirst?mappedFirst:destinationFirstStorage.data();
        const auto* destinationLast=mappedLast?mappedLast:destinationLastStorage.data();
        const auto* color=colorData();
        int cachedSourceY=-1;std::size_t cachedCount=0;
        for(int py=std::max(0,firstRow);py<=std::min(height-1,lastRow);++py) {
            const int sourceY=mappedSourceY?mappedSourceY[py]:(2*py+1)*_height/(2*height);
            const std::size_t rowStart=std::size_t(sourceY)*_width,rowEnd=rowStart+_width;
            auto* destination=reinterpret_cast<uint16_t*>(nativeFrameBuffer+std::size_t(y+py)*nativeStride)+x;
            if(sourceY!=cachedSourceY) {
                cachedSourceY=sourceY;cachedCount=0;
                const auto* sourceColor=_splitColor && _width==_splitColorWidth && sourceY>=_splitColorRow
                    ? _splitColor+std::size_t(sourceY-_splitColorRow)*_width
                    : color+rowStart;
                const std::size_t firstByte=rowStart>>3,lastByte=(rowEnd-1)>>3;
                for(std::size_t byte=firstByte;byte<=lastByte;++byte) {
                    unsigned bits=_occupiedDepth[byte];
                    if(byte==firstByte)bits&=0xffu<<unsigned(rowStart&7);
                    if(byte==lastByte && (rowEnd&7))bits&=(1u<<unsigned(rowEnd&7))-1u;
                    while(bits) {
                        const unsigned bit=unsigned(__builtin_ctz(bits));bits&=bits-1;
                        const auto sx=uint16_t(byte*8+bit-rowStart);
                        sourceX[cachedCount]=sx;
                        const uint16_t value=sourceColor[sx];
                        row[cachedCount++]=uint16_t((value<<8)|(value>>8));
                    }
                }
            }
            for(std::size_t i=0;i<cachedCount;++i) {
                const auto sx=sourceX[i];
                for(uint16_t px=destinationFirst[sx];px<destinationLast[sx];++px)
                    destination[px]=row[i];
            }
        }
    }
#endif
private:
    template<bool Scale> void blitScaledImpl(lgfx::LGFXBase& canvas,int x,int y,int width,int height,
                                             uint8_t* nativeFrameBuffer,std::size_t nativeStride) {
#ifndef ESP_PLATFORM
        (void)nativeFrameBuffer;(void)nativeStride;
#endif
        std::array<uint16_t,Width> row{},sourceX{};
        if constexpr(Scale)for(int px=0;px<width;++px)sourceX[px]=uint16_t((2*px+1)*_width/(2*width));
        const auto* depth=depthData();const auto* color=colorData();
        const bool deferredOccupancy=_sparseDepthClearFastPath && _deferredSparseDepthRecord;
        const bool fusedOccupancy=deferredOccupancy && width>=_width && height>=_height;
        std::size_t occupancyByte=std::size_t(-1);uint8_t occupancyBits=0;
        const auto recordOccupied=[&](std::size_t index) {
            const auto byte=index>>3;
            if(byte!=occupancyByte) {
                if(occupancyByte!=std::size_t(-1) && occupancyBits)_occupiedDepth[occupancyByte]=occupancyBits;
                occupancyByte=byte;occupancyBits=0;
            }
            occupancyBits|=uint8_t(1u<<(index&7));
        };
        int previousSourceY=-1;
#ifdef ESP_PLATFORM
        int32_t clipX=0,clipY=0,clipW=0,clipH=0;
        canvas.getClipRect(&clipX,&clipY,&clipW,&clipH);
        const bool native=_nativeFrameBufferFastPath && nativeFrameBuffer && nativeStride>=std::size_t(x+width)*2 &&
            canvas.getColorDepth()==16 && canvas.getRotation()==0 &&
            x>=clipX && y>=clipY && x+width<=clipX+clipW && y+height<=clipY+clipH &&
            y>=0;
        const bool direct=!native && _directSpanFastPath && canvas.getColorDepth()==16 && canvas.getRotation()==0 &&
            x>=clipX && y>=clipY && x+width<=clipX+clipW && y+height<=clipY+clipH;
        if(direct)canvas.startWrite();
#endif
#ifdef ESP_PLATFORM
        // During scaled RX-78 rendering the occupancy map is complete before
        // composite. Scan its set pixels in source-row order instead of
        // issuing a PSRAM depth read for every expanded destination pixel.
        if constexpr(Scale)if(native && _sparseCompositeFastPath && _sparseDepthClearFastPath &&
                             !_deferredSparseDepthRecord && _occupiedDepth) {
            blitScaledNativeSparseRows(nativeFrameBuffer,nativeStride,x,y,width,height,0,height-1);
            return;
        }
#endif
        for(int py=0;py<height;++py) {
            const int sourceY=Scale?(2*py+1)*_height/(2*height):py;
            const auto offset=std::size_t(sourceY)*_width;
            const auto* sourceColor=_splitColor && _width==_splitColorWidth && sourceY>=_splitColorRow
                ? _splitColor+std::size_t(sourceY-_splitColorRow)*_width
                : color+offset;
            const auto* sourceDepth=_splitDepth && _width==_splitDepthWidth && sourceY>=_splitDepthRow
                ? _splitDepth+std::size_t(sourceY-_splitDepthRow)*_width
                : depth+offset;
            const bool recordRow=fusedOccupancy && sourceY!=previousSourceY;
            int previousSourceX=-1;
#ifdef ESP_PLATFORM
            if(native) {
                auto* destination=reinterpret_cast<uint16_t*>(nativeFrameBuffer+std::size_t(y+py)*nativeStride)+x;
                const auto store=[&](int px,uint16_t value) {
                    destination[px]=uint16_t((value<<8)|(value>>8));
                };
                if constexpr(Scale) {
                    for(int px=0;px<width;++px) {
                        const auto sx=sourceX[px];
                        const bool visible=sourceDepth[sx]!=0;
                        if(visible) {
                            store(px,sourceColor[sx]);
                            if(recordRow && sx!=previousSourceX)recordOccupied(offset+sx);
                        }
                        previousSourceX=sx;
                    }
                } else {
                    for(int px=0;px<width;++px)if(sourceDepth[px]) {
                        store(px,sourceColor[px]);
                        if(recordRow)recordOccupied(offset+px);
                    }
                }
                previousSourceY=sourceY;
                continue;
            }
#endif
            int start=-1;
            for(int px=0;px<=width;++px) {
                const auto sx=Scale?(px<width?sourceX[px]:0):px;
                const bool visible=px<width && sourceDepth[sx]!=0;
                if(visible) {
                    row[px]=sourceColor[sx];
                    if(recordRow && sx!=previousSourceX)recordOccupied(offset+sx);
                    if(start<0)start=px;
                } else if(start>=0) {
#ifdef ESP_PLATFORM
                    if(direct) {
                        canvas.setAddrWindow(x+start,y+py,px-start,1);
                        canvas.writePixels(reinterpret_cast<const lgfx::rgb565_t*>(row.data()+start),px-start);
                    } else
#endif
                    drawColorSpan(canvas,x+start,y+py,px-start,row.data()+start);
                    start=-1;
                }
                previousSourceX=sx;
            }
            previousSourceY=sourceY;
        }
        if(deferredOccupancy && !fusedOccupancy)
            for(std::size_t index=0;index<std::size_t(_width)*_height;++index)
                if(depthPixel(index))recordOccupied(index);
        if(occupancyByte!=std::size_t(-1) && occupancyBits)_occupiedDepth[occupancyByte]=occupancyBits;
#ifdef ESP_PLATFORM
        if(direct)canvas.endWrite();
#endif
    }
public:
    int width() const {return _width;}
    int height() const {return _height;}
    uint16_t depthAt(int x,int y) const {return depthPixel(std::size_t(y)*_width+x);}
private:
    const RacePaintAtlas* _paintAtlas=nullptr;
    bool _incrementalInterpolation=false;
    bool _solidFastPath=false;
    bool _solidSpanFastPath=false;
    bool _trustedSolidDepthFastPath=false;
    bool _solidQuadFastPath=false;
    bool _directSpanFastPath=false;
    bool _nativeFrameBufferFastPath=false;
    bool _sparseCompositeFastPath=false;
    bool _sparseDepthClearRequested=false,_sparseDepthClearFastPath=false,_lastSparseDepthClear=false;
    bool _sparseDepthSpanClearRequested=false;
    bool _deferredSparseDepthRecord=false;
    std::size_t _trackedDepthPixels=0;
    int _trackedDepthWidth=0;
    uint8_t* _occupiedDepth=nullptr;
    uint16_t* _splitColor=nullptr;
    int _splitColorWidth=0,_splitColorRow=0;
    uint16_t* _splitDepth=nullptr;
    int _splitDepthWidth=0,_splitDepthRow=0;
    std::size_t _occupiedDepthBytes=0;
    using Pixels=std::array<uint16_t,Width*Height>;
    RenderScratch<Pixels> _fastDepth,_fastColor;
    mutable std::array<uint16_t,PencilOcclusion::kCapacity> _occlusionCandidates{};
    RenderScratch<std::array<uint16_t,PencilOcclusion::kCapacity>> _fastOcclusionRows,_fastOcclusionCandidates;
    uint16_t* depthData() {return _fastDepth.get() ? _fastDepth.get()->data() : _depth.data();}
    const uint16_t* depthData() const {return _fastDepth.get() ? _fastDepth.get()->data() : _depth.data();}
    uint16_t& depthPixel(std::size_t index) {
        const std::size_t split=std::size_t(_splitDepthRow)*_width;
        return _splitDepth && _width==_splitDepthWidth && index>=split
            ? _splitDepth[index-split] : depthData()[index];
    }
    uint16_t depthPixel(std::size_t index) const {
        const std::size_t split=std::size_t(_splitDepthRow)*_width;
        return _splitDepth && _width==_splitDepthWidth && index>=split
            ? _splitDepth[index-split] : depthData()[index];
    }
    void clearDepthRange(std::size_t first,std::size_t count) {
        if(!count)return;
        const std::size_t end=first+count,split=std::size_t(_splitDepthRow)*_width;
        if(!_splitDepth || _width!=_splitDepthWidth || end<=split) {
            std::memset(depthData()+first,0,count*sizeof(uint16_t));return;
        }
        if(first<split)std::memset(depthData()+first,0,(split-first)*sizeof(uint16_t));
        const std::size_t lowerFirst=std::max(first,split);
        std::memset(_splitDepth+(lowerFirst-split),0,(end-lowerFirst)*sizeof(uint16_t));
    }
    uint16_t* colorData() {return _fastColor.get() ? _fastColor.get()->data() : _color.data();}
    const uint16_t* colorData() const {return _fastColor.get() ? _fastColor.get()->data() : _color.data();}
    std::array<uint16_t,Width*Height> _depth{};
    std::array<uint16_t,Width*Height> _color{};
    int _x=0,_y=0,_width=Width,_height=Height;
};
} // namespace lets_and_go
