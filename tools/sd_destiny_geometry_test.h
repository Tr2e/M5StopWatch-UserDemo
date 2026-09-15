#pragma once
#include "../main/apps/app_gundam_museum/model/destiny_gundam.h"
#include "sd_rx78_equipment_test.h"
#include <initializer_list>
#include <iostream>
#include <memory>

namespace sd_destiny_check {
using namespace gundam_museum;
using Range=DestinyAssembly::Range;

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
struct Allow { Range moving,other; };
inline unsigned strict(const Mesh& m,Range moving,std::initializer_list<Allow> allowed={}){
    unsigned pairs=0;for(size_t i=moving.begin;i<moving.end;++i)for(size_t j=0;j<m.count;++j){
        if(j>=moving.begin&&j<moving.end)continue;bool skip=false;
        for(auto a:allowed)skip|=i>=a.moving.begin&&i<a.moving.end&&j>=a.other.begin&&j<a.other.end;
        pairs+=!skip&&(crosses(m,i,j)>0);
    }return pairs;
}
inline void report(const Mesh& m,Range moving,const char* name,std::initializer_list<Allow> allowed={}){
    std::array<unsigned,static_cast<unsigned>(Part::Count)> counts{};
    for(size_t i=moving.begin;i<moving.end;++i)for(size_t j=0;j<m.count;++j){
        if(j>=moving.begin&&j<moving.end)continue;bool skip=false;
        for(auto a:allowed)skip|=i>=a.moving.begin&&i<a.moving.end&&j>=a.other.begin&&j<a.other.end;
        if(!skip&&crosses(m,i,j))++counts[static_cast<unsigned>(m.parts[j])];
    }
    for(unsigned i=0;i<counts.size();++i)if(counts[i])std::cerr<<name<<" cross_part="<<i<<" pairs="<<counts[i]<<'\n';
}
inline std::pair<Point,Point> bounds(const Mesh& m,Range r){
    Point lo{100,100,100},hi{-100,-100,-100};for(size_t i=r.begin;i<r.end;++i)for(auto p:m.panels[i].point){
        lo.x=std::min(lo.x,p.x);lo.y=std::min(lo.y,p.y);lo.z=std::min(lo.z,p.z);
        hi.x=std::max(hi.x,p.x);hi.y=std::max(hi.y,p.y);hi.z=std::max(hi.z,p.z);
    }return {lo,hi};
}
inline void check(const Mesh& production){
    auto m=std::make_unique<Mesh>();DestinyAssembly a;buildDestinyGundam(*m,{},DestinyStage::Final,&a);
    assert(!m->overflowed&&m->count==production.count);
    const Range required[]={a.headMount,a.helmet,a.backpackMount,a.rifle,a.shieldMount,a.shield,
        a.swordMount,a.sword,a.cannonMount,a.cannon,a.palms[0],a.palms[1],
        a.wingMounts[0],a.wingMounts[1],a.wings[0],a.wings[1]};
    for(auto r:required)assert(r.end>r.begin);

    const auto neck=bounds(*m,a.headMount),helmet=bounds(*m,a.helmet),backpack=bounds(*m,a.backpackMount);
    assert(neck.first.y<2.0f&&neck.second.y>2.19f&&helmet.first.y<2.08f&&helmet.second.y>3.44f);
    assert(backpack.first.z<-.48f&&backpack.second.z>-.21f);
    const unsigned supports[]={between(*m,a.headMount,a.helmet),between(*m,a.rifle,a.palms[0]),
        between(*m,a.shieldMount,a.shield),between(*m,a.wingMounts[0],a.wings[0]),
        between(*m,a.wingMounts[1],a.wings[1]),between(*m,a.swordMount,a.sword),between(*m,a.cannonMount,a.cannon)};
    std::cout<<"sd_destiny support head="<<supports[0]<<" rifle="<<supports[1]<<" shield="<<supports[2]
        <<" wings="<<supports[3]<<','<<supports[4]<<" sword="<<supports[5]<<" cannon="<<supports[6]<<'\n';
    for(auto count:supports)assert(count>0);

    const unsigned rifle=strict(*m,a.rifle,{{a.rifle,a.palms[0]}});
    const unsigned shield=strict(*m,a.shield,{{a.shield,a.shieldMount}});
    const unsigned wing0=strict(*m,a.wings[0],{{a.wings[0],a.wingMounts[0]}});
    const unsigned wing1=strict(*m,a.wings[1],{{a.wings[1],a.wingMounts[1]}});
    const unsigned sword=strict(*m,a.sword,{{a.sword,a.swordMount}});
    const unsigned cannon=strict(*m,a.cannon,{{a.cannon,a.cannonMount}});
    unsigned palms=0;for(auto hand:a.palms)for(size_t i=hand.begin;i<hand.end;++i)
        for(size_t j=0;j<m->count;++j)if(m->parts[j]==Part::Arms)palms+=crosses(*m,i,j)>0;
    std::cout<<"sd_destiny unintended rifle="<<rifle<<" shield="<<shield<<" wings="<<wing0<<','<<wing1
        <<" sword="<<sword<<" cannon="<<cannon<<" palm_armor="<<palms<<'\n';
    if(wing0)report(*m,a.wings[0],"wing0",{{a.wings[0],a.wingMounts[0]}});
    if(wing1)report(*m,a.wings[1],"wing1",{{a.wings[1],a.wingMounts[1]}});
    if(sword)report(*m,a.sword,"sword",{{a.sword,a.swordMount}});
    if(cannon)report(*m,a.cannon,"cannon",{{a.cannon,a.cannonMount}});
    assert(rifle==0&&shield==0&&wing0==0&&wing1==0&&sword==0&&cannon==0&&palms==0);

    float floorArea[2]={},crown=0,chin=10,width=0,zmin=10,zmax=-10,wingMin=100,wingMax=-100;
    for(size_t i=0;i<m->count;++i){
        if(m->parts[i]==Part::Feet){auto p=m->panels[i].point;bool floor=true;for(auto q:p){assert(q.y>=.0249f);floor&=std::abs(q.y-.025f)<.0001f;}
            if(floor)for(int t=0;t<2;++t){auto n=cross(subtract(p[t+1],p[0]),subtract(p[t+2],p[0]));floorArea[p[0].x>0]+=.5f*std::sqrt(dot(n,n));}}
        if(m->parts[i]==Part::Aile)for(auto p:m->panels[i].point){wingMin=std::min(wingMin,p.x);wingMax=std::max(wingMax,p.x);}
        if(m->parts[i]==Part::Head&&i>=a.helmet.begin&&i<a.helmet.end)for(auto p:m->panels[i].point)if(p.y<3.10f){
            crown=std::max(crown,p.y);chin=std::min(chin,p.y);width=std::max(width,std::abs(p.x)*2);zmin=std::min(zmin,p.z);zmax=std::max(zmax,p.z);}
    }
    const float ratio=(crown-.025f)/(crown-chin),depth=(zmax-zmin)/width;
    std::cout<<"sd_destiny panels="<<m->count<<" helmet_body_ratio="<<ratio<<" depth_width="<<depth
        <<" wing_span="<<wingMax-wingMin<<" sole_area="<<floorArea[0]<<','<<floorArea[1]<<'\n';
    assert(ratio>2.7f&&ratio<3.3f&&depth>.75f&&depth<1.2f&&wingMax-wingMin>4.4f);
    assert(floorArea[0]>.30f&&floorArea[1]>.30f);

    // Controlled negatives prove that the support gates detect both a detached
    // display part and a misplaced weapon instead of accepting visual proximity.
    auto detached=*m;for(size_t i=a.helmet.begin;i<a.helmet.end;++i)for(auto& p:detached.panels[i].point)p.y+=.42f;
    const unsigned badHead=between(detached,a.headMount,a.helmet);assert(badHead==0);
    buildDestinyGundam(*m,{},DestinyStage::Final,&a);
    for(size_t i=a.wings[0].begin;i<a.wings[0].end;++i)for(auto& p:m->panels[i].point)p.x-=.55f;
    const unsigned badWing=between(*m,a.wingMounts[0],a.wings[0]);assert(badWing==0);
    buildDestinyGundam(*m,{},DestinyStage::Final,&a);
    for(size_t i=a.rifle.begin;i<a.rifle.end;++i)for(auto& p:m->panels[i].point){p.x+=.72f;p.y+=.48f;p.z-=.42f;}
    const unsigned badRifle=strict(*m,a.rifle,{{a.rifle,a.palms[0]}});assert(badRifle>0);
    std::cout<<"sd_destiny negatives detached_head="<<badHead<<" detached_wing="<<badWing<<" misplaced_rifle="<<badRifle<<'\n';
}
} // namespace sd_destiny_check
