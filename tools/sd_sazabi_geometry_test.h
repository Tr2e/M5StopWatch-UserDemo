#pragma once
#include "../main/apps/app_gundam_museum/model/sazabi.h"
#include "sd_rx78_equipment_test.h"
#include <memory>

namespace sd_sazabi_check {
using namespace gundam_museum;
inline unsigned crosses(const Mesh& m,size_t i,size_t j){
    unsigned hits=0;const auto a=m.panels[i].point,b=m.panels[j].point;
    for(int ta=0;ta<2;++ta)for(int tb=0;tb<2;++tb){Point x[]={a[0],a[ta+1],a[ta+2]},y[]={b[0],b[tb+1],b[tb+2]};
        for(int e=0;e<3;++e)for(int d=0;d<2;++d){Point p;hits+=d?sd_equipment_check::crossing(y[e],y[(e+1)%3],x[0],x[1],x[2],p):sd_equipment_check::crossing(x[e],x[(e+1)%3],y[0],y[1],y[2],p);}}
    return hits;
}
inline unsigned rangeAgainstParts(const Mesh& m,SazabiAssembly::Range r,std::initializer_list<Part> ignored){
    unsigned pairs=0;for(size_t i=r.begin;i<r.end;++i)for(size_t j=0;j<m.count;++j){if(j>=r.begin&&j<r.end)continue;bool skip=false;for(auto p:ignored)skip|=m.parts[j]==p;if(skip)continue;if(crosses(m,i,j))++pairs;}return pairs;
}
inline unsigned between(const Mesh& m,SazabiAssembly::Range a,SazabiAssembly::Range b){unsigned pairs=0;for(size_t i=a.begin;i<a.end;++i)for(size_t j=b.begin;j<b.end;++j)if(crosses(m,i,j))++pairs;return pairs;}
inline void reportRange(const Mesh& m,SazabiAssembly::Range r,std::initializer_list<Part> ignored,const char* name){
    std::array<unsigned,static_cast<unsigned>(Part::Count)> counts{};for(size_t i=r.begin;i<r.end;++i)for(size_t j=0;j<m.count;++j){if(j>=r.begin&&j<r.end)continue;bool skip=false;for(auto p:ignored)skip|=m.parts[j]==p;if(!skip&&crosses(m,i,j))++counts[static_cast<unsigned>(m.parts[j])];}
    for(unsigned i=0;i<counts.size();++i)if(counts[i])std::cout<<name<<"_cross_part="<<i<<" pairs="<<counts[i]<<'\n';
}
inline void check(const Mesh& production){
    auto m=std::make_unique<Mesh>();SazabiAssembly a;buildSazabi(*m,{},SazabiStage::Final,&a);assert(!m->overflowed&&m->count==production.count);
    assert(a.rifle.end>a.rifle.begin&&a.shield.end>a.shield.begin);for(auto r:a.funnels)assert(r.end>r.begin+20);for(auto r:a.shoulders)assert(r.end>r.begin+30);for(auto r:a.palms)assert(r.end>r.begin);
    unsigned funnelPairs=0,funnelBody=0;for(size_t i=0;i<a.funnels.size();++i){
        funnelBody+=rangeAgainstParts(*m,a.funnels[i],{Part::Backpack,Part::Funnels});
        for(size_t j=i+1;j<a.funnels.size();++j)funnelPairs+=between(*m,a.funnels[i],a.funnels[j]);
    }
    const unsigned rifle=rangeAgainstParts(*m,a.rifle,{Part::Hands,Part::Rifle});
    const unsigned shield=rangeAgainstParts(*m,a.shield,{Part::Arms,Part::Hands,Part::Shield,Part::Sabers});
    unsigned palms=0;for(auto r:a.palms)for(size_t i=r.begin;i<r.end;++i)for(size_t j=0;j<m->count;++j)if(m->parts[j]==Part::Arms)palms+=crosses(*m,i,j)>0;
    std::cout<<"sd_sazabi contacts rifle="<<rifle<<" shield="<<shield<<" funnel_pairs="<<funnelPairs<<" funnel_body="<<funnelBody<<" palm_armor="<<palms<<'\n';
    if(rifle)reportRange(*m,a.rifle,{Part::Hands,Part::Rifle},"rifle");
    if(shield)reportRange(*m,a.shield,{Part::Arms,Part::Hands,Part::Shield,Part::Sabers},"shield");
    assert(rifle==0&&shield==0&&funnelPairs==0&&funnelBody==0&&palms==0);
    float floorArea[2]={},crown=0,chin=10,width=0,zmin=10,zmax=-10,shoulderMin=100,shoulderMax=-100;
    for(size_t i=0;i<m->count;++i){
        if(m->parts[i]==Part::Feet){auto p=m->panels[i].point;bool floor=true;for(auto q:p){assert(q.y>=.0249f);floor&=std::abs(q.y-.025f)<.0001f;}if(floor)for(int t=0;t<2;++t){auto n=cross(subtract(p[t+1],p[0]),subtract(p[t+2],p[0]));floorArea[p[0].x>0]+=.5f*std::sqrt(dot(n,n));}}
        if(m->parts[i]==Part::Shoulders)for(auto p:m->panels[i].point){shoulderMin=std::min(shoulderMin,p.x);shoulderMax=std::max(shoulderMax,p.x);}
        if(m->parts[i]==Part::Head)for(auto p:m->panels[i].point)if(p.y<3.05f){crown=std::max(crown,p.y);chin=std::min(chin,p.y);width=std::max(width,std::abs(p.x)*2);zmin=std::min(zmin,p.z);zmax=std::max(zmax,p.z);}
    }
    const float ratio=(crown-.025f)/(crown-chin),depth=(zmax-zmin)/width;
    std::cout<<"sd_sazabi panels="<<m->count<<" helmet_body_ratio="<<ratio<<" depth_width="<<depth<<" shoulder_span="<<shoulderMax-shoulderMin<<" sole_area="<<floorArea[0]<<','<<floorArea[1]<<'\n';
    assert(ratio>2.8f&&ratio<3.35f&&depth>.75f&&depth<1.2f&&shoulderMax-shoulderMin>2.2f&&floorArea[0]>.38f&&floorArea[1]>.38f);
    // Controlled equipment error: move the complete rifle into the chest.
    for(size_t i=a.rifle.begin;i<a.rifle.end;++i)for(auto& p:m->panels[i].point){p.x+=.72f;p.y+=.45f;p.z-=.40f;}
    const unsigned badRifle=rangeAgainstParts(*m,a.rifle,{Part::Hands,Part::Rifle});assert(badRifle>0);
    buildSazabi(*m,{},SazabiStage::Final,&a);
    const auto shifted=a.funnels[0];for(size_t i=shifted.begin;i<shifted.end;++i)for(auto& p:m->panels[i].point){p.x-=.10f;p.y-=.12f;p.z+=.025f;}
    const unsigned badFunnels=between(*m,a.funnels[0],a.funnels[1]);
    std::cout<<"sd_sazabi negative rifle="<<badRifle<<" funnel_overlap="<<badFunnels<<'\n';assert(badFunnels>0);
}
} // namespace sd_sazabi_check
