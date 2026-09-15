#pragma once
#include "../main/apps/app_gundam_museum/model/char_zaku.h"
#include "sd_rx78_equipment_test.h"
#include "sd_curved_shape_test.h"
#include <limits>
#include <memory>

namespace sd_zaku_check {
using namespace gundam_museum;
inline unsigned crosses(const Mesh& m,size_t i,size_t j){
    unsigned hits=0;const auto a=m.panels[i].point,b=m.panels[j].point;
    for(int ta=0;ta<2;++ta)for(int tb=0;tb<2;++tb){Point x[]={a[0],a[ta+1],a[ta+2]},y[]={b[0],b[tb+1],b[tb+2]};
        for(int e=0;e<3;++e)for(int d=0;d<2;++d){Point p;hits+=d?sd_equipment_check::crossing(y[e],y[(e+1)%3],x[0],x[1],x[2],p):sd_equipment_check::crossing(x[e],x[(e+1)%3],y[0],y[1],y[2],p);}}
    return hits;
}
inline unsigned rangeAgainstParts(const Mesh& m,ZakuAssembly::Range r,std::initializer_list<Part> ignored){
    unsigned pairs=0;for(size_t i=r.begin;i<r.end;++i)for(size_t j=0;j<m.count;++j){if(j>=r.begin&&j<r.end)continue;bool skip=false;for(auto p:ignored){
        // Shoulder insertion is limited to the 8-sided connector (24 panels).
        skip|=m.parts[j]==p && (p!=Part::Shoulders || i<r.begin+24)
            && (p!=Part::Waist || (j>=r.begin-24 && j<r.begin && i<r.begin+24));
    }if(!skip&&crosses(m,i,j))++pairs;}return pairs;
}
inline void reportRange(const Mesh& m,ZakuAssembly::Range r,std::initializer_list<Part> ignored,const char* name){
    std::array<unsigned,static_cast<unsigned>(Part::Count)> counts{};for(size_t i=r.begin;i<r.end;++i)for(size_t j=0;j<m.count;++j){if(j>=r.begin&&j<r.end)continue;bool skip=false;for(auto p:ignored)skip|=m.parts[j]==p;if(!skip&&crosses(m,i,j))++counts[static_cast<unsigned>(m.parts[j])];}
    for(unsigned i=0;i<counts.size();++i)if(counts[i])std::cout<<name<<"_cross_part="<<i<<" pairs="<<counts[i]<<'\n';
}
inline unsigned rifleCrossings(const Mesh& m,const ZakuAssembly& assembly,bool report=false){
    Point handLo{100,100,100},handHi{-100,-100,-100};
    for(size_t i=assembly.palms[0].begin;i<assembly.palms[0].end;++i)for(auto p:m.panels[i].point){
        handLo={std::min(handLo.x,p.x),std::min(handLo.y,p.y),std::min(handLo.z,p.z)};
        handHi={std::max(handHi.x,p.x),std::max(handHi.y,p.y),std::max(handHi.z,p.z)};}
    unsigned bad=0;bool first=true;std::array<unsigned,static_cast<unsigned>(Part::Count)> byPart{};
    for(size_t i=0;i<m.count;++i)if(m.parts[i]==Part::Rifle)for(size_t j=0;j<m.count;++j){
        if(m.parts[j]==Part::Rifle || m.parts[j]>=Part::Shield)continue;
        const auto a=m.panels[i].point,b=m.panels[j].point;bool touched=false,allowed=true;
        for(int ta=0;ta<2;++ta)for(int tb=0;tb<2;++tb){Point x[]={a[0],a[ta+1],a[ta+2]},y[]={b[0],b[tb+1],b[tb+2]};
            for(int e=0;e<3;++e)for(int d=0;d<2;++d){Point p;if(d?sd_equipment_check::crossing(y[e],y[(e+1)%3],x[0],x[1],x[2],p):sd_equipment_check::crossing(x[e],x[(e+1)%3],y[0],y[1],y[2],p)){
                // Only the six-panel grip may enter the actual right-palm envelope.
                touched=true;const bool okay=i<assembly.rifle.begin+6 && j>=assembly.palms[0].begin && j<assembly.palms[0].end && p.x>=handLo.x-.001f && p.x<=handHi.x+.001f && p.y>=handLo.y-.001f && p.y<=handHi.y+.001f && p.z>=handLo.z-.001f && p.z<=handHi.z+.001f;
                if(report&&!okay&&first){std::cout<<" first_rifle_cross xyz="<<p.x<<','<<p.y<<','<<p.z<<" part="<<unsigned(m.parts[j])<<" rifle_panel="<<i-assembly.rifle.begin<<" body_panel="<<j<<" palm="<<assembly.palms[0].begin<<":"<<assembly.palms[0].end<<'\n';first=false;}allowed &= okay;
            }}
        }if(touched&&!allowed){++bad;++byPart[static_cast<unsigned>(m.parts[j])];}
    }if(report)for(unsigned i=0;i<byPart.size();++i)if(byPart[i])std::cout<<" rifle_cross_part="<<i<<" pairs="<<byPart[i]<<'\n';return bad;
}
inline void check(const Mesh& production){
    auto m=std::make_unique<Mesh>();ZakuAssembly a;buildCharZaku(*m,{},ZakuStage::Final,&a);
    assert(!m->overflowed && m->count==production.count);
    for(auto r:{a.rightShield,a.leftSpikes,a.headHose,a.waistHose,a.rifle,a.heatHawk,a.palms[0],a.palms[1]})assert(r.end>r.begin);
    auto bounds=[&](ZakuAssembly::Range r){Point lo{100,100,100},hi{-100,-100,-100};for(size_t i=r.begin;i<r.end;++i)for(auto p:m->panels[i].point){lo.x=std::min(lo.x,p.x);lo.y=std::min(lo.y,p.y);lo.z=std::min(lo.z,p.z);hi.x=std::max(hi.x,p.x);hi.y=std::max(hi.y,p.y);hi.z=std::max(hi.z,p.z);}return std::pair<Point,Point>{lo,hi};};
    const auto shield=bounds(a.rightShield),spikes=bounds(a.leftSpikes),headHose=bounds(a.headHose),waistHose=bounds(a.waistHose);
    assert(shield.second.x<0 && shield.second.y-shield.first.y>.85f);
    assert(spikes.first.x>0 && spikes.second.x>1.25f);
    // Official SDCS hoses wrap at muzzle/waist height and reach behind the shell.
    assert(headHose.first.y>2.19f && headHose.second.y>2.43f && headHose.first.z<-.4f);
    assert(waistHose.first.y>1.34f && waistHose.second.y>1.47f && waistHose.first.z<-.25f);
    float floorArea[2]={},crown=0,chin=10,width=0,zmin=10,zmax=-10,rifleFloor=10,antennaTop=0,antennaForward=-10,shieldArea=0,shoulderMin=100,shoulderMax=-100;
    for(size_t i=0;i<m->count;++i){
        if(m->parts[i]==Part::Feet){auto p=m->panels[i].point;bool floor=true;for(auto q:p){assert(q.y>=.0249f);floor&=std::abs(q.y-.025f)<.0001f;}if(floor)for(int t=0;t<2;++t){auto n=cross(subtract(p[t+1],p[0]),subtract(p[t+2],p[0]));floorArea[p[0].x>0]+=.5f*std::sqrt(dot(n,n));}}
        if(m->parts[i]==Part::Rifle)for(auto p:m->panels[i].point)rifleFloor=std::min(rifleFloor,p.y);
        if(m->parts[i]==Part::Head)for(auto p:m->panels[i].point){
            if(p.y<3.08f){crown=std::max(crown,p.y);chin=std::min(chin,p.y);width=std::max(width,std::abs(p.x)*2);zmin=std::min(zmin,p.z);zmax=std::max(zmax,p.z);}
            else{antennaTop=std::max(antennaTop,p.y);antennaForward=std::max(antennaForward,p.z);}
        }
        if(m->parts[i]==Part::Shoulders)for(auto p:m->panels[i].point){shoulderMin=std::min(shoulderMin,p.x);shoulderMax=std::max(shoulderMax,p.x);}
        if(m->parts[i]==Part::Shield){const auto p=m->panels[i].point;auto area=[&](Point a,Point b,Point c){auto n=cross(subtract(b,a),subtract(c,a));return .5f*std::sqrt(dot(n,n));};shieldArea=std::max(shieldArea,area(p[0],p[1],p[2])+area(p[0],p[2],p[3]));}
    }
    const float ratio=(crown-.025f)/(crown-chin),depth=(zmax-zmin)/width;
    std::cout<<"sd_zaku panels="<<m->count<<" helmet_body_ratio="<<ratio<<" depth_width="<<depth<<" shoulder_span="<<shoulderMax-shoulderMin<<" antenna_top="<<antennaTop<<" antenna_forward="<<antennaForward<<" shield_panel_area="<<shieldArea<<" sole_area="<<floorArea[0]<<','<<floorArea[1]<<" rifle_floor="<<rifleFloor<<'\n';
    // The helmet dome has no Gundam chin extension; its visible head interval is
    // shorter and therefore uses an independent SD range.
    assert(ratio>2.9f&&ratio<3.35f&&depth>.72f&&depth<1.2f&&shoulderMax-shoulderMin>2.0f&&antennaTop>3.25f&&antennaTop<3.35f&&antennaForward>.10f&&shieldArea>.5f&&floorArea[0]>.3f&&floorArea[1]>.3f&&rifleFloor>.025f);
    const unsigned correct=rifleCrossings(*m,a,true);std::cout<<"sd_zaku rifle_unintended="<<correct<<'\n';assert(correct==0);
    const unsigned shieldBody=rangeAgainstParts(*m,a.rightShield,{Part::Shoulders,Part::Shield});
    const unsigned hawkBody=rangeAgainstParts(*m,a.heatHawk,{Part::Waist,Part::Sabers});
    unsigned palms=0;for(auto r:a.palms)for(size_t i=r.begin;i<r.end;++i)for(size_t j=0;j<m->count;++j)if(m->parts[j]==Part::Arms)palms+=crosses(*m,i,j)>0;
    std::cout<<"sd_zaku contacts shield_body="<<shieldBody<<" heat_hawk_body="<<hawkBody<<" palm_armor="<<palms<<'\n';
    if(shieldBody)reportRange(*m,a.rightShield,{Part::Shoulders,Part::Shield},"shield");
    if(hawkBody)reportRange(*m,a.heatHawk,{Part::Waist,Part::Sabers},"heat_hawk");
    assert(shieldBody==0&&hawkBody==0&&palms==0);
    for(size_t i=0;i<m->count;++i)if(m->parts[i]==Part::Rifle)for(auto& p:m->panels[i].point){p.x+=.72f;p.y+=.45f;p.z-=.35f;}
    const unsigned negative=rifleCrossings(*m,a);std::cout<<"sd_zaku negative_rifle_unintended="<<negative<<'\n';assert(negative>0);
    buildCharZaku(*m,{},ZakuStage::Final,&a);
    for(size_t i=a.heatHawk.begin;i<a.heatHawk.end;++i)for(auto& p:m->panels[i].point){p.x-=.55f;p.y+=.35f;p.z+=.40f;}
    const unsigned negativeHawk=rangeAgainstParts(*m,a.heatHawk,{Part::Waist,Part::Sabers});
    buildCharZaku(*m,{},ZakuStage::Final,&a);
    for(size_t i=a.rightShield.begin;i<a.rightShield.end;++i)for(auto& p:m->panels[i].point){p.x+=.75f;p.z+=.30f;}
    const unsigned negativeShield=rangeAgainstParts(*m,a.rightShield,{Part::Shoulders,Part::Shield});
    std::cout<<"sd_zaku negative heat_hawk="<<negativeHawk<<" shoulder_shield="<<negativeShield<<'\n';
    assert(negativeHawk>0&&negativeShield>0);
    buildCharZaku(*m,{},ZakuStage::Final,&a);
    const Point drumAxis{.0303f,.7150f,.6984f};float axialLo=100,axialHi=-100;
    for(size_t i=a.drum.begin;i<a.drum.end;++i)for(auto p:m->panels[i].point){const float v=dot(p,drumAxis);axialLo=std::min(axialLo,v);axialHi=std::max(axialHi,v);}
    std::cout<<"zaku_top_drum_axial_thickness="<<axialHi-axialLo<<'\n';assert(axialHi-axialLo<.10f);
    sd_curved_check::crownAndNegative(*m);
}
} // namespace sd_zaku_check
