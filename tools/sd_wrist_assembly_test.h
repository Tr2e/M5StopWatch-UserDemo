#pragma once
#include "../main/apps/app_gundam_museum/model/rx78.h"
#include "sd_rx78_equipment_test.h"
#include <cassert>
#include <cmath>
#include <iostream>

namespace sd_wrist_check {
using namespace gundam_museum;
struct Range { size_t begin=0,end=0; };

inline float span(const Mesh& m,Range r){
    Point lo{100,100,100},hi{-100,-100,-100};
    for(size_t i=r.begin;i<r.end;++i)for(auto p:m.panels[i].point){
        lo={std::min(lo.x,p.x),std::min(lo.y,p.y),std::min(lo.z,p.z)};
        hi={std::max(hi.x,p.x),std::max(hi.y,p.y),std::max(hi.z,p.z)};
    }
    return std::max(hi.x-lo.x,std::max(hi.y-lo.y,hi.z-lo.z));
}
inline float gap(const Mesh& m,Range palm){
    float best=100;
    for(size_t i=palm.begin;i<palm.end;++i)for(auto p:m.panels[i].point)
        for(size_t j=0;j<m.count;++j)if(m.parts[j]==Part::Arms)for(auto q:m.panels[j].point){
            const auto d=subtract(p,q);best=std::min(best,std::sqrt(dot(d,d)));
        }
    return best;
}
inline unsigned crosses(const Mesh& m,size_t i,size_t j){
    unsigned hits=0;const auto a=m.panels[i].point,b=m.panels[j].point;
    for(int ta=0;ta<2;++ta)for(int tb=0;tb<2;++tb){Point x[]={a[0],a[ta+1],a[ta+2]},y[]={b[0],b[tb+1],b[tb+2]};
        for(int e=0;e<3;++e)for(int d=0;d<2;++d){Point p;hits+=d?
            sd_equipment_check::crossing(y[e],y[(e+1)%3],x[0],x[1],x[2],p):
            sd_equipment_check::crossing(x[e],x[(e+1)%3],y[0],y[1],y[2],p);}}
    return hits;
}
inline unsigned between(const Mesh& m,Range a,Range b){
    unsigned pairs=0;for(size_t i=a.begin;i<a.end;++i)for(size_t j=b.begin;j<b.end;++j)pairs+=crosses(m,i,j)>0;return pairs;
}
inline unsigned againstPart(const Mesh& m,Range r,Part part){
    unsigned pairs=0;for(size_t i=r.begin;i<r.end;++i)for(size_t j=0;j<m.count;++j){
        if(j>=r.begin&&j<r.end)continue;
        if(m.parts[j]==part)pairs+=crosses(m,i,j)>0;
    }return pairs;
}
inline void check(const Mesh& m,Range wrist,Range palm,const char* name,int side){
    assert(wrist.end>wrist.begin&&palm.end>palm.begin);
    const float pin=span(m,wrist),clearance=gap(m,palm);
    const unsigned toArm=againstPart(m,wrist,Part::Arms),toPalm=between(m,wrist,palm);
    std::cout<<"sd_wrist "<<name<<" side="<<side<<" pin="<<pin<<" palm_gap="<<clearance
             <<" forearm="<<toArm<<" palm="<<toPalm<<'\n';
    // Front-view hanging cubes were 0.20–0.23. Seated left pins sit near 0.15;
    // the rifle-hand pose needs a little more because the fist is rotated.
    assert(toArm>0&&toPalm>0);
    assert(pin>.07f&&pin<.18f);
    assert(clearance<.10f);
}
inline void checkDetached(const Mesh& m,Range wrist,Range palm){
    auto bad=m;
    for(size_t i=wrist.begin;i<wrist.end;++i)for(auto& p:bad.panels[i].point)p.x+=.35f;
    assert(againstPart(bad,wrist,Part::Arms)==0&&between(bad,wrist,palm)==0);
}
} // namespace sd_wrist_check
