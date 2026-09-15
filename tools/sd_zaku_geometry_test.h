#pragma once
#include "../main/apps/app_gundam_museum/model/char_zaku.h"
#include "sd_rx78_equipment_test.h"
#include <limits>
#include <memory>

namespace sd_zaku_check {
using namespace gundam_museum;
inline unsigned rifleCrossings(const Mesh& m,bool report=false){
    unsigned bad=0;bool first=true;std::array<unsigned,static_cast<unsigned>(Part::Count)> byPart{};
    for(size_t i=0;i<m.count;++i)if(m.parts[i]==Part::Rifle)for(size_t j=0;j<m.count;++j){
        if(m.parts[j]==Part::Rifle || m.parts[j]>=Part::Shield)continue;
        const auto a=m.panels[i].point,b=m.panels[j].point;bool touched=false,allowed=true;
        for(int ta=0;ta<2;++ta)for(int tb=0;tb<2;++tb){Point x[]={a[0],a[ta+1],a[ta+2]},y[]={b[0],b[tb+1],b[tb+2]};
            for(int e=0;e<3;++e)for(int d=0;d<2;++d){Point p;if(d?sd_equipment_check::crossing(y[e],y[(e+1)%3],x[0],x[1],x[2],p):sd_equipment_check::crossing(x[e],x[(e+1)%3],y[0],y[1],y[2],p)){
                touched=true;const bool okay=m.parts[j]==Part::Hands && p.x<-.55f && p.y>.92f && p.y<1.25f;
                if(report&&!okay&&first){std::cout<<" first_rifle_cross xyz="<<p.x<<','<<p.y<<','<<p.z<<" part="<<unsigned(m.parts[j])<<'\n';first=false;}allowed &= okay;
            }}
        }if(touched&&!allowed){++bad;++byPart[static_cast<unsigned>(m.parts[j])];}
    }if(report)for(unsigned i=0;i<byPart.size();++i)if(byPart[i])std::cout<<" rifle_cross_part="<<i<<" pairs="<<byPart[i]<<'\n';return bad;
}
inline void check(const Mesh& production){
    auto m=std::make_unique<Mesh>();ZakuAssembly a;buildCharZaku(*m,{},ZakuStage::Final,&a);
    assert(!m->overflowed && m->count==production.count);
    for(auto r:{a.rightShield,a.leftSpikes,a.headHose,a.waistHose,a.palm})assert(r.end>r.begin);
    auto bounds=[&](ZakuAssembly::Range r){Point lo{100,100,100},hi{-100,-100,-100};for(size_t i=r.begin;i<r.end;++i)for(auto p:m->panels[i].point){lo.x=std::min(lo.x,p.x);lo.y=std::min(lo.y,p.y);lo.z=std::min(lo.z,p.z);hi.x=std::max(hi.x,p.x);hi.y=std::max(hi.y,p.y);hi.z=std::max(hi.z,p.z);}return std::pair<Point,Point>{lo,hi};};
    const auto shield=bounds(a.rightShield),spikes=bounds(a.leftSpikes),headHose=bounds(a.headHose),waistHose=bounds(a.waistHose);
    assert(shield.second.x<0 && shield.second.y-shield.first.y>.85f);
    assert(spikes.first.x>0 && spikes.second.x>1.25f);
    assert(headHose.first.y<2.15f && headHose.second.y>2.34f);
    assert(waistHose.first.y<1.25f && waistHose.second.y>1.47f);
    float floorArea[2]={},crown=0,chin=10,width=0,zmin=10,zmax=-10,rifleFloor=10;
    for(size_t i=0;i<m->count;++i){
        if(m->parts[i]==Part::Feet){auto p=m->panels[i].point;bool floor=true;for(auto q:p){assert(q.y>=.0249f);floor&=std::abs(q.y-.025f)<.0001f;}if(floor)for(int t=0;t<2;++t){auto n=cross(subtract(p[t+1],p[0]),subtract(p[t+2],p[0]));floorArea[p[0].x>0]+=.5f*std::sqrt(dot(n,n));}}
        if(m->parts[i]==Part::Rifle)for(auto p:m->panels[i].point)rifleFloor=std::min(rifleFloor,p.y);
        if(m->parts[i]==Part::Head)for(auto p:m->panels[i].point)if(p.y<3.08f){crown=std::max(crown,p.y);chin=std::min(chin,p.y);width=std::max(width,std::abs(p.x)*2);zmin=std::min(zmin,p.z);zmax=std::max(zmax,p.z);}
    }
    const float ratio=(crown-.025f)/(crown-chin),depth=(zmax-zmin)/width;
    std::cout<<"sd_zaku panels="<<m->count<<" helmet_body_ratio="<<ratio<<" depth_width="<<depth<<" sole_area="<<floorArea[0]<<','<<floorArea[1]<<" rifle_floor="<<rifleFloor<<'\n';
    // The helmet dome has no Gundam chin extension; its visible head interval is
    // shorter and therefore uses an independent SD range.
    assert(ratio>2.9f&&ratio<3.35f&&depth>.72f&&depth<1.2f&&floorArea[0]>.3f&&floorArea[1]>.3f&&rifleFloor>.025f);
    const unsigned correct=rifleCrossings(*m,true);std::cout<<"sd_zaku rifle_unintended="<<correct<<'\n';assert(correct==0);
    for(size_t i=0;i<m->count;++i)if(m->parts[i]==Part::Rifle)for(auto& p:m->panels[i].point){p.x+=.72f;p.y+=.45f;p.z-=.35f;}
    const unsigned negative=rifleCrossings(*m);std::cout<<"sd_zaku negative_rifle_unintended="<<negative<<'\n';assert(negative>0);
}
} // namespace sd_zaku_check
