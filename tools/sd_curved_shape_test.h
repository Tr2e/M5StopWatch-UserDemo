#pragma once
#include "../main/apps/app_gundam_museum/model/rx78.h"
#include <algorithm>
#include <cassert>
#include <iostream>

namespace sd_curved_check {
using namespace gundam_museum;
struct Section { float xmin=100,xmax=-100,zmin=100,zmax=-100;unsigned samples=0; };
// Intersect actual mesh edges with a horizontal plane, rather than just checking
// the whole model AABB (a cylinder and a dome can have identical overall bounds).
inline Section section(const Mesh& m,Part part,float y){
    Section result;
    for(size_t i=0;i<m.count;++i)if(m.parts[i]==part)for(int e=0;e<4;++e){
        const auto a=m.panels[i].point[e],b=m.panels[i].point[(e+1)%4];
        if(std::abs(b.y-a.y)<1e-6f || (y-a.y)*(y-b.y)>0)continue;
        const float t=(y-a.y)/(b.y-a.y),x=a.x+(b.x-a.x)*t,z=a.z+(b.z-a.z)*t;
        result.xmin=std::min(result.xmin,x);result.xmax=std::max(result.xmax,x);
        result.zmin=std::min(result.zmin,z);result.zmax=std::max(result.zmax,z);++result.samples;
    }return result;
}
inline bool roundedCrown(const Mesh& m,float headYOffset=0){
    const auto mid=section(m,Part::Head,2.70f+headYOffset),top=section(m,Part::Head,2.90f+headYOffset);
    return mid.samples&&top.samples&&(top.xmax-top.xmin)<.85f*(mid.xmax-mid.xmin)
        &&(top.zmax-top.zmin)<.9f*(mid.zmax-mid.zmin);
}
inline void crownAndNegative(Mesh& m,float headYOffset=0){
    const auto mid=section(m,Part::Head,2.70f+headYOffset),top=section(m,Part::Head,2.90f+headYOffset);
    std::cout<<"curved_crown mid_width="<<mid.xmax-mid.xmin<<" top_width="<<top.xmax-top.xmin
             <<" mid_depth="<<mid.zmax-mid.zmin<<" top_depth="<<top.zmax-top.zmin<<'\n';
    assert(roundedCrown(m,headYOffset));
    // Collapse the middle without changing the top, antenna or foot envelope.
    for(size_t i=0;i<m.count;++i)if(m.parts[i]==Part::Head)for(auto& p:m.panels[i].point)
        if(p.y>2.54f+headYOffset&&p.y<2.86f+headYOffset)p.x*=.35f;
    assert(!roundedCrown(m,headYOffset));std::cout<<"curved_crown_negative_detected=1\n";
}
} // namespace sd_curved_check
