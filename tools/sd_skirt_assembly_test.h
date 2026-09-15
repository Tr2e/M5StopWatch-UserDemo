#pragma once
#include "../main/apps/app_gundam_museum/model/sd_skirt_assembly.h"
#include "../main/apps/app_gundam_museum/model/rx78.h"
#include <algorithm>
#include <cassert>
#include <iostream>

namespace sd_skirt_check {
using namespace gundam_museum;

struct Bounds { Point lo{100,100,100},hi{-100,-100,-100}; };
inline Bounds bounds(const Mesh& mesh,SkirtAssembly::Range range){
    Bounds result;
    assert(range.end>range.begin && range.end<=mesh.count);
    for(size_t i=range.begin;i<range.end;++i){
        assert(mesh.parts[i]==Part::Waist);
        for(auto p:mesh.panels[i].point){
            result.lo.x=std::min(result.lo.x,p.x);result.lo.y=std::min(result.lo.y,p.y);result.lo.z=std::min(result.lo.z,p.z);
            result.hi.x=std::max(result.hi.x,p.x);result.hi.y=std::max(result.hi.y,p.y);result.hi.z=std::max(result.hi.z,p.z);
        }
    }
    return result;
}
inline bool layoutValid(const Mesh& mesh,const SkirtAssembly& skirts,bool requireSide){
    for(int side=0;side<2;++side){
        if(skirts.front[side].end<=skirts.front[side].begin || skirts.rear[side].end<=skirts.rear[side].begin)return false;
        const auto front=bounds(mesh,skirts.front[side]),rear=bounds(mesh,skirts.rear[side]);
        const bool negative=side==0;
        if(negative?(front.hi.x>=-.025f||rear.hi.x>=-.025f):(front.lo.x<=.025f||rear.lo.x<=.025f))return false;
        if(front.lo.z<=.18f || rear.hi.z>=-.18f)return false;
        if(front.lo.y<.88f || rear.lo.y<.88f || front.hi.y>1.50f || rear.hi.y>1.50f)return false;
        if(requireSide){
            if(skirts.side[side].end<=skirts.side[side].begin)return false;
            const auto outer=bounds(mesh,skirts.side[side]);
            if(negative?outer.hi.x>=-.12f:outer.lo.x<=.12f)return false;
        }
    }
    return true;
}
inline void check(Mesh& mesh,const SkirtAssembly& skirts,bool requireSide,const char* model){
    assert(layoutValid(mesh,skirts,requireSide));
    const auto frontRight=skirts.front[0];
    for(size_t i=frontRight.begin;i<frontRight.end;++i)for(auto& p:mesh.panels[i].point)p.x+=.80f;
    assert(!layoutValid(mesh,skirts,requireSide));
    for(size_t i=frontRight.begin;i<frontRight.end;++i)for(auto& p:mesh.panels[i].point)p.x-=.80f;
    const auto rearRight=skirts.rear[0];
    for(size_t i=rearRight.begin;i<rearRight.end;++i)for(auto& p:mesh.panels[i].point)p.z+=.80f;
    assert(!layoutValid(mesh,skirts,requireSide));
    for(size_t i=rearRight.begin;i<rearRight.end;++i)for(auto& p:mesh.panels[i].point)p.z-=.80f;
    assert(layoutValid(mesh,skirts,requireSide));
    std::cout<<"sd_skirts model="<<model<<" front="
        <<skirts.front[0].end-skirts.front[0].begin<<','<<skirts.front[1].end-skirts.front[1].begin
        <<" side="<<skirts.side[0].end-skirts.side[0].begin<<','<<skirts.side[1].end-skirts.side[1].begin
        <<" rear="<<skirts.rear[0].end-skirts.rear[0].begin<<','<<skirts.rear[1].end-skirts.rear[1].begin
        <<" negatives=2\n";
}
} // namespace sd_skirt_check
