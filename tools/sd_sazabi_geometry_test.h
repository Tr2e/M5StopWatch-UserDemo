#pragma once
#include "../main/apps/app_gundam_museum/model/sazabi.h"
#include "sd_rx78_equipment_test.h"
#include "sd_curved_shape_test.h"
#include "sd_eye_socket_test.h"
#include "sd_skirt_assembly_test.h"
#include "sd_wrist_assembly_test.h"
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
    unsigned pairs=0;for(size_t i=r.begin;i<r.end;++i)for(size_t j=0;j<m.count;++j){if(j>=r.begin&&j<r.end)continue;bool skip=false;for(auto p:ignored)skip|=m.parts[j]==p;if(skip)continue;if(crosses(m,i,j)){if(pairs==0)std::cout<<"contact first range="<<r.begin<<":"<<r.end<<" i="<<i<<" j="<<j<<" part="<<unsigned(m.parts[j])<<" xyz="<<m.panels[i].point[0].x<<","<<m.panels[i].point[0].y<<","<<m.panels[i].point[0].z<<'\n';++pairs;}}return pairs;
}
inline unsigned between(const Mesh& m,SazabiAssembly::Range a,SazabiAssembly::Range b){unsigned pairs=0;for(size_t i=a.begin;i<a.end;++i)for(size_t j=b.begin;j<b.end;++j)if(crosses(m,i,j))++pairs;return pairs;}
struct Contact { SazabiAssembly::Range moving,body; };
inline unsigned strictContacts(const Mesh& m,SazabiAssembly::Range r,std::initializer_list<Contact> allowed={}){
    unsigned pairs=0;for(size_t i=r.begin;i<r.end;++i)for(size_t j=0;j<m.count;++j){
        if(j>=r.begin&&j<r.end)continue;bool skip=false;
        for(auto c:allowed)skip|=i>=c.moving.begin&&i<c.moving.end&&j>=c.body.begin&&j<c.body.end;
        if(!skip&&crosses(m,i,j)){if(!pairs)std::cout<<"strict_contact moving="<<i<<" body="<<j<<" part="<<unsigned(m.parts[j])<<'\n';++pairs;}
    }return pairs;
}
inline void reportRange(const Mesh& m,SazabiAssembly::Range r,std::initializer_list<Part> ignored,const char* name){
    std::array<unsigned,static_cast<unsigned>(Part::Count)> counts{};for(size_t i=r.begin;i<r.end;++i)for(size_t j=0;j<m.count;++j){if(j>=r.begin&&j<r.end)continue;bool skip=false;for(auto p:ignored)skip|=m.parts[j]==p;if(!skip&&crosses(m,i,j))++counts[static_cast<unsigned>(m.parts[j])];}
    for(unsigned i=0;i<counts.size();++i)if(counts[i])std::cout<<name<<"_cross_part="<<i<<" pairs="<<counts[i]<<'\n';
}
inline void check(const Mesh& production){
    auto m=std::make_unique<Mesh>();SazabiAssembly a;buildSazabi(*m,{},SazabiStage::Final,&a);assert(!m->overflowed&&m->count==production.count);
    sd_skirt_check::check(*m,a.skirts,true,"sazabi");
    assert(a.rifle.end>a.rifle.begin&&a.shield.end>a.shield.begin&&a.headMount.end>a.headMount.begin&&a.mask.end>a.mask.begin);for(auto r:a.funnels)assert(r.end>r.begin+20);for(auto r:a.shoulders)assert(r.end>r.begin+30);for(auto r:a.palms)assert(r.end>r.begin);for(auto r:a.wrists)assert(r.end>r.begin);for(auto r:a.forearms)assert(r.end>r.begin);
    for(int side=0;side<2;++side){
        sd_wrist_check::Range wrist{a.wrists[side].begin,a.wrists[side].end};
        sd_wrist_check::Range palm{a.palms[side].begin,a.palms[side].end};
        sd_wrist_check::check(*m,wrist,palm,"sazabi",side);
        sd_wrist_check::checkDetached(*m,wrist,palm);
    }
    auto bounds=[&](SazabiAssembly::Range r){Point lo{100,100,100},hi{-100,-100,-100};for(size_t i=r.begin;i<r.end;++i)for(auto p:m->panels[i].point){lo.x=std::min(lo.x,p.x);lo.y=std::min(lo.y,p.y);lo.z=std::min(lo.z,p.z);hi.x=std::max(hi.x,p.x);hi.y=std::max(hi.y,p.y);hi.z=std::max(hi.z,p.z);}return std::pair<Point,Point>{lo,hi};};
    const auto headMount=bounds(a.headMount),mask=bounds(a.mask);
    const auto lens=bounds(a.eyeLens);
    assert(lens.second.z<.50f&&lens.first.z<.48f&&lens.second.y-lens.first.y>.065f);
    assert(std::abs(sd_eye_check::frontSurface(*m,0,2.452f)-.490f)<.0001f);
    for(int i=0;i<10;++i){const float angle=i*6.283185307f/10;
        assert(std::abs(sd_eye_check::frontSurface(*m,.030f*std::cos(angle),2.452f+.030f*std::sin(angle))-.490f)<.0001f);
    }
    for(float s:{-1.f,1.f}){
        // The middle of each sloping window must expose the recessed back,
        // not a forward black strip or a red cap. End height is only .020.
        const float upper=2.49f+.13f*.5f,lower=2.405f+.195f*.5f;
        assert(upper-lower>.050f&&upper-lower<.060f);
        assert(std::abs(sd_eye_check::frontSurface(*m,s*.215f,(upper+lower)*.5f)-.4275f)<.0001f);
    }
    auto badEye=std::make_unique<Mesh>(*m);
    for(size_t i=a.eyeLens.begin;i<a.eyeLens.end;++i)for(auto& p:badEye->panels[i].point)p.z+=.10f;
    assert(sd_eye_check::frontSurface(*badEye,0,2.452f)>.55f);
    *badEye=*m;
    for(auto r:a.eyeWindow)for(size_t i=r.begin;i<r.end;++i)for(auto& p:badEye->panels[i].point)p.z+=.075f;
    assert(sd_eye_check::frontSurface(*badEye,.215f,2.52875f)>.49f);
    std::cout<<"sazabi_eye_window recess=.070 lens_recess=.060 negatives_detected=2\n";
    assert(headMount.first.y<2.14f&&headMount.second.y>2.30f&&headMount.first.z<-.17f&&headMount.second.z>.12f);
    assert(mask.first.x<-.29f&&mask.second.x>.29f&&mask.first.y<2.15f&&mask.second.y>2.38f&&mask.first.z<.06f&&mask.first.z<headMount.second.z&&mask.second.z>.57f);
    auto detached=*m;for(size_t i=a.headMount.begin;i<a.headMount.end;++i)for(auto& p:detached.panels[i].point)p.y+=.40f;
    float detachedNeckBottom=100;for(size_t i=a.headMount.begin;i<a.headMount.end;++i)for(auto p:detached.panels[i].point)detachedNeckBottom=std::min(detachedNeckBottom,p.y);
    for(size_t i=a.mask.begin;i<a.mask.end;++i)for(auto& p:detached.panels[i].point)p.z+=.35f;
    float detachedMaskRear=100;for(size_t i=a.mask.begin;i<a.mask.end;++i)for(auto p:detached.panels[i].point)detachedMaskRear=std::min(detachedMaskRear,p.z);
    assert(detachedNeckBottom>2.14f&&detachedMaskRear>headMount.second.z);std::cout<<"sazabi_support_negatives_detected=2\n";
    unsigned funnelPairs=0,funnelBody=0;for(size_t i=0;i<a.funnels.size();++i){
        funnelBody+=strictContacts(*m,a.funnels[i],{{a.funnels[i],a.containers[i/3]}});
        for(size_t j=i+1;j<a.funnels.size();++j)funnelPairs+=between(*m,a.funnels[i],a.funnels[j]);
    }
    const unsigned rifle=strictContacts(*m,a.rifle,{{{a.rifle.begin,a.rifle.begin+6},a.palms[0]}});
    const unsigned shield=strictContacts(*m,a.shield,{{a.shield,a.shieldMount}});
    unsigned palms=0;for(auto r:a.palms)for(size_t i=r.begin;i<r.end;++i)for(size_t j=0;j<m->count;++j)if(m->parts[j]==Part::Arms)palms+=crosses(*m,i,j)>0;
    std::cout<<"sd_sazabi contacts rifle="<<rifle<<" shield="<<shield<<" funnel_pairs="<<funnelPairs<<" funnel_body="<<funnelBody<<" palm_armor="<<palms<<'\n';
    if(rifle)reportRange(*m,a.rifle,{Part::Hands,Part::Rifle},"rifle");
    if(shield)reportRange(*m,a.shield,{Part::Arms,Part::Hands,Part::Shield,Part::Sabers},"shield");
    assert(rifle==0&&shield==0&&funnelPairs==0&&funnelBody==0&&palms==0);
    float floorArea[2]={},crown=0,chin=10,width=0,zmin=10,zmax=-10,shoulderMin=100,shoulderMax=-100;
    for(size_t i=0;i<m->count;++i){
        if(m->parts[i]==Part::Feet){auto p=m->panels[i].point;bool floor=true;for(auto q:p){assert(q.y>=.0249f);floor&=std::abs(q.y-.025f)<.0001f;}if(floor)for(int t=0;t<2;++t){auto n=cross(subtract(p[t+1],p[0]),subtract(p[t+2],p[0]));floorArea[p[0].x>0]+=.5f*std::sqrt(dot(n,n));}}
        if(m->parts[i]==Part::Shoulders)for(auto p:m->panels[i].point){shoulderMin=std::min(shoulderMin,p.x);shoulderMax=std::max(shoulderMax,p.x);}
        if(m->parts[i]==Part::Head&&!(i>=a.headMount.begin&&i<a.headMount.end))for(auto p:m->panels[i].point)if(p.y<3.05f){crown=std::max(crown,p.y);chin=std::min(chin,p.y);width=std::max(width,std::abs(p.x)*2);zmin=std::min(zmin,p.z);zmax=std::max(zmax,p.z);}
    }
    const float ratio=(crown-.025f)/(crown-chin),depth=(zmax-zmin)/width;
    std::cout<<"sd_sazabi panels="<<m->count<<" helmet_body_ratio="<<ratio<<" depth_width="<<depth<<" shoulder_span="<<shoulderMax-shoulderMin<<" sole_area="<<floorArea[0]<<','<<floorArea[1]<<'\n';
    assert(ratio>2.8f&&ratio<3.35f&&depth>.75f&&depth<1.2f&&shoulderMax-shoulderMin>2.2f&&floorArea[0]>.38f&&floorArea[1]>.38f);
    // Controlled equipment error: move the complete rifle into the chest.
    for(size_t i=a.rifle.begin;i<a.rifle.end;++i)for(auto& p:m->panels[i].point){p.x+=.72f;p.y+=.45f;p.z-=.40f;}
    const unsigned badRifle=strictContacts(*m,a.rifle,{{{a.rifle.begin,a.rifle.begin+6},a.palms[0]}});assert(badRifle>0);
    buildSazabi(*m,{},SazabiStage::Final,&a);
    const auto shifted=a.funnels[0];for(size_t i=shifted.begin;i<shifted.end;++i)for(auto& p:m->panels[i].point){p.x+=.02f;p.y+=.16f;p.z-=.07f;}
    const unsigned badFunnels=between(*m,a.funnels[0],a.funnels[1]);
    std::cout<<"sd_sazabi negative rifle="<<badRifle<<" funnel_overlap="<<badFunnels<<'\n';assert(badFunnels>0);
    buildSazabi(*m,{},SazabiStage::Final,&a);sd_curved_check::crownAndNegative(*m);
}
} // namespace sd_sazabi_check
