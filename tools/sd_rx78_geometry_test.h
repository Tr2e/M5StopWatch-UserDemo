#pragma once
#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

// Independent checks of the delivered mesh, not the builder's parameters.
// Intersect head triangles with front-facing probe rays to test face depth.
inline void checkSdRx78Geometry(const gundam_museum::Mesh& m){
    using namespace gundam_museum;
    float headBack=100,headFront=-100;
    float low[2]={100,100},footArea[2]={0,0},headHalf=0,shoulderHalf=0,headLow=100,headTop=-100,sole=100;
    float tipX[2]={-100,-100};
    for(size_t i=0;i<m.count;++i){
        const auto& panel=m.panels[i];
        for(auto p:panel.point){
            if(m.parts[i]==Part::Feet){const int side=p.x>0;low[side]=std::min(low[side],p.y);sole=std::min(sole,p.y);}
            if(m.parts[i]==Part::Shoulders)shoulderHalf=std::max(shoulderHalf,std::abs(p.x));
            if(m.parts[i]!=Part::Head)continue;
            headLow=std::min(headLow,p.y);
            headBack=std::min(headBack,p.z);headFront=std::max(headFront,p.z);
            if(std::abs(p.x)<.20f)headTop=std::max(headTop,p.y); // exclude the V-fin tips
            if(p.y<2.45f)headHalf=std::max(headHalf,std::abs(p.x));
            const int side=p.x>0;const float x=std::abs(p.x);
            if(x>tipX[side]){tipX[side]=x;}
        }
    }
    // SD bounds are photographic estimates from the model card, not a similarity score.
    const float heads=(headTop-sole)/(headTop-headLow),shoulderRatio=headHalf/shoulderHalf;
    // Physical mesh sanity only; projected photo ratios are checked separately.
    assert(heads>2.5f && heads<2.8f && shoulderRatio>.46f && shoulderRatio<.51f);
    assert(std::abs(low[0]-low[1])<1e-5f);
    int tipFaces[2]={0,0};
    float tipZLow[2]={100,100},tipZHigh[2]={-100,-100},tipYLow[2]={100,100},tipYHigh[2]={-100,-100};
    for(size_t i=0;i<m.count;++i){
        auto& a=m.panels[i].point;
        if(m.parts[i]==Part::Feet){
            const int side=a[0].x>0;bool onFloor=true;
            for(auto p:a)onFloor &= std::abs(p.y-low[side])<1e-5f;
            if(onFloor){auto n=cross(subtract(a[1],a[0]),subtract(a[2],a[0]));footArea[side]+=std::sqrt(dot(n,n))*.5f;}
        }
        if(m.parts[i]!=Part::Head)continue;
        for(int side=0;side<2;++side){bool contains=false;
            for(auto p:a)if(std::abs(p.x)>tipX[side]-.025f && (p.x>0)==bool(side)){
                tipZLow[side]=std::min(tipZLow[side],p.z);tipZHigh[side]=std::max(tipZHigh[side],p.z);
                tipYLow[side]=std::min(tipYLow[side],p.y);tipYHigh[side]=std::max(tipYHigh[side],p.y);contains=true;
            }
            tipFaces[side]+=contains;
        }
    }
    assert(footArea[0]>.10f && footArea[1]>.10f); // A broad contact patch, not one toe vertex.
    for(int side=0;side<2;++side){
        assert(tipFaces[side]>=4 && tipZHigh[side]-tipZLow[side]>.015f);
        assert(tipYHigh[side]-tipYLow[side]>.01f && tipYHigh[side]-tipYLow[side]<.08f);
    } // User photo: small blunt end, not a needle; thin relative to blade length.
    // Cut actual head triangles at two outer blade stations, beyond the helmet.
    // A constant-width or constant-thickness strip must fail this check.
    const auto bladeCut=[&](float x){
        float ymin=100,ymax=-100,zmin=100,zmax=-100;
        for(size_t i=0;i<m.count;++i){if(m.parts[i]!=Part::Head)continue;
            const auto& a=m.panels[i].point;
            for(int t=0;t<2;++t){Point tri[]={a[0],a[t+1],a[t+2]};
                for(int e=0;e<3;++e){auto p=tri[e],q=tri[(e+1)%3];
                    if(std::abs(q.x-p.x)<1e-7f || x<std::min(p.x,q.x) || x>std::max(p.x,q.x))continue;
                    const float u=(x-p.x)/(q.x-p.x),y=p.y+u*(q.y-p.y),z=p.z+u*(q.z-p.z);
                    ymin=std::min(ymin,y);ymax=std::max(ymax,y);zmin=std::min(zmin,z);zmax=std::max(zmax,z);
                }
            }
        }return Point{ymax-ymin,zmax-zmin,0};
    };
    const auto middle=bladeCut(.54f),outer=bladeCut(.78f);
    assert(middle.x>0 && outer.x/middle.x<.65f && outer.x/middle.x>.2f);
    assert(middle.y>0 && outer.y/middle.y<.75f);
    const float depthWidth=(headFront-headBack)/(headHalf*2);
    assert(depthWidth>1.08f && depthWidth<1.35f); // oblique-reference volume guard, not a scanned dimension
    std::cout<<"sd_volume depth_width="<<depthWidth<<" blade_width_ratio="<<outer.x/middle.x
        <<" blade_depth_ratio="<<outer.y/middle.y<<'\n';
    const auto surface=[&](float x,float y){float z=-100;
        for(size_t i=0;i<m.count;++i){if(m.parts[i]!=Part::Head)continue;auto& a=m.panels[i].point;
            for(int tri=0;tri<2;++tri){Point p=a[0],q=a[tri+1],r=a[tri+2];
                const float den=(q.y-r.y)*(p.x-r.x)+(r.x-q.x)*(p.y-r.y);
                if(std::abs(den)<1e-8f)continue;
                const float u=((q.y-r.y)*(x-r.x)+(r.x-q.x)*(y-r.y))/den;
                const float v=((r.y-p.y)*(x-r.x)+(p.x-r.x)*(y-r.y))/den;
                if(u>=0 && v>=0 && u+v<=1)z=std::max(z,u*p.z+v*q.z+(1-u-v)*r.z);
            }
        }return z;
    };
    float maskFront=-100,browFront=-100;
    for(float x:{-.08f,.0f,.08f}){
        for(float dy:{.20f,.25f,.30f})maskFront=std::max(maskFront,surface(x,headLow+dy));
        for(float dy:{.46f,.49f,.52f})browFront=std::max(browFront,surface(x,headLow+dy));
    }
    assert(maskFront>0 && browFront-maskFront>.025f);
    std::cout<<"sd_geometry heads="<<heads<<" head_shoulder="<<shoulderRatio
        <<" sole_y="<<low[0]<<','<<low[1]<<" contact_area="<<footArea[0]<<','<<footArea[1]
        <<" mask_z="<<maskFront<<" brow_z="<<browFront<<" tip_faces="<<tipFaces[0]<<','<<tipFaces[1]<<'\n';
}
