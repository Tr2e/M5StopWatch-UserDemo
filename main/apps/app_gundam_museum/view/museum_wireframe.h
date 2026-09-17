#pragma once
#include "../../app_lets_and_go_racer/view/car_surface_raster.h"
#include <array>
#include <cmath>

namespace gundam_museum {
// Match museum_space paper/ink without including that header (View cycle).
inline constexpr uint16_t hiddenLineInk=0x4208,hiddenLinePaper=0xdefb;

inline void prepareHiddenLineFill(lets_and_go::PreparedCarPanel& prepared) {
    prepared.color=hiddenLinePaper;
    prepared.paint=lets_and_go::CarPaint::Solid;
    prepared.light=255;
}

// Shared 3D edges are drawn once. 0 means an empty hash slot.
struct EdgeFilter {
    static constexpr std::size_t slots=8192;
    std::array<uint32_t,slots> keys{};
    void begin(){keys.fill(0);}
    bool take(uint16_t a,uint16_t b){
        if(a==b)return false;
        if(a>b)std::swap(a,b);
        const uint32_t key=(uint32_t(a)<<16)|uint32_t(b);
        std::size_t slot=key&(slots-1);
        for(std::size_t n=0;n<slots;++n){
            auto& cell=keys[(slot+n)&(slots-1)];
            if(cell==key)return false;
            if(cell==0){cell=key;return true;}
        }
        return true;
    }
};

template<int Width,int Height>
void strokeHiddenLinePanel(lets_and_go::CarSurfaceRaster<Width,Height>& raster,
                           const lets_and_go::TrackCamera& camera,
                           const lets_and_go::PreparedCarPanel& prepared,
                           EdgeFilter* edges=nullptr,const uint16_t* corners=nullptr,
                           bool writeEmpty=false,uint16_t color=hiddenLineInk) {
    if(!prepared.visibility)return;
    const auto skip=[&](float dx,float dy){return dx*dx+dy*dy<.25f;};
    const auto owned=[&](int i){
        if(!edges||!corners)return true;
        return edges->take(corners[i],corners[(i+1)%4]);
    };
    if(prepared.visibility==1) {
        for(int i=0;i<4;++i) {
            if(!owned(i))continue;
            const auto& a=prepared.screen[i];
            const auto& b=prepared.screen[(i+1)%4];
            if(skip(a.x-b.x,a.y-b.y))continue;
            raster.stroke(a,b,color,8,writeEmpty);
        }
        return;
    }
    for(int i=0;i<4;++i) {
        if(!owned(i))continue;
        const auto& a=prepared.camera[i];
        const auto& b=prepared.camera[(i+1)%4];
        const float dx=a.x-b.x,dy=a.y-b.y,dz=a.z-b.z;
        if(dx*dx+dy*dy+dz*dz<1e-8f)continue;
        raster.strokeCamera(camera,a,b,color,writeEmpty);
    }
}
} // namespace gundam_museum
