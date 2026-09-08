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

    static Point project(TrackVec3 p) {
        return {int16_t(std::lround(centerX+p.x*2.6f)),
                int16_t(std::lround(centerY+p.z*2.6f-p.y*.65f))};
    }
    void open(const PencilTrack& track) {
        for(std::size_t i=0;i<section.size();++i)
            section[i]={project(track.left[i]),project(track.right[i])};
        for(std::size_t i=0;i<order.size();++i)order[i]=uint8_t(i);
        std::sort(order.begin(),order.end(),[&](uint8_t a,uint8_t b) {
            const float ay=track.left[a].y+track.right[a+1].y;
            const float by=track.left[b].y+track.right[b+1].y;
            return ay==by ? a<b : ay<by;
        });
    }
    void draw(LGFX_Sprite& canvas) const {
        canvas.fillCircle(centerX,centerY,radius,track_paint::night);
        canvas.drawCircle(centerX,centerY,radius,track_paint::ridge);
        const auto line=[&](Point a,Point b,uint16_t color) {
            canvas.drawLine(a.x,a.y,b.x,b.y,color);
        };
        for(int pass=0;pass<2;++pass) for(auto i:order) {
            const auto a=section[i].left,b=section[i].right;
            const auto c=section[i+1].right,d=section[i+1].left;
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
            line(a,d,paint.wall);line(b,c,paint.wall);
            if(i==0)line(a,b,track_paint::edge);
        }
    }
};

} // namespace lets_and_go
