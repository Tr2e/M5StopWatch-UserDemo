#pragma once
#include "sd_model_builder.h"

namespace gundam_museum::sd_curved {
using sd_model::Builder;
using sd_model::Ring;

// Open angular loft. A visor and its surrounding helmet share the same oval
// sections instead of putting a flat black plate across a spherical shell.
inline void arc(Builder& b,std::initializer_list<Ring> rings,uint16_t color,
                float start,float end,int segments){
    const auto point=[&](Ring r,int i){const float a=start+(end-start)*i/segments;
        return Point{std::sin(a)*r.w,r.y,r.z+std::cos(a)*r.d};};
    for(auto it=rings.begin()+1;it!=rings.end();++it)for(int i=0;i<segments;++i){
        const auto lo=*(it-1),hi=*it;const auto p=point(lo,i),q=point(lo,i+1);
        b.face(p,q,point(hi,i+1),point(hi,i),color,{p.x+q.x,0,p.z+q.z-2*lo.z},true);
    }
}

// A single swept surface with shared rings; no overlapping capped cylinders
// at elbows. The path is authored in the local frame and sampled as Catmull-Rom.
inline void hose(Builder& b,std::initializer_list<Point> controls,float radius,
                 uint16_t color,int samples=24,int sides=8,bool corrugated=true){
    if(controls.size()<2 || radius<=0 || samples<2 || sides<3)return;
    const auto mix=[](Point a,Point c,float t){return Point{a.x+(c.x-a.x)*t,a.y+(c.y-a.y)*t,a.z+(c.z-a.z)*t};};
    const auto at=[&](int i){return controls.begin()[std::clamp(i,0,int(controls.size())-1)];};
    const auto center=[&](float u){const float v=std::clamp(u,0.f,1.f)*(controls.size()-1);const int k=std::min(int(v),int(controls.size())-2);const float t=v-k;
        const Point a=at(k-1),c=at(k),d=at(k+1),e=at(k+2);
        const auto axis=[&](float p,float q,float r,float s){return .5f*((2*q)+(-p+r)*t+(2*p-5*q+4*r-s)*t*t+(-p+3*q-3*r+s)*t*t*t);};
        return Point{axis(a.x,c.x,d.x,e.x),axis(a.y,c.y,d.y,e.y),axis(a.z,c.z,d.z,e.z)};};
    const auto ring=[&](int i,int j){const float u=float(i)/samples;const auto c=center(u);
        auto tangent=subtract(center(std::min(1.f,u+.001f)),center(std::max(0.f,u-.001f)));
        const float len=std::sqrt(dot(tangent,tangent));tangent=len>1e-8f?Point{tangent.x/len,tangent.y/len,tangent.z/len}:Point{1,0,0};
        auto x=cross(tangent,std::abs(tangent.y)<.9f?Point{0,1,0}:Point{1,0,0});const float xl=std::sqrt(dot(x,x));x={x.x/xl,x.y/xl,x.z/xl};const auto y=cross(tangent,x);
        const float angle=2*sd_model::pi*j/sides,r=radius*(corrugated&&i%3==0?.91f:1.f);
        return Point{c.x+r*(x.x*std::cos(angle)+y.x*std::sin(angle)),c.y+r*(x.y*std::cos(angle)+y.y*std::sin(angle)),c.z+r*(x.z*std::cos(angle)+y.z*std::sin(angle))};};
    for(int i=0;i<samples;++i)for(int j=0;j<sides;++j){const auto a=ring(i,j),c=ring(i,j+1),d=ring(i+1,j+1),e=ring(i+1,j);
        b.face(a,c,d,e,color,subtract(mix(a,e,.5f),center((i+.5f)/samples)),true);}
    for(int end:{0,samples})for(int j=0;j<sides;++j){const auto c=center(float(end)/samples),a=ring(end,j),d=ring(end,j+1);
        b.face(c,a,d,d,color,subtract(c,center(end? .999f:.001f)),true);}
}
} // namespace gundam_museum::sd_curved
