#pragma once
#include "../main/apps/app_gundam_museum/model/strike_gundam.h"
#include "sd_rx78_equipment_test.h"
#include "sd_eye_socket_test.h"
#include "sd_skirt_assembly_test.h"
#include <limits>
#include <memory>

namespace sd_strike_check {
using namespace gundam_museum;
inline bool permitted(Part a,Part b,Point p){
    // Only physical peg insertions, expressed in this model's own coordinates.
    return (a==Part::Shield && b==Part::Arms && p.x>.965f && p.x<.975f &&
        p.y>1.399f && p.y<1.478f && p.z>.01f && p.z<.115f) ||
        (a==Part::Aile && b==Part::Backpack && std::abs(p.x)<.049f &&
        p.y>1.80f && p.y<1.88f && p.z>-.402f && p.z<-.366f) ||
        (a==Part::Aile && b==Part::Sabers && std::abs(p.x)>.20f && std::abs(p.x)<.27f &&
        p.y>2.019f && p.y<2.151f && p.z>-.72f && p.z<-.64f) ||
        // D7 wing roots overlap only the side wall of the center housing.
        (a==Part::Aile && b==Part::Aile && std::abs(p.x)>=.249f && std::abs(p.x)<.273f &&
        p.y>2.069f && p.y<2.107f && p.z>-.853f && p.z<-.518f);
}
inline unsigned pairCrossings(const Mesh& m,size_t i,size_t j,bool& allowed){
    unsigned hits=0;auto a=m.panels[i].point,b=m.panels[j].point;
    for(int ta=0;ta<2;++ta)for(int tb=0;tb<2;++tb){
        Point x[]={a[0],a[ta+1],a[ta+2]},y[]={b[0],b[tb+1],b[tb+2]};
        for(int e=0;e<3;++e)for(int dir=0;dir<2;++dir){Point p;
            if(dir?sd_equipment_check::crossing(y[e],y[(e+1)%3],x[0],x[1],x[2],p):
                sd_equipment_check::crossing(x[e],x[(e+1)%3],y[0],y[1],y[2],p)){
                ++hits;allowed &= permitted(m.parts[i],m.parts[j],p);
            }
        }
    }return hits;
}
struct Contacts {unsigned unintended=0,shieldPeg=0,ailePeg=0,saberPeg=0,unitPairs=0,wingRoots=0,palmArmor=0;};
inline Contacts inspect(const Mesh& m,const StrikeAssembly& assembly){
    Contacts r;
    for(size_t i=0;i<m.count;++i){if(m.parts[i]<Part::Rifle)continue;
        for(size_t j=0;j<m.count;++j){
            if(m.parts[i]==m.parts[j] || (m.parts[j]>=Part::Rifle && j<i))continue;
            bool allowed=true;if(!pairCrossings(m,i,j,allowed))continue;
            if(!allowed)++r.unintended;else if(m.parts[i]==Part::Shield)++r.shieldPeg;else if(m.parts[j]==Part::Sabers)++r.saberPeg;else ++r.ailePeg;
        }
    }
    for(size_t a=0;a<5;++a)for(size_t b=a+1;b<5;++b)
        for(size_t i=assembly.aile[a].begin;i<assembly.aile[a].end;++i)
            for(size_t j=assembly.aile[b].begin;j<assembly.aile[b].end;++j){bool allowed=true;
                if(pairCrossings(m,i,j,allowed)){if(allowed && a==0 && b<=2)++r.wingRoots;else ++r.unitPairs;}
            }
    for(auto palm:assembly.palms)for(size_t i=palm.begin;i<palm.end;++i)
        for(size_t j=0;j<m.count;++j)if(m.parts[j]==Part::Arms){bool allowed=false;
            if(pairCrossings(m,i,j,allowed))++r.palmArmor;
        }
    return r;
}

// Symmetry uses geometry, not RGB equality: directional lighting is asymmetric.
inline float ventMirrorError(const Mesh& m,const StrikeAssembly& assembly){
    float error=0;
    for(int side=0;side<2;++side){auto a=assembly.vents[side],b=assembly.vents[1-side];
        assert(a.end>a.begin && a.end-a.begin==b.end-b.begin);
        for(size_t i=a.begin;i<a.end;++i)for(auto p:m.panels[i].point){float nearest=100;
            for(size_t j=b.begin;j<b.end;++j)for(auto q:m.panels[j].point){
                auto d=subtract(Point{-p.x,p.y,p.z},q);nearest=std::min(nearest,std::sqrt(dot(d,d)));
            }error=std::max(error,nearest);
        }
    }return error;
}
inline void check(const Mesh& production){
    auto m=std::make_unique<Mesh>();StrikeAssembly assembly;buildStrikeGundam(*m,{},StrikeStage::Final,&assembly);
    assert(m->count==production.count && !m->overflowed);
    sd_skirt_check::check(*m,assembly.skirts,true,"strike");
    sd_eye_check::check(*m,assembly.eyes,.51f,.32f);
    const float mirrorError=ventMirrorError(*m,assembly);
    std::cout<<"sd_strike chest_vent_mirror_error="<<mirrorError<<'\n';assert(mirrorError<1e-5f);
    // Deliberately lift one side; the symmetry gate must reject this defect.
    for(size_t i=assembly.vents[1].begin;i<assembly.vents[1].end;++i)
        for(auto& p:m->panels[i].point)p.y+=.035f;
    const float badMirror=ventMirrorError(*m,assembly);assert(badMirror>.03f);
    std::cout<<"sd_strike negative vent_height_error="<<badMirror<<'\n';
    buildStrikeGundam(*m,{},StrikeStage::Final,&assembly);
    auto result=inspect(*m,assembly);
    std::cout<<"sd_strike contacts unintended="<<result.unintended<<" shield_peg="<<result.shieldPeg
        <<" aile_peg="<<result.ailePeg<<" inter_aile="<<result.unitPairs<<" palm_armor="<<result.palmArmor<<" wing_roots="<<result.wingRoots<<" saber_peg="<<result.saberPeg<<'\n';
    assert(result.unintended==0 && result.unitPairs==0 && result.palmArmor==0);
    assert(result.shieldPeg>0 && result.shieldPeg<=16 && result.ailePeg>0 && result.ailePeg<=12 && result.saberPeg>0 && result.saberPeg<=48 && result.wingRoots>0 && result.wingRoots<=48);
    // Controlled negative: shift the complete rifle toward the forearm.
    // This intentionally invalidates the correct grip frame and must be rejected.
    for(size_t i=0;i<m->count;++i)if(m->parts[i]==Part::Rifle)
        for(auto& p:m->panels[i].point){p.z-=.15f;}
    const auto bad=inspect(*m,assembly);
    std::cout<<"sd_strike negative rifle_offset_unintended="<<bad.unintended<<'\n';assert(bad.unintended>0);
    buildStrikeGundam(*m,{},StrikeStage::Final,&assembly);
    float rifleMin=100;
    for(size_t i=0;i<m->count;++i)if(m->parts[i]==Part::Rifle)for(auto p:m->panels[i].point)rifleMin=std::min(rifleMin,p.y);
    std::cout<<"sd_strike rifle_floor="<<rifleMin<<'\n';assert(rifleMin>.025f);
    float crown=0,chin=10,headWidth=0,zmin=10,zmax=-10;
    unsigned soleVertices[2]={},outerTips[2]={},innerTips[2]={};
    for(size_t i=0;i<m->count;++i){
        if(m->parts[i]==Part::Feet)for(auto p:m->panels[i].point){assert(p.y>=.0249f);if(std::abs(p.y-.025f)<.0001f)++soleVertices[p.x>0];}
        if(m->parts[i]!=Part::Head)continue;
        for(auto p:m->panels[i].point){
            if(p.y<3.16f && std::abs(p.x)<.59f){crown=std::max(crown,p.y);chin=std::min(chin,p.y);headWidth=std::max(headWidth,std::abs(p.x)*2);zmin=std::min(zmin,p.z);zmax=std::max(zmax,p.z);}
            if(p.y>3.44f){if(std::abs(p.x)>.85f)++outerTips[p.x>0];else if(std::abs(p.x)>.25f&&std::abs(p.x)<.36f)++innerTips[p.x>0];}
        }
    }
    const float ratio=(crown-.025f)/(crown-chin),depth=(zmax-zmin)/headWidth;
    std::cout<<"sd_strike helmet_body_ratio="<<ratio<<" depth_width="<<depth<<'\n';
    assert(ratio>2.7f && ratio<3.0f && depth>.85f && depth<1.15f);
    for(int side=0;side<2;++side)assert(soleVertices[side]>=12 && outerTips[side]>=4 && innerTips[side]>=4);
    float soleArea[2]={};
    for(size_t i=0;i<m->count;++i)if(m->parts[i]==Part::Feet){auto a=m->panels[i].point;
        bool floor=true;for(auto p:a)floor &= std::abs(p.y-.025f)<.0001f;
        if(floor)for(int t=0;t<2;++t){auto n=cross(subtract(a[t+1],a[0]),subtract(a[t+2],a[0]));soleArea[a[0].x>0]+=.5f*std::sqrt(dot(n,n));}
    }
    assert(soleArea[0]>.30f && soleArea[1]>.30f);
    const auto surface=[&](float x,float y){float z=-100;
        for(size_t i=0;i<m->count;++i){if(m->parts[i]!=Part::Head)continue;auto a=m->panels[i].point;
            for(int t=0;t<2;++t){Point p=a[0],q=a[t+1],r=a[t+2];
                float den=(q.y-r.y)*(p.x-r.x)+(r.x-q.x)*(p.y-r.y);if(std::abs(den)<1e-8f)continue;
                float u=((q.y-r.y)*(x-r.x)+(r.x-q.x)*(y-r.y))/den;
                float v=((r.y-p.y)*(x-r.x)+(p.x-r.x)*(y-r.y))/den;
                if(u>=0&&v>=0&&u+v<=1)z=std::max(z,u*p.z+v*q.z+(1-u-v)*r.z);
            }
        }return z;
    };
    float mask=-100,brow=-100;
    for(float x:{-.06f,0.f,.06f}){
        for(float y:{2.22f,2.27f,2.32f})mask=std::max(mask,surface(x,y));
        for(float y:{2.51f,2.55f,2.58f})brow=std::max(brow,surface(x,y));
    }
    assert(mask>0 && brow-mask>.03f);
    const auto cut=[&](float x){float yl=100,yh=-100,zl=100,zh=-100;
        for(size_t i=0;i<m->count;++i){if(m->parts[i]!=Part::Head)continue;auto a=m->panels[i].point;
            for(int t=0;t<2;++t){Point tri[]={a[0],a[t+1],a[t+2]};for(int e=0;e<3;++e){auto p=tri[e],q=tri[(e+1)%3];
                if(std::abs(q.x-p.x)<1e-7f || x<std::min(p.x,q.x) || x>std::max(p.x,q.x))continue;
                float u=(x-p.x)/(q.x-p.x),y=p.y+u*(q.y-p.y),z=p.z+u*(q.z-p.z);
                yl=std::min(yl,y);yh=std::max(yh,y);zl=std::min(zl,z);zh=std::max(zh,z);
            }}
        }return Point{yh-yl,zh-zl,0};
    };
    const auto middle=cut(.65f),end=cut(.86f);
    assert(end.x/middle.x>.15f && end.x/middle.x<.6f && end.y/middle.y<.7f);
    std::cout<<"sd_strike face mask_z="<<mask<<" brow_z="<<brow<<" blade_width_ratio="<<end.x/middle.x
        <<" blade_depth_ratio="<<end.y/middle.y<<" sole_area="<<soleArea[0]<<','<<soleArea[1]<<'\n';
    for(const auto& range:assembly.aile)assert(range.end>range.begin+20);
    // A deliberately displaced pod must also fail same-Part unit checks.
    const auto pod=assembly.aile[3];
    for(size_t i=pod.begin;i<pod.end;++i)for(auto& p:m->panels[i].point)p.x+=.94f;
    const auto invalidPod=inspect(*m,assembly);assert(invalidPod.unitPairs>0);
    std::cout<<"sd_strike negative pod_overlap="<<invalidPod.unitPairs<<'\n';
}
} // namespace sd_strike_check
