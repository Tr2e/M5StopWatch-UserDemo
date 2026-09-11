#pragma once
#include "color_span.h"
#include "raster_math.h"
#include "render_scratch.h"

#include "track_projection.h"
#include "render_budget.h"
#include "../model/overpass_track.h"
#include "../model/grand_spiral_data.h"
#include <hal/hal.h>
#include <array>
#include <cstring>
#ifdef ESP_PLATFORM
#include <esp_timer.h>
#include <mooncake_log.h>
#endif

namespace lets_and_go {

// Convex frustum-clipped triangles. Keeping inverse depth makes occlusion
// perspective-correct without a full-screen depth buffer or per-frame heap.
struct PencilSurface {
    std::array<TrackScreenPoint, 8u> points{};
    TrackVec3 inverseDepth{}; // 1/z = ax + by + c
    float minX = 0, minY = 0, maxX = 0, maxY = 0;
    uint8_t count = 0;
    int8_t winding = 0; // Reuses padding; zero supports manually authored faces.
    uint16_t color = 0;
};

inline PencilSurface projectPencilSurface(const TrackCamera& camera,
    TrackVec3 a, TrackVec3 b, TrackVec3 c, int width, int height)
{
    std::array<TrackCameraPoint, 8u> polygon{};
    polygon[0] = trackToCamera(camera, a);
    polygon[1] = trackToCamera(camera, b);
    polygon[2] = trackToCamera(camera, c);
    std::size_t count = 3u;
    // Clip BEFORE projection; clamping projected vertices would bend the road.
    for (int plane = 0; plane < 5 && count != 0u; ++plane) {
        const auto side = [&](TrackCameraPoint p) {
            switch (plane) {
                case 0: return p.z - kTrackNearPlane;
                case 1: return camera.focalLength * p.x + camera.principalX * p.z;
                case 2: return (width - 1 - camera.principalX) * p.z - camera.focalLength * p.x;
                case 3: return camera.principalY * p.z - camera.focalLength * p.y;
                default: return (height - 1 - camera.principalY) * p.z + camera.focalLength * p.y;
            }
        };
        const auto input = polygon;
        const std::size_t inputCount = count;
        count = 0u;
        for (std::size_t i = 0; i < inputCount; ++i) {
            const auto from = input[i], to = input[(i + 1u) % inputCount];
            const float d0 = side(from), d1 = side(to);
            if (d0 >= 0.0f) polygon[count++] = from;
            if ((d0 >= 0.0f) != (d1 >= 0.0f)) {
                const float t = d0 / (d0 - d1);
                polygon[count++] = {from.x + (to.x - from.x) * t,
                    from.y + (to.y - from.y) * t, from.z + (to.z - from.z) * t};
            }
        }
    }
    PencilSurface result;
    if (count < 3u) return result;
    for (std::size_t i = 0; i < count; ++i) {
        polygon[i].z = std::max(kTrackNearPlane, polygon[i].z);
        if (!projectTrackPoint(camera, polygon[i], result.points[i])) return {};
    }
    // Clipping can introduce collinear vertices. Find a non-degenerate basis.
    const auto p = result.points[0];
    for (std::size_t i = 1u; i + 1u < count; ++i) {
        const auto q = result.points[i], r = result.points[i + 1u];
        const float det = (q.x - p.x) * (r.y - p.y) - (q.y - p.y) * (r.x - p.x);
        if (std::abs(det) < 0.001f) continue;
        const float d1 = 1.0f / polygon[i].z - 1.0f / polygon[0].z;
        const float d2 = 1.0f / polygon[i + 1u].z - 1.0f / polygon[0].z;
        result.inverseDepth.x = (d1 * (r.y - p.y) - d2 * (q.y - p.y)) / det;
        result.inverseDepth.y = ((q.x - p.x) * d2 - (r.x - p.x) * d1) / det;
        result.inverseDepth.z = 1.0f / polygon[0].z - result.inverseDepth.x * p.x - result.inverseDepth.y * p.y;
        result.count = static_cast<uint8_t>(count);
        break;
    }
    result.minX = result.maxX = p.x;
    result.minY = result.maxY = p.y;
    for (std::size_t i = 1; i < count; ++i) {
        result.minX = std::min(result.minX, result.points[i].x);
        result.maxX = std::max(result.maxX, result.points[i].x);
        result.minY = std::min(result.minY, result.points[i].y);
        result.maxY = std::max(result.maxY, result.points[i].y);
    }
    float area=0;
    for(std::size_t i=0;i<count;++i) {
        const auto a=result.points[i],b=result.points[(i+1)%count];
        area+=a.x*b.y-b.x*a.y;
    }
    result.winding=area>=0 ? 1 : -1;
    return result;
}

inline void fillPencilSurface(lgfx::LGFXBase& canvas, const PencilSurface& surface, uint16_t color)
{
    for (std::size_t i = 1; i + 1u < surface.count; ++i) {
        const auto a = surface.points[0], b = surface.points[i], c = surface.points[i + 1u];
        canvas.fillTriangle(std::lround(a.x), std::lround(a.y), std::lround(b.x),
            std::lround(b.y), std::lround(c.x), std::lround(c.y), color);
    }
}

inline bool pencilHiddenInterval(const PencilSurface& surface,
    TrackScreenPoint a, TrackScreenPoint b, float inverseA, float inverseB,
    float& enter, float& leave)
{
    if (surface.count < 3u || std::max(a.x, b.x) < surface.minX ||
        std::min(a.x, b.x) > surface.maxX || std::max(a.y, b.y) < surface.minY ||
        std::min(a.y, b.y) > surface.maxY) return false;
    enter = 0.0f; leave = 1.0f;
    const auto clip = [&](float d0, float d1) {
        if (d0 < 0 && d1 < 0) return false;
        if ((d0 < 0) != (d1 < 0)) {
            const float t = d0 / (d0 - d1);
            if (d0 < 0) enter = std::max(enter, t);
            else leave = std::min(leave, t);
        }
        return enter <= leave;
    };
    const auto& d = surface.inverseDepth;
    if (!clip(d.x * a.x + d.y * a.y + d.z - inverseA - 0.0001f,
              d.x * b.x + d.y * b.y + d.z - inverseB - 0.0001f)) return false;
    float orientation=surface.winding;
    if(orientation==0) {
        float area = 0.0f;
        for (std::size_t i = 0; i < surface.count; ++i) {
            const auto p = surface.points[i], q = surface.points[(i + 1u) % surface.count];
            area += p.x * q.y - p.y * q.x;
        }
        orientation=area>=0 ? 1.f : -1.f;
    }
    for (std::size_t i = 0; i < surface.count; ++i) {
        const auto p = surface.points[i], q = surface.points[(i + 1u) % surface.count];
        const auto side = [&](TrackScreenPoint v) {
            return orientation * ((q.x - p.x) * (v.y - p.y) - (q.y - p.y) * (v.x - p.x));
        };
        if (!clip(side(a), side(b))) return false;
    }
    return true;
}

struct PencilTrack {
    static constexpr std::size_t kSegments = 96u;
    static constexpr std::size_t kMaximumSegments = 160u;
    std::array<TrackVec3, kMaximumSegments + 1u> left{}, right{};
    struct Bound { TrackVec3 center; float radius; };
    std::array<Bound,kMaximumSegments> bounds{};
    std::size_t count=kSegments;
    bool cullSegments=false;
    void open(const OverpassTrack& track) {
        count=track.id()==TrackId::GrandSpiral ? kMaximumSegments : kSegments;
        cullSegments=track.id()==TrackId::GrandSpiral;
        for (std::size_t i = 0; i <= count; ++i) {
            const float s = track.id()==TrackId::GrandSpiral ? grand_spiral::kRenderDistances[i] : track.length() * i / count;
            left[i] = track.edge(s, -1.0f); right[i] = track.edge(s, 1.0f);
        }
        if(cullSegments)for(std::size_t i=0;i<count;++i) {
            const auto center=trackScale(trackAdd(trackAdd(left[i],right[i]),trackAdd(left[i+1],right[i+1])),.25f);
            float radius=0;
            for(auto p:{left[i],right[i],left[i+1],right[i+1]})radius=std::max(radius,trackLength(trackSubtract(p,center)));
            bounds[i]={center,radius};
        }
    }
    // Return the exact piecewise-triangular surface used by drawPencilTrack.
    // Vehicle placement must follow this mesh rather than the analytic
    // centreline, otherwise a rigid car can enter a downhill chord or the
    // start/finish seam while the road still looks continuous.
    TrackVec3 roadPoint(const OverpassTrack& track,float distance,float lateralOffset) const {
        const float length=track.length();
        float wrapped=std::fmod(distance,length);
        if(wrapped<0)wrapped+=length;
        std::size_t segment=0;
        float along=0;
        if(track.id()==TrackId::GrandSpiral) {
            const auto upper=std::upper_bound(grand_spiral::kRenderDistances.begin(),
                grand_spiral::kRenderDistances.end(),wrapped);
            segment=std::min<std::size_t>(
                std::max<std::ptrdiff_t>(0,upper-grand_spiral::kRenderDistances.begin()-1),count-1);
            const float from=grand_spiral::kRenderDistances[segment];
            const float span=grand_spiral::kRenderDistances[segment+1]-from;
            along=span>.00001f ? (wrapped-from)/span : 0;
        } else {
            const float scaled=wrapped*count/length;
            segment=std::min<std::size_t>(std::size_t(scaled),count-1);
            along=scaled-segment;
        }
        const float across=std::clamp(.5f+.5f*lateralOffset/OverpassTrack::kHalfWidth,0.f,1.f);
        const auto a=left[segment],b=right[segment];
        const auto c=right[segment+1],d=left[segment+1];
        // The road is split a-b-c and a-c-d. Interpolate the matching triangle
        // so twisted/banked quads produce the same height as the depth planes.
        return across>=along
            ? trackAdd(a,trackAdd(trackScale(trackSubtract(b,a),across),
                                  trackScale(trackSubtract(c,b),along)))
            : trackAdd(a,trackAdd(trackScale(trackSubtract(c,d),across),
                                  trackScale(trackSubtract(d,a),along)));
    }
};

struct PencilOcclusion {
    // Two deck triangles and two solid outer walls. No central dividers.
    static constexpr std::size_t kCapacity=PencilTrack::kMaximumSegments*6u+48u;
    std::array<PencilSurface, kCapacity> surfaces{};
    // One scanline, not a full-screen depth buffer. Kept off the task stack.
    std::array<float,466> rowDepth{};
    std::array<uint16_t,466> rowColor{};
    struct Rows {std::array<float,466> depth;std::array<uint16_t,466> color;};
    RenderScratch<Rows> fastRows;
    std::array<uint16_t,kCapacity> lineCandidates{};
    RenderScratch<std::array<uint16_t,kCapacity>> fastLineCandidates;
    bool preferInternalMemory() {fastLineCandidates.allocate();return fastRows.allocate();}
    std::size_t count = 0u;
    bool overflowed = false;

    void append(const PencilSurface& surface) {
        if(surface.count==0u) return;
        if(count==surfaces.size()) { overflowed=true; return; }
        surfaces[count++]=surface;
    }

    void paint(lgfx::LGFXBase& canvas) {
        if(count==0)return;
        auto& rowDepth=fastRows.get() ? fastRows.get()->depth : this->rowDepth;
        auto& rowColor=fastRows.get() ? fastRows.get()->color : this->rowColor;
        float minY=float(canvas.height()),maxY=0;
        for(std::size_t face=0;face<count;++face) {
            minY=std::min(minY,surfaces[face].minY);maxY=std::max(maxY,surfaces[face].maxY);
        }
        // Coverage is minY <= y+.5 < maxY. Empty screen rows need no clear/copy.
        const int firstY=int(std::clamp(rasterCeil(minY-.5f),0.f,float(canvas.height())));
        const int endY=int(std::clamp(rasterCeil(maxY-.5f),0.f,float(canvas.height())));
#ifdef ESP_PLATFORM
        uint64_t clearUs=0,fillUs=0,copyUs=0;
#endif
        for(int offset=0;offset<canvas.width();offset+=int(rowDepth.size())) {
            const int width=std::min(int(rowDepth.size()),int(canvas.width())-offset);
            for(int y=firstY;y<endY;++y) {
#ifdef ESP_PLATFORM
                const uint64_t startUs=esp_timer_get_time();
#endif
                std::memset(rowDepth.data(),0,rowDepth.size()*sizeof(float));
#ifdef ESP_PLATFORM
                const uint64_t clearedUs=esp_timer_get_time();
                clearUs+=clearedUs-startUs;
#endif
                const float scanY=y+.5f;
                for(std::size_t face=0;face<count;++face) {
                    const auto& s=surfaces[face];
                    if(scanY<s.minY || scanY>=s.maxY || s.maxX<offset || s.minX>=offset+width)continue;
                    float left=float(canvas.width()),right=-1;
                    for(unsigned i=0;i<s.count;++i) {
                        const auto a=s.points[i],b=s.points[(i+1)%s.count];
                        if((a.y<=scanY && b.y>scanY) || (b.y<=scanY && a.y>scanY)) {
                            const float x=a.x+(b.x-a.x)*(scanY-a.y)/(b.y-a.y);
                            left=std::min(left,x);right=std::max(right,x);
                        }
                    }
                    const int x0=std::max(offset,int(rasterCeil(left-.5f)));
                    const int x1=std::min(offset+width-1,int(rasterFloor(right-.5f)));
                    const auto& d=s.inverseDepth;
                    float z=d.x*(x0+.5f)+d.y*scanY+d.z;
                    for(int x=x0;x<=x1;++x,z+=d.x) if(z>rowDepth[x-offset]) {
                        rowDepth[x-offset]=z;rowColor[x-offset]=s.color;
                    }
                }
#ifdef ESP_PLATFORM
                const uint64_t filledUs=esp_timer_get_time();
                fillUs+=filledUs-clearedUs;
#endif
                for(int x=0;x<width;) {
                    if(rowDepth[x]<=0) {++x;continue;}
                    const int start=x++;
                    while(x<width && rowDepth[x]>0)++x;
                    drawColorSpan(canvas,offset+start,y,x-start,rowColor.data()+start);
                }
#ifdef ESP_PLATFORM
                copyUs+=esp_timer_get_time()-filledUs;
#endif
            }
        }
#ifdef ESP_PLATFORM
        static uint64_t lastLogUs=0;
        const uint64_t nowUs=esp_timer_get_time();
        if(nowUs-lastLogUs>=2000000u) {
            lastLogUs=nowUs;
            mclog::tagInfo("PaintStage","rows={} clear_us={} fill_us={} copy_us={}",
                endY-firstY,uint32_t(clearUs),uint32_t(fillUs),uint32_t(copyUs));
        }
#endif
    }

    void drawLine(lgfx::LGFXBase& canvas, const TrackCamera& camera,
                  TrackVec3 from, TrackVec3 to, uint16_t color,
                  const uint16_t* candidates=nullptr,std::size_t candidateCount=0) const {
        auto ca = trackToCamera(camera, from), cb = trackToCamera(camera, to);
        if (!clipTrackSegmentToNear(ca, cb)) return;
        TrackScreenPoint a{}, b{};
        if (!projectTrackPoint(camera, ca, a) || !projectTrackPoint(camera, cb, b)) return;
        // Reject off-screen lines before the surface visibility walk. Keep the
        // original endpoints for perspective-correct inverse-depth clipping.
        if(std::max(a.x,b.x)<0 || std::min(a.x,b.x)>canvas.width()-1 ||
           std::max(a.y,b.y)<0 || std::min(a.y,b.y)>canvas.height()-1)return;
        struct Interval { float from, to; };
        std::array<Interval, 16u> visible{};
        visible[0] = {0, 1};
        std::size_t pieces = 1u;
        const auto faceCount=candidates ? candidateCount : count;
        for (std::size_t i = 0; i < faceCount && pieces != 0u; ++i) {
            float enter, leave;
            if (!pencilHiddenInterval(surfaces[candidates ? candidates[i] : i], a, b,
                                     1 / ca.z, 1 / cb.z, enter, leave)) continue;
            for (std::size_t p = 0; p < pieces;) {
                const auto interval = visible[p];
                if (leave <= interval.from || enter >= interval.to) { ++p; continue; }
                if (enter > interval.from && leave < interval.to) {
                    if (pieces == visible.size()) return; // Conservative, bounded fragmentation.
                    visible[pieces++] = {leave, interval.to};
                    visible[p++].to = enter;
                } else if (enter <= interval.from && leave >= interval.to) {
                    visible[p] = visible[--pieces];
                } else {
                    if (enter <= interval.from) visible[p].from = leave;
                    else visible[p].to = enter;
                    ++p;
                }
            }
        }
        for (std::size_t i = 0; i < pieces; ++i) {
            TrackScreenPoint p{a.x + (b.x - a.x) * visible[i].from, a.y + (b.y - a.y) * visible[i].from};
            TrackScreenPoint q{a.x + (b.x - a.x) * visible[i].to, a.y + (b.y - a.y) * visible[i].to};
            if (clipTrackSegmentToViewport(p, q, canvas.width() - 1, canvas.height() - 1))
                canvas.drawLine(std::lround(p.x), std::lround(p.y), std::lround(q.x), std::lround(q.y), color);
        }
    }
};

namespace track_paint {
inline constexpr uint16_t road=0xe73cu, roadLight=0xef7du;
inline constexpr uint16_t chalk=0xf79eu, coral=0xc9c8u, blue=0x2b95u;
inline constexpr uint16_t edge=0x4269u, fascia=0xa575u, underside=0x526du;
inline constexpr uint16_t night=0x10e4u, floor=0x1926u, ridge=0x29a8u;
inline constexpr float wallHeight=.26f, deckThickness=.22f;

struct ModulePaint { uint16_t deck, wall, rim, seam; };
inline ModulePaint module(std::size_t segment,std::size_t count=PencilTrack::kSegments)
{
    segment=segment*PencilTrack::kSegments/count;
    // JCJC 94892: white straights/bridge, one red bend and one blue bend.
    // The product's three lanes become one open driving surface in this game.
    if(segment>=12 && segment<36) return {coral,0x9986u,0xeb4eu,0xa987u};
    if(segment>=60 && segment<84) return {blue,0x19edu,0x651bu,0x2271u};
    return {road,fascia,chalk,0xbdf8u};
}

inline uint16_t wireShade(uint16_t color,unsigned brightness)
{
    // Integer RGB565 shading preserves each module's original hue.
    return uint16_t(((((color>>11)&31)*brightness/256)<<11) |
                    ((((color>>5)&63)*brightness/256)<<5) |
                    ((color&31)*brightness/256));
}

inline void backdrop(lgfx::LGFXBase& canvas,PencilDetail detail,bool scenery=true)
{
    canvas.fillScreen(night);
    const int horizon=canvas.height()*162/466;
    canvas.fillRect(0,horizon,canvas.width(),canvas.height()-horizon,floor);
    if(!scenery)return;
    constexpr std::array<int,9> heights{{150,137,146,117,143,129,149,132,151}};
    for(unsigned i=0;i+1<heights.size();++i) {
        const int x=int(i)*canvas.width()/8,next=int(i+1)*canvas.width()/8;
        canvas.fillTriangle(x,horizon,x,heights[i],next,heights[i+1],0x1925u);
        canvas.fillTriangle(x,horizon,next,heights[i+1],next,horizon,0x1925u);
        if(detail!=PencilDetail::Low)
            canvas.drawLine(x,heights[i],next,heights[i+1],0x2147u);
    }
    canvas.drawLine(0,horizon,canvas.width()-1,horizon,ridge);
}

inline TrackVec3 mix(TrackVec3 a,TrackVec3 b,float t) {
    return trackAdd(a,trackScale(trackSubtract(b,a),t));
}
inline void line(lgfx::LGFXBase& canvas,const TrackCamera& camera,
                 TrackVec3 a,TrackVec3 b,uint16_t color) {
    auto ca=trackToCamera(camera,a),cb=trackToCamera(camera,b);
    TrackScreenPoint p{},q{};
    if(clipTrackSegmentToNear(ca,cb) && projectTrackPoint(camera,ca,p) &&
       projectTrackPoint(camera,cb,q) &&
       clipTrackSegmentToViewport(p,q,canvas.width()-1,canvas.height()-1))
        canvas.drawLine(std::lround(p.x),std::lround(p.y),std::lround(q.x),std::lround(q.y),color);
}
inline void quad(lgfx::LGFXBase& canvas,const TrackCamera& camera,
                 TrackVec3 a,TrackVec3 b,TrackVec3 c,TrackVec3 d,uint16_t color,
                 PencilOcclusion* occlusion=nullptr) {
    for(const auto& face : {projectPencilSurface(camera,a,b,c,canvas.width(),canvas.height()),
                          projectPencilSurface(camera,a,c,d,canvas.width(),canvas.height())}) {
        fillPencilSurface(canvas,face,color);
        if(occlusion) occlusion->append(face);
    }
}
} // namespace track_paint

// An overview-only ground wash and sparse bridge bents. No supports are placed
// at the crossing itself, so the lower carriageway stays visibly unobstructed.
inline void drawPencilTrackGround(lgfx::LGFXBase& canvas,const TrackCamera& camera,
                                  const PencilTrack& track,PencilDetail detail)
{
    using namespace track_paint;
    const auto ground=[](TrackVec3 p) { return TrackVec3{p.x+.45f,.03f,p.z+.35f}; };
    for(std::size_t i=0;i<track.count;++i)
        quad(canvas,camera,ground(track.left[i]),ground(track.right[i]),
             ground(track.right[i+1]),ground(track.left[i+1]),0x10c3u);
    if(detail==PencilDetail::Low) return;
    for(std::size_t i=0;i<track.count;i+=12) {
        const auto center=mix(track.left[i],track.right[i],.5f);
        if(center.y<1.8f || center.x*center.x+center.z*center.z<20.f) continue;
        for(const auto p : {track.left[i],track.right[i]}) {
            // Crossings are no longer necessarily at the origin. Reject a bent
            // above any lower ribbon, including the full carriageway width.
            bool blocksRoad=false;
            for(std::size_t j=0;j<track.count;++j) {
                const auto a=mix(track.left[j],track.right[j],.5f);
                const auto b=mix(track.left[j+1],track.right[j+1],.5f);
                const float dx=b.x-a.x,dz=b.z-a.z;
                const float t=std::clamp(((p.x-a.x)*dx+(p.z-a.z)*dz)/std::max(.001f,dx*dx+dz*dz),0.f,1.f);
                const auto q=mix(a,b,t);
                if(q.y<p.y-.6f && std::hypot(q.x-p.x,q.z-p.z)<OverpassTrack::kHalfWidth+.45f)
                    blocksRoad=true;
            }
            if(blocksRoad)continue;
            const TrackVec3 top{p.x,p.y-.22f,p.z},foot{p.x,.05f,p.z};
            const TrackVec3 width=trackScale(trackNormalize(trackSubtract(track.right[i],track.left[i])),.16f);
            quad(canvas,camera,trackSubtract(foot,width),trackSubtract(top,width),
                 trackAdd(top,width),trackAdd(foot,width),0x738fu);
            line(canvas,camera,trackAdd(top,width),trackAdd(foot,width),0xbdf7u);
            const TrackVec3 base{.28f,0,.18f};
            quad(canvas,camera,trackSubtract(foot,base),{foot.x+.28f,foot.y,foot.z-.18f},
                 trackAdd(foot,base),{foot.x-.28f,foot.y,foot.z+.18f},0x4a4bu);
        }
    }
}

inline bool pencilSegmentVisible(const TrackCamera& camera,const PencilTrack& track,std::size_t i,
                                int width,int height,const std::array<float,4>& planeScale)
{
    if(!track.cullSegments)return true;
    const auto& bound=track.bounds[i];
    const float radius=bound.radius+std::max(track_paint::wallHeight,track_paint::deckThickness)+.01f;
    const auto p=trackToCamera(camera,bound.center);
    if(p.z+radius<kTrackNearPlane)return false;
    const float f=camera.focalLength;
    return f*p.x+camera.principalX*p.z >= -radius*planeScale[0] &&
           -f*p.x+(width-1-camera.principalX)*p.z >= -radius*planeScale[1] &&
           -f*p.y+camera.principalY*p.z >= -radius*planeScale[2] &&
           f*p.y+(height-1-camera.principalY)*p.z >= -radius*planeScale[3];
}

inline void drawPencilTrack(lgfx::LGFXBase& canvas, const TrackCamera& camera,
    const PencilTrack& track, PencilDetail detail, PencilOcclusion* occlusion,bool decorations=true,
    bool wireframe=false)
{
    using namespace track_paint;
    if(!occlusion)return;
#ifdef ESP_PLATFORM
    const uint64_t startUs=esp_timer_get_time();
#endif
    occlusion->count=0;occlusion->overflowed=false;
    const std::array<float,4> planeScale{{std::hypot(camera.focalLength,camera.principalX),
        std::hypot(camera.focalLength,canvas.width()-1-camera.principalX),
        std::hypot(camera.focalLength,camera.principalY),
        std::hypot(camera.focalLength,canvas.height()-1-camera.principalY)}};
    const auto surface=[&](TrackVec3 a,TrackVec3 b,TrackVec3 c,TrackVec3 d,uint16_t color) {
        for(auto face : {projectPencilSurface(camera,a,b,c,canvas.width(),canvas.height()),
                        projectPencilSurface(camera,a,c,d,canvas.width(),canvas.height())}) {
            face.color=color;occlusion->append(face);
        }
    };
    // Resolve ALL solid surfaces by inverse depth. Sorting whole segments was
    // insufficient: a neighbouring deck could erase the top of a raised wall.
    for(std::size_t i=0;i<track.count;++i) {
        if(!pencilSegmentVisible(camera,track,i,canvas.width(),canvas.height(),planeScale))continue;
        const auto a=track.left[i],b=track.right[i],c=track.right[i+1],d=track.left[i+1];
        const TrackVec3 drop{0,-deckThickness,0},lift{0,wallHeight,0};
        const auto paint=module(i,track.count);
        const auto across=trackSubtract(b,a);
        const auto normal=trackCross(trackSubtract(d,a),across);
        const bool above=trackDot(normal,trackSubtract(camera.position,a))>=0;
        const auto wall=[&](TrackVec3 p,TrackVec3 q) {
            surface(trackAdd(p,drop),trackAdd(q,drop),trackAdd(q,lift),trackAdd(p,lift),paint.wall);
        };
        wall(a,d);wall(b,c);
        // A banked/ramping quad is generally twisted. Its two triangles can
        // face opposite sides of the camera; sharing one normal opens a hole.
        const auto deckTriangle=[&](TrackVec3 p,TrackVec3 q,TrackVec3 r) {
            const auto normal=trackCross(trackSubtract(r,p),trackSubtract(q,p));
            const bool top=trackDot(normal,trackSubtract(camera.position,p))>=0;
            if(!top) {p=trackAdd(p,drop);q=trackAdd(q,drop);r=trackAdd(r,drop);}
            auto face=projectPencilSurface(camera,p,q,r,canvas.width(),canvas.height());
            face.color=top ? paint.deck : underside;occlusion->append(face);
        };
        deckTriangle(a,b,c);deckTriangle(a,c,d);
        if(above && i==0) {
            // A two-row chequered band with real area (the old marker was one
            // pixel-thin line). Its rear edge is exactly the common finish line.
            for(int row=0;row<2;++row) for(int cell=0;cell<12;++cell) {
                const float t0=row*.16f,t1=(row+1)*.16f;
                const auto left0=mix(a,d,t0),right0=mix(b,c,t0);
                const auto left1=mix(a,d,t1),right1=mix(b,c,t1);
                const float u0=.065f+.87f*cell/12,u1=.065f+.87f*(cell+1)/12;
                // Small physical lift prevents coplanar depth ties. It doesn't
                // move the timing line, whose rear edge remains at distance 0.
                const TrackVec3 decal{0,.001f,0};
                surface(trackAdd(mix(left0,right0,u0),decal),trackAdd(mix(left0,right0,u1),decal),
                        trackAdd(mix(left1,right1,u1),decal),trackAdd(mix(left1,right1,u0),decal),
                        (row+cell)%2 ? chalk : edge);
            }
        }
    }
#ifdef ESP_PLATFORM
    const uint64_t projectUs=esp_timer_get_time();
#endif
    // Hidden-line wireframe keeps the exact solid visibility geometry for cars
    // and bridges, but avoids the road's scanline depth/fill/copy pass.
    if(!wireframe)occlusion->paint(canvas);
#ifdef ESP_PLATFORM
    const uint64_t paintUs=esp_timer_get_time();
#endif
    // Details also use the scene visibility test; bridge joints and far rims
    // must not leak through the near carriageway or through the bridge bottom.
    auto& lineCandidates=occlusion->fastLineCandidates.get() ? *occlusion->fastLineCandidates.get() : occlusion->lineCandidates;
    const uint16_t* candidates=nullptr;
    std::size_t candidateCount=0;
#ifdef ESP_PLATFORM
    uint32_t wireSections=0,nearSections=0,wireLines=0,wireCandidates=0;
#endif
    const auto stroke=[&](TrackVec3 a,TrackVec3 b,uint16_t color) {
#ifdef ESP_PLATFORM
        if(wireframe) {++wireLines;wireCandidates+=candidates ? candidateCount : occlusion->count;}
#endif
        occlusion->drawLine(canvas,camera,a,b,color,candidates,candidateCount);
    };
    if(wireframe) for(std::size_t i=0;i<track.count;++i) {
        const auto a=track.left[i],b=track.right[i],c=track.right[i+1],d=track.left[i+1];
        const TrackVec3 lift{0,wallHeight,0},drop{0,-deckThickness,0};
        // Project the near-clipped convex hull of the section's eight corners.
        // Intersect every front/back pair: even twisted track quads remain
        // conservatively enclosed, without reverting to the whole screen.
        std::array<TrackCameraPoint,8> corners;
        unsigned cornerCount=0,frontCount=0;
        float minX=INFINITY,maxX=-INFINITY,minY=INFINITY,maxY=-INFINITY,maxDepth=0;
        const auto bound=[&](TrackCameraPoint p) {
            TrackScreenPoint screen;
            if(!projectTrackPoint(camera,p,screen))return;
            minX=std::min(minX,screen.x);maxX=std::max(maxX,screen.x);
            minY=std::min(minY,screen.y);maxY=std::max(maxY,screen.y);
        };
        for(auto p : {a,b,c,d})for(auto offset : {drop,lift}) {
            const auto point=trackToCamera(camera,trackAdd(p,offset));
            corners[cornerCount++]=point;
            maxDepth=std::max(maxDepth,point.z);
            if(point.z>=kTrackNearPlane) {++frontCount;bound(point);}
        }
        if(frontCount==0)continue;
        if(frontCount<corners.size()) {
#ifdef ESP_PLATFORM
            ++nearSections;
#endif
            for(unsigned from=0;from<corners.size();++from)
                for(unsigned to=from+1;to<corners.size();++to) {
                    auto p=corners[from],q=corners[to];
                    if((p.z>=kTrackNearPlane)==(q.z>=kTrackNearPlane))continue;
                    if(clipTrackSegmentToNear(p,q)) {bound(p);bound(q);}
                }
        }
        if(maxX<0 || minX>canvas.width()-1 || maxY<0 || minY>canvas.height()-1)continue;
        // Subpixel slack makes the box conservative under float rounding.
        minX=std::max(0.f,minX-.01f);maxX=std::min(float(canvas.width()-1),maxX+.01f);
        minY=std::max(0.f,minY-.01f);maxY=std::min(float(canvas.height()-1),maxY+.01f);
        candidateCount=0;
        const float inverseFar=1.f/maxDepth;
        for(std::size_t face=0;face<occlusion->count;++face) {
            const auto& s=occlusion->surfaces[face];
            const float left=std::max(minX,s.minX),right=std::min(maxX,s.maxX);
            const float top=std::max(minY,s.minY),bottom=std::min(maxY,s.maxY);
            if(left>right || top>bottom)continue;
            const auto& z=s.inverseDepth;
            // The largest inverse depth over this rectangle bounds the face.
            // A face entirely behind the section cannot hide any of its lines.
            const float nearest=z.x*(z.x>=0 ? right : left)+
                                z.y*(z.y>=0 ? bottom : top)+z.z;
            if(nearest<=inverseFar-.0001f)continue;
            lineCandidates[candidateCount++]=uint16_t(face);
        }
        candidates=lineCandidates.data();
#ifdef ESP_PLATFORM
        ++wireSections;
#endif
        const float depth=trackToCamera(camera,mix(mix(a,b,.5f),mix(d,c,.5f),.5f)).z;
        // Use the same white/red/blue modules as the solid road and minimap.
        // Depth changes brightness only; grid spacing stays in world space.
        const bool near=depth<10.f,mid=depth<22.f;
        const auto paint=module(i,track.count);
        const uint16_t rim=wireShade(paint.rim,near ? 256 : mid ? 208 : 160);
        const uint16_t grid=wireShade(paint.deck,near ? 184 : mid ? 144 : 112);
        stroke(trackAdd(a,lift),trackAdd(d,lift),rim);
        stroke(trackAdd(b,lift),trackAdd(c,lift),rim);
        stroke(a,d,grid);stroke(b,c,grid);
        stroke(trackAdd(a,drop),trackAdd(d,drop),grid);
        stroke(trackAdd(b,drop),trackAdd(c,drop),grid);
        for(float u : {.25f,.5f,.75f}) {
            if(!near && u!=.5f)continue;
            stroke(mix(a,b,u),mix(d,c,u),grid);
        }
        const std::size_t spacing=near ? 1u : mid ? 2u : 4u;
        if(i%spacing==0)stroke(a,b,grid);
        if(i%(spacing*2)==0) {
            stroke(trackAdd(a,drop),trackAdd(a,lift),grid);
            stroke(trackAdd(b,drop),trackAdd(b,lift),grid);
        }
        // Finish marker remains at the physical timing line, with a second
        // crossbar and short divisions instead of filled checkerboard cells.
        if(i==0) {
            const TrackVec3 decal{0,.002f,0};
            const auto left=trackAdd(a,decal),right=trackAdd(b,decal);
            const auto leftEnd=trackAdd(mix(a,d,.32f),decal);
            const auto rightEnd=trackAdd(mix(b,c,.32f),decal);
            stroke(left,right,chalk);stroke(leftEnd,rightEnd,chalk);
            for(int cell=0;cell<=12;++cell)
                stroke(mix(left,right,cell/12.f),mix(leftEnd,rightEnd,cell/12.f),chalk);
        }
    }
    for(std::size_t i=0;i<track.count && decorations && !wireframe;++i) {
        const auto a=track.left[i],b=track.right[i],c=track.right[i+1],d=track.left[i+1];
        const auto paint=module(i,track.count);
        const TrackVec3 lift{0,wallHeight,0};
        stroke(trackAdd(a,lift),trackAdd(d,lift),paint.rim);
        stroke(trackAdd(b,lift),trackAdd(c,lift),paint.rim);
        if(detail==PencilDetail::Low)continue;
        if(i%4==0) {
            stroke(a,b,paint.seam);
            stroke(a,trackAdd(a,lift),paint.seam);
            stroke(b,trackAdd(b,lift),paint.seam);
        }
        if(detail==PencilDetail::High) {
            stroke(mix(a,b,.045f),mix(d,c,.045f),paint.rim);
            if(i%6==3) for(float side : {.13f,.87f}) {
                const auto tail=mix(mix(a,b,side),mix(d,c,side),.3f);
                const auto tip=mix(mix(a,b,side),mix(d,c,side),.8f);
                const auto wing=trackScale(trackSubtract(b,a),.022f);
                stroke(trackAdd(tail,wing),tip,paint.rim);
                stroke(trackSubtract(tail,wing),tip,paint.rim);
            }
        }
    }
#ifdef ESP_PLATFORM
    const uint64_t endUs=esp_timer_get_time();
    struct WirePeak {
        uint32_t us=0,sections=0,near=0,lines=0,candidates=0,surfaces=0;
        TrackVec3 camera{};
    };
    static WirePeak peak;
    if(wireframe && endUs-startUs>peak.us)
        peak={uint32_t(endUs-startUs),wireSections,nearSections,wireLines,wireCandidates,
              uint32_t(occlusion->count),camera.position};
    static uint64_t lastLogUs=0;
    if(endUs-lastLogUs>=2000000u) {
        lastLogUs=endUs;
        mclog::tagInfo("TrackStage","project_us={} paint_us={} lines_us={} wireframe={}",
            uint32_t(projectUs-startUs),uint32_t(paintUs-projectUs),uint32_t(endUs-paintUs),wireframe);
        if(wireframe) {
            mclog::tagInfo("WirePeak","track_us={} sections={} near_sections={} strokes={} candidate_budget={} surfaces={} camera_x={} camera_y={} camera_z={}",
                peak.us,peak.sections,peak.near,peak.lines,peak.candidates,peak.surfaces,
                peak.camera.x,peak.camera.y,peak.camera.z);
            peak={};
        }
    }
#endif
}

} // namespace lets_and_go
