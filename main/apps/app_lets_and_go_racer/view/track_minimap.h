#pragma once

#include "pencil_scene.h"
#include <algorithm>

namespace lets_and_go {

// Cached, height-sorted miniature of the same 96 physical road segments.
// No unrelated figure-eight icon: banking, bridge and colors match the course.
struct TrackMiniMap {
    struct Point { int16_t x,y; };
    struct Section { Point left,right; };
    std::array<Section,PencilTrack::kSegments+1> section{};
    std::array<uint8_t,PencilTrack::kSegments> order{};
    static constexpr int centerX=354,centerY=122,radius=43;

    float originX=0,originY=0,scale=2.6f;
    Point project(TrackVec3 p) const {
        return {int16_t(std::lround(centerX+(p.x-originX)*scale)),
                int16_t(std::lround(centerY+(p.z-p.y*.25f-originY)*scale))};
    }
    void open(const PencilTrack& track) {
        float minX=1e6f,maxX=-1e6f,minY=1e6f,maxY=-1e6f;
        for(std::size_t i=0;i<section.size();++i) for(auto p:{track.left[i],track.right[i]}) {
            minX=std::min(minX,p.x);maxX=std::max(maxX,p.x);
            minY=std::min(minY,p.z-p.y*.25f);maxY=std::max(maxY,p.z-p.y*.25f);
        }
        originX=(minX+maxX)*.5f;originY=(minY+maxY)*.5f;
        float extent=1.f;
        for(std::size_t i=0;i<section.size();++i) for(auto p:{track.left[i],track.right[i]})
            extent=std::max(extent,std::hypot(p.x-originX,p.z-p.y*.25f-originY));
        scale=(radius-6.f)/extent; // Include shadows and four-pixel markers.
        for(std::size_t i=0;i<section.size();++i)
            section[i]={project(track.left[i]),project(track.right[i])};
        for(std::size_t i=0;i<order.size();++i)order[i]=uint8_t(i);
        std::sort(order.begin(),order.end(),[&](uint8_t a,uint8_t b) {
            const float ay=track.left[a].y+track.right[a+1].y;
            const float by=track.left[b].y+track.right[b+1].y;
            return ay==by ? a<b : ay<by;
        });
    }
    void draw(LGFX_Sprite& canvas,int originX=0,int originY=0) const {
        canvas.fillCircle(centerX-originX,centerY-originY,radius,track_paint::night);
        canvas.drawCircle(centerX-originX,centerY-originY,radius,track_paint::ridge);
        const auto line=[&](Point a,Point b,uint16_t color) {
            canvas.drawLine(a.x-originX,a.y-originY,b.x-originX,b.y-originY,color);
        };
        for(int pass=0;pass<2;++pass) for(auto i:order) {
            const auto local=[&](Point p){return Point{int16_t(p.x-originX),int16_t(p.y-originY)};};
            const auto a=local(section[i].left),b=local(section[i].right);
            const auto c=local(section[i+1].right),d=local(section[i+1].left);
            const auto paint=track_paint::module(i);
            // All contact shadows precede the ribbon. A per-segment shadow
            // would cut stripes into the neighbouring module at this size.
            if(pass==0) {
                canvas.fillTriangle(a.x,a.y+2,b.x,b.y+2,c.x,c.y+2,0x0802u);
                canvas.fillTriangle(a.x,a.y+2,c.x,c.y+2,d.x,d.y+2,0x0802u);
                continue;
            }
            canvas.fillTriangle(a.x,a.y,b.x,b.y,c.x,c.y,paint.deck);
            canvas.fillTriangle(a.x,a.y,c.x,c.y,d.x,d.y,paint.deck);
            line(section[i].left,section[i+1].left,paint.wall);
            line(section[i].right,section[i+1].right,paint.wall);
            if(i==0)line(section[i].left,section[i].right,track_paint::edge);
        }
    }
};

} // namespace lets_and_go
