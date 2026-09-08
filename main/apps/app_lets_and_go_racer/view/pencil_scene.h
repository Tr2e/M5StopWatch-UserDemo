#pragma once

#include "track_projection.h"
#include "render_budget.h"
#include "../model/overpass_track.h"
#include <hal/hal.h>
#include <array>

namespace lets_and_go {

// Convex frustum-clipped triangles. Keeping inverse depth makes occlusion
// perspective-correct without a full-screen depth buffer or per-frame heap.
struct PencilSurface {
    std::array<TrackScreenPoint, 8u> points{};
    TrackVec3 inverseDepth{}; // 1/z = ax + by + c
    float minX = 0, minY = 0, maxX = 0, maxY = 0;
    uint8_t count = 0;
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
    return result;
}

inline void fillPencilSurface(LGFX_Sprite& canvas, const PencilSurface& surface, uint16_t color)
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
    float area = 0.0f;
    for (std::size_t i = 0; i < surface.count; ++i) {
        const auto p = surface.points[i], q = surface.points[(i + 1u) % surface.count];
        area += p.x * q.y - p.y * q.x;
    }
    const float orientation = area >= 0 ? 1.0f : -1.0f;
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
    std::array<TrackVec3, kSegments + 1u> left{}, right{};
    void open(const OverpassTrack& track) {
        for (std::size_t i = 0; i <= kSegments; ++i) {
            const float s = track.length() * i / kSegments;
            left[i] = track.edge(s, -1.0f); right[i] = track.edge(s, 1.0f);
        }
    }
};

struct PencilOcclusion {
    // Two deck triangles plus at most one camera-facing fascia (two triangles).
    std::array<PencilSurface, PencilTrack::kSegments * 4u> surfaces{};
    std::size_t count = 0u;
    bool overflowed = false;

    void append(const PencilSurface& surface) {
        if(surface.count==0u) return;
        if(count==surfaces.size()) { overflowed=true; return; }
        surfaces[count++]=surface;
    }

    void drawLine(LGFX_Sprite& canvas, const TrackCamera& camera,
                  TrackVec3 from, TrackVec3 to, uint16_t color) const {
        auto ca = trackToCamera(camera, from), cb = trackToCamera(camera, to);
        if (!clipTrackSegmentToNear(ca, cb)) return;
        TrackScreenPoint a{}, b{};
        if (!projectTrackPoint(camera, ca, a) || !projectTrackPoint(camera, cb, b)) return;
        struct Interval { float from, to; };
        std::array<Interval, 16u> visible{};
        visible[0] = {0, 1};
        std::size_t pieces = 1u;
        for (std::size_t i = 0; i < count && pieces != 0u; ++i) {
            float enter, leave;
            if (!pencilHiddenInterval(surfaces[i], a, b, 1 / ca.z, 1 / cb.z, enter, leave)) continue;
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
inline constexpr uint16_t road=0xce99u, roadLight=0xd6dau;
inline constexpr uint16_t chalk=0xef7du, coral=0xbac9u, teal=0x4bafu;
inline constexpr uint16_t edge=0x4269u, fascia=0x8c91u, underside=0x6390u;

inline TrackVec3 mix(TrackVec3 a,TrackVec3 b,float t) {
    return trackAdd(a,trackScale(trackSubtract(b,a),t));
}
inline void line(LGFX_Sprite& canvas,const TrackCamera& camera,
                 TrackVec3 a,TrackVec3 b,uint16_t color) {
    auto ca=trackToCamera(camera,a),cb=trackToCamera(camera,b);
    TrackScreenPoint p{},q{};
    if(clipTrackSegmentToNear(ca,cb) && projectTrackPoint(camera,ca,p) &&
       projectTrackPoint(camera,cb,q) &&
       clipTrackSegmentToViewport(p,q,canvas.width()-1,canvas.height()-1))
        canvas.drawLine(std::lround(p.x),std::lround(p.y),std::lround(q.x),std::lround(q.y),color);
}
inline void quad(LGFX_Sprite& canvas,const TrackCamera& camera,
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
inline void drawPencilTrackGround(LGFX_Sprite& canvas,const TrackCamera& camera,
                                  const PencilTrack& track,PencilDetail detail)
{
    using namespace track_paint;
    const auto ground=[](TrackVec3 p) { return TrackVec3{p.x+.45f,.03f,p.z+.35f}; };
    for(std::size_t i=0;i<PencilTrack::kSegments;++i)
        quad(canvas,camera,ground(track.left[i]),ground(track.right[i]),
             ground(track.right[i+1]),ground(track.left[i+1]),0xded6u);
    if(detail==PencilDetail::Low) return;
    for(std::size_t i=0;i<PencilTrack::kSegments;i+=12) {
        const auto center=mix(track.left[i],track.right[i],.5f);
        if(center.y<1.8f || center.x*center.x+center.z*center.z<20.f) continue;
        for(const auto p : {track.left[i],track.right[i]}) {
            const TrackVec3 top{p.x,p.y-.22f,p.z},foot{p.x,.05f,p.z};
            const TrackVec3 width=trackScale(trackNormalize(trackSubtract(track.right[i],track.left[i])),.10f);
            quad(canvas,camera,trackSubtract(foot,width),trackSubtract(top,width),
                 trackAdd(top,width),trackAdd(foot,width),0xb594u);
            line(canvas,camera,foot,top,0x8490u);
        }
    }
}

inline void drawPencilTrack(LGFX_Sprite& canvas, const TrackCamera& camera,
    const PencilTrack& track, PencilDetail detail, PencilOcclusion* occlusion = nullptr)
{
    using namespace track_paint;
    std::array<uint8_t,PencilTrack::kSegments> order{};
    std::array<float,PencilTrack::kSegments> depths{};
    if(occlusion) { occlusion->count=0; occlusion->overflowed=false; }
    for(std::size_t i=0;i<order.size();++i) {
        order[i]=static_cast<uint8_t>(i);
        depths[i]=trackToCamera(camera,mix(track.left[i],track.right[i+1],.5f)).z;
    }
    std::sort(order.begin(),order.end(),[&](uint8_t a,uint8_t b){return depths[a]>depths[b];});
    for(const auto i : order) {
        const auto a=track.left[i],b=track.right[i],c=track.right[i+1],d=track.left[i+1];
        if(std::max({trackToCamera(camera,a).z,trackToCamera(camera,b).z,
                     trackToCamera(camera,c).z,trackToCamera(camera,d).z})<kTrackNearPlane) continue;
        const TrackVec3 drop{0,-.22f,0},lift{0,.30f,0};
        const auto across=trackSubtract(b,a);
        const auto normal=trackCross(trackSubtract(d,a),across);
        const bool above=trackDot(normal,trackSubtract(camera.position,a))>=0;
        // Only the outward-facing side can be visible. At most four occluder
        // triangles per segment, including underside views below the bridge.
        if(trackDot(across,trackSubtract(camera.position,b))>0)
            quad(canvas,camera,b,c,trackAdd(c,drop),trackAdd(b,drop),fascia,occlusion);
        else if(trackDot(across,trackSubtract(camera.position,a))<0)
            quad(canvas,camera,d,a,trackAdd(a,drop),trackAdd(d,drop),fascia,occlusion);
        if(!above) {
            quad(canvas,camera,trackAdd(a,drop),trackAdd(b,drop),trackAdd(c,drop),
                 trackAdd(d,drop),underside,occlusion);
            line(canvas,camera,trackAdd(a,drop),trackAdd(d,drop),edge);
            line(canvas,camera,trackAdd(b,drop),trackAdd(c,drop),edge);
            if(detail!=PencilDetail::Low && i%4==0)
                line(canvas,camera,trackAdd(a,drop),trackAdd(b,drop),fascia);
            continue;
        }
        quad(canvas,camera,a,b,c,d,(i/8)%2==0 ? road : roadLight,occlusion);
        // Painted shoulders remain inside the original physical edges. There is
        // deliberately no central rail, lane separator, or narrowed road model.
        const uint16_t curb=(i%2)==0 ? coral : chalk;
        quad(canvas,camera,a,mix(a,b,.065f),mix(d,c,.065f),d,curb);
        quad(canvas,camera,mix(a,b,.935f),b,c,mix(d,c,.935f),curb);
        line(canvas,camera,a,d,edge); line(canvas,camera,b,c,edge);
        line(canvas,camera,trackAdd(a,lift),trackAdd(d,lift),teal);
        line(canvas,camera,trackAdd(b,lift),trackAdd(c,lift),teal);
        const unsigned postStep=detail==PencilDetail::High ? 2 : 4;
        if(detail!=PencilDetail::Low && i%postStep==0) {
            line(canvas,camera,a,trackAdd(a,lift),teal);
            line(canvas,camera,b,trackAdd(b,lift),teal);
        }
        // Short edge-side direction chevrons, never a centre line. Anchored in
        // world space so they cannot shimmer or slide as the camera moves.
        if(detail!=PencilDetail::Low && i%6==3) {
            for(float side : {.13f,.87f}) {
                const auto rear=mix(a,b,side),front=mix(d,c,side);
                const auto tip=mix(rear,front,.8f),tail=mix(rear,front,.3f);
                const auto wing=trackScale(across,.022f);
                line(canvas,camera,trackAdd(tail,wing),tip,chalk);
                line(canvas,camera,trackSubtract(tail,wing),tip,chalk);
            }
        }
        if(detail==PencilDetail::High && i%4==2) {
            // Sparse concrete pencil strokes run with the road, not across it.
            line(canvas,camera,mix(a,b,.22f),mix(mix(a,b,.22f),mix(d,c,.22f),.30f),0xb5d6u);
        }
        if(i==0) {
            // A two-row chequered band with real area (the old marker was one
            // pixel-thin line). Its rear edge is exactly the common finish line.
            for(int row=0;row<2;++row) for(int cell=0;cell<12;++cell) {
                const float t0=row*.16f,t1=(row+1)*.16f;
                const auto left0=mix(a,d,t0),right0=mix(b,c,t0);
                const auto left1=mix(a,d,t1),right1=mix(b,c,t1);
                const float u0=.065f+.87f*cell/12,u1=.065f+.87f*(cell+1)/12;
                quad(canvas,camera,mix(left0,right0,u0),mix(left0,right0,u1),
                     mix(left1,right1,u1),mix(left1,right1,u0),(row+cell)%2 ? chalk : edge);
            }
        }
    }
}

} // namespace lets_and_go
